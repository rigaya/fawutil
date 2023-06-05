// -----------------------------------------------------------------------------------------
// QSVEnc/NVEnc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2023 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// --------------------------------------------------------------------------------------------

#include <cstdint>
#include <chrono>
#include <filesystem>
#include "rgy_osdep.h"
#include "rgy_tchar.h"
#include "rgy_faw.h"
#include "fawutil_version.h"

enum {
    FAW_ENC,
    FAW_DEC,
};

static void print_help() {
    _ftprintf(stdout, _T("fawutil %s\n"), VER_STR_FILEVERSION_TCHAR);
    _ftprintf(stdout, _T("wav -> aac\n"));
    _ftprintf(stdout, _T("  fawutil [-D] input.wav [output.aac]\n"));
    _ftprintf(stdout, _T("\n"));
    _ftprintf(stdout, _T("aac -> wav\n"));
    _ftprintf(stdout, _T("  fawutil [-E] [-sn] [-dxxx] input.aac [output.wav]\n"));
    _ftprintf(stdout, _T("    n = 1 or 2(1:1/1 2:1/2)\n"));
    _ftprintf(stdout, _T("    xxx ... in ms\n"));
}

static bool is_pipe(const TCHAR *file) {
    return _tcscmp(file, _T("-")) == 0;
}

static size_t write_buffer(FILE **fp, const tstring& filename, uint64_t& writeBytesTotal, const uint8_t *buf, size_t bufSize) {
    if (bufSize > 0) {
        if (*fp == nullptr) {
            if (_tfopen_s(fp, filename.c_str(), _T("wb")) != 0 || *fp == nullptr) {
                _ftprintf(stderr, _T("failed to open output file!\n"));
                return 0;
            }
        }
        auto written = fwrite(buf, 1, bufSize, *fp);
        writeBytesTotal += written;
        return written;
    }
    return 0;
}

static void write_size(const TCHAR *mes, const uint64_t size, bool CR = false) {
    const TCHAR *unit[5] = { _T("B"), _T("KiB"), _T("MiB"), _T("GiB"), _T("TiB") };
    int selectunit = 0;
    for (int iu = 1; iu < 5; iu++) {
        if ((size >> (iu * 10)) == 0) {
            selectunit = iu - 1;
            break;
        }
    }
    _ftprintf(stderr, _T("%s %10.3f %s%s"), mes, (double)size / (double)(1 << (10 * selectunit)), unit[selectunit], (CR) ? _T("\r") : _T("\n"));
}

static int run(const int mode, const RGYFAWMode fawmode, const int delay, const TCHAR *input, const tstring output[2]) {
    FILE *fp_in = nullptr, *fp_out[2] = { nullptr, nullptr };
    if (is_pipe(input)) {
        fp_in = stdin;
    } else if (_tfopen_s(&fp_in, input, _T("rb")) != 0 || fp_in == nullptr) {
        _ftprintf(stderr, _T("failed to open input file!\n"));
        return 1;
    }
    if (is_pipe(output[0].c_str())) {
        fp_out[0] = stdout;
    } else if (_tfopen_s(&fp_out[0], output[0].c_str(), _T("wb")) != 0 || fp_out[0] == nullptr) {
        _ftprintf(stderr, _T("failed to open output file!\n"));
        fclose(fp_in);
        return 1;
    }

    std::vector<uint8_t> buffer(1 * 1024 * 1024);

    size_t readBytes = fread(buffer.data(), 1, buffer.size(), fp_in);
    uint64_t readBytesTotal = readBytes;
    uint64_t writeBytesTotal[2] = { 0, 0 };

    if (mode == FAW_DEC) {
        RGYFAWDecoderOutput out_buffer;
        RGYFAWDecoder decoder;
        const uint32_t wav_header_size = decoder.init(buffer.data());
        decoder.decode(out_buffer, buffer.data() + wav_header_size, readBytes - wav_header_size);
        for (int i = 0; i < 2; i++) {
            write_buffer(&fp_out[i], output[i], writeBytesTotal[i], out_buffer[i].data(), out_buffer[i].size());
        }

        auto prev = std::chrono::system_clock::now();
        while ((readBytes = fread(buffer.data(), 1, buffer.size(), fp_in)) > 0) {
            readBytesTotal += readBytes;
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - prev).count() > 500) {
                write_size(_T("Reading"), readBytesTotal, true);
                prev = now;
            }
            decoder.decode(out_buffer, buffer.data(), readBytes);
            for (int i = 0; i < 2; i++) {
                write_buffer(&fp_out[i], output[i], writeBytesTotal[i], out_buffer[i].data(), out_buffer[i].size());
            }
        }
        decoder.fin(out_buffer);
        for (int i = 0; i < 2; i++) {
            write_buffer(&fp_out[i], output[i], writeBytesTotal[i], out_buffer[i].data(), out_buffer[i].size());
        }
    } else {
        const int audio_channels = 2;
        const int elemsize = (fawmode == RGYFAWMode::Half) ? sizeof(char) : sizeof(short);
        const int wav_sample_size = audio_channels * elemsize;

        RGYWAVHeader wavheader = { 0 };
        wavheader.file_size = 0;
        wavheader.subchunk_size = 16;
        wavheader.audio_format = 1;
        wavheader.number_of_channels = 2;
        wavheader.sample_rate = 48000;
        wavheader.byte_rate = wavheader.sample_rate * audio_channels * elemsize;
        wavheader.block_align = wav_sample_size;
        wavheader.bits_per_sample = elemsize * 8;
        wavheader.data_size = 0;
        {
            std::vector<uint8_t> wavheaderBytes = wavheader.createHeader();
            write_buffer(&fp_out[0], output[0], writeBytesTotal[0], wavheaderBytes.data(), wavheaderBytes.size());
            // 4byte 0 で埋める (FAWは必ずこうなっている模様)
            std::vector<uint8_t> zero4(4, 0);
            write_buffer(&fp_out[0], output[0], writeBytesTotal[0], zero4.data(), zero4.size());
        }

        std::vector<uint8_t> out_buffer;
        RGYFAWEncoder encoder;
        encoder.init(&wavheader, fawmode, delay);
        encoder.encode(out_buffer, buffer.data(), readBytes);
        write_buffer(&fp_out[0], output[0], writeBytesTotal[0], out_buffer.data(), out_buffer.size());

        auto prev = std::chrono::system_clock::now();
        while ((readBytes = fread(buffer.data(), 1, buffer.size(), fp_in)) > 0) {
            readBytesTotal += readBytes;
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - prev).count() > 500) {
                write_size(_T("Reading"), readBytesTotal, true);
                prev = now;
            }
            encoder.encode(out_buffer, buffer.data(), readBytes);
            write_buffer(&fp_out[0], output[0], writeBytesTotal[0], out_buffer.data(), out_buffer.size());
        }
        encoder.fin(out_buffer);
        write_buffer(&fp_out[0], output[0], writeBytesTotal[0], out_buffer.data(), out_buffer.size());

        // wavヘッダの上書き
        wavheader.data_size = writeBytesTotal[0] - WAVE_HEADER_SIZE;
        std::vector<uint8_t> wavheaderBytes = wavheader.createHeader();
        _fseeki64(fp_out[0], 0, SEEK_SET);
        fwrite(wavheaderBytes.data(), 1, wavheaderBytes.size(), fp_out[0]);
    }
    _ftprintf(stderr, _T("\nFinished\n"));
    write_size(_T("read    "), readBytesTotal);
    for (int i = 0; i < 2; i++) {
        if (i== 0 || writeBytesTotal[i] > 0) {
            write_size(_T("written"), writeBytesTotal[i]);
        }
    }

    fclose(fp_in);
    for (int i = 0; i < 2; i++) {
        if (fp_out[i]) {
            fclose(fp_out[i]);
        }
    }
    return 0;
}

int _tmain(int argc, const TCHAR **argv) {
    if (argc <= 1) {
        print_help();
        return 0;
    }
    int iargoffset = 1;
    int mode = FAW_ENC;
    RGYFAWMode fawmode = RGYFAWMode::Full;
    int delay = 0;
    for (int i = 0; i < argc; i++) {
        if (_tcscmp(_T("-h"), argv[i]) == 0) {
            print_help();
            return 0;
        }
        if (_tcscmp(_T("-D"), argv[i]) == 0) {
            mode = FAW_DEC;
            iargoffset++;
        }
        if (_tcsncmp(_T("-d"), argv[i], 2) == 0) {
            try {
                delay = std::stoi(argv[i] + 2);
            } catch (...) {
                _ftprintf(stderr, _T("Invalid delay set.\n"));
                return 1;
            }
            iargoffset++;
        }
        if (_tcsncmp(_T("-s"), argv[i], 2) == 0) {
            try {
                int value = std::stoi(argv[i] + 2);
                if (value == 1) {
                    fawmode = RGYFAWMode::Full;
                } else if (value == 2) {
                    fawmode = RGYFAWMode::Half;
                }
            } catch (...) {
                _ftprintf(stderr, _T("Invalid faw mode set.\n"));
                return 1;
            }
            iargoffset++;
        }
    }
    const TCHAR *input = argv[iargoffset];
    const auto inputpath = std::filesystem::path(input);
    if (_tcsicmp(inputpath.extension().c_str(), _T(".wav")) == 0) {
        mode = FAW_DEC;
    } else if (_tcsicmp(inputpath.extension().c_str(), _T(".aac")) == 0) {
        mode = FAW_ENC;
    }
    if (delay == 0) {
        if (mode == FAW_ENC) {
            auto ptr = _tcsstr(input, _T("DELAY "));
            if (ptr) {
                int value = 0;
                if (_stscanf_s(ptr, _T("DELAY %d"), &value) == 1) {
                    delay = value;
                }
            }
        }
    } else {
        if (mode == FAW_DEC) {
            _ftprintf(stderr, _T("delay not supported with wav -> aac mode.\n"));
            return 1;
        }
    }

    tstring output[2];
    if (argc >= iargoffset+2) {
        output[0] = argv[iargoffset + 1];
    }
    if (output[0].empty()) {
        auto outputpath = inputpath.parent_path();
        outputpath /= inputpath.stem();
        outputpath += (mode == FAW_DEC) ? _T("_out.aac") : _T("_out.wav");
        output[0] = outputpath.c_str();
    }
    if (!is_pipe(output[0].c_str())) {
        const auto outputpath = std::filesystem::path(output[0]);
        auto output2path = outputpath.parent_path();
        output2path /= outputpath.stem();
        output2path += _T("_track2");
        output2path += outputpath.extension();
        output[1] = output2path;
    }

    _ftprintf(stderr, _T("fawutil %s\n"), VER_STR_FILEVERSION_TCHAR);
    _ftprintf(stderr, _T("mode:   %s\n"), (mode == FAW_DEC) ? _T("wav -> aac") : _T("aac -> wav"));
    _ftprintf(stderr, _T("input:  %s\n"), is_pipe(input) ? _T("stdin")  : input);
    _ftprintf(stderr, _T("output: %s\n"), is_pipe(output[0].c_str()) ? _T("stdout") : output[0].c_str());
    _ftprintf(stderr, _T("delay:  %d ms\n"), delay);
    return run(mode, fawmode, delay, input, output);
}
