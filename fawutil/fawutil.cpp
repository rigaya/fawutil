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
#include <array>
#include <filesystem>
#include "rgy_osdep.h"
#include "rgy_tchar.h"
#include "rgy_faw.h"
#include "fawutil_version.h"
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <shellapi.h>
#endif

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
    return file && _tcscmp(file, _T("-")) == 0;
}

static std::unique_ptr<FILE, decltype(&fclose)> open_file(const tstring& filename, const bool input) {
    std::unique_ptr<FILE, decltype(&fclose)>fp(nullptr, fclose);
    FILE *fptr = nullptr;
    if (is_pipe(filename.c_str())) {
        fptr = input ? stdin : stdout;
#if defined(_WIN32) || defined(_WIN64)
        if (_setmode(_fileno(fptr), _O_BINARY) < 0) {
            _ftprintf(stderr, _T("failed to switch %s to binary mode.\n"), input ? _T("stdin") : _T("stdout"));
            return fp;
        }
#endif //#if defined(_WIN32) || defined(_WIN64)
    } else {
        if (_tfopen_s(&fptr, filename.c_str(), input ? _T("rb") : _T("wb")) != 0 || fptr == nullptr) {
            _ftprintf(stderr, _T("failed to open %s file: %s!\n"), input ? _T("input") : _T("output"), filename.c_str());
            return fp;
        }
    }
    return std::move(std::unique_ptr<FILE, decltype(&fclose)>(fptr, fclose));
}

static size_t write_buffer(std::unique_ptr<FILE, decltype(&fclose)>& fp, const tstring& filename, uint64_t& writeBytesTotal, const uint8_t *buf, size_t bufSize) {
    if (bufSize > 0) {
        if (!fp) {
            fp = open_file(filename, false);
        }
        auto written = fwrite(buf, 1, bufSize, fp.get());
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

static int run_decode(const RGYFAWMode fawmode, const tstring& input, const std::array<tstring, 2>& output) {
    auto fp_in = open_file(input, true);
    if (!fp_in) {
        return 1;
    }
    std::vector<std::unique_ptr<FILE, decltype(&fclose)>> fp_out;

    fp_out.push_back(open_file(output[0], false));
    if (!fp_out.back()) {
        return 1;
    }
    fp_out.push_back(std::unique_ptr<FILE, decltype(&fclose)>(nullptr, fclose));

    const bool use_pipe = is_pipe(input.c_str()) || is_pipe(output[0].c_str()) || is_pipe(output[1].c_str());

    std::vector<uint8_t> buffer((use_pipe) ? 8 * 1024 : 64 * 1024 * 1024);
    size_t readBytes = fread(buffer.data(), 1, buffer.size(), fp_in.get());
    uint64_t readBytesTotal = readBytes;
    uint64_t writeBytesTotal[2] = { 0, 0 };

    RGYFAWDecoderOutput out_buffer;
    RGYFAWDecoder decoder;
    const uint32_t wav_header_size = decoder.init(buffer.data());
    decoder.decode(out_buffer, buffer.data() + wav_header_size, readBytes - wav_header_size);
    for (int i = 0; i < 2; i++) {
        write_buffer(fp_out[i], output[i], writeBytesTotal[i], out_buffer[i].data(), out_buffer[i].size());
    }

    auto prev = std::chrono::system_clock::now();
    while ((readBytes = fread(buffer.data(), 1, buffer.size(), fp_in.get())) > 0) {
        readBytesTotal += readBytes;
        auto now = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - prev).count() > 500) {
            write_size(_T("Reading"), readBytesTotal, true);
            prev = now;
        }
        decoder.decode(out_buffer, buffer.data(), readBytes);
        for (int i = 0; i < 2; i++) {
            write_buffer(fp_out[i], output[i], writeBytesTotal[i], out_buffer[i].data(), out_buffer[i].size());
        }
    }
    decoder.fin(out_buffer);
    for (int i = 0; i < 2; i++) {
        write_buffer(fp_out[i], output[i], writeBytesTotal[i], out_buffer[i].data(), out_buffer[i].size());
    }
    _ftprintf(stderr, _T("\nFinished\n"));
    write_size(_T("read    "), readBytesTotal);
    for (int i = 0; i < 2; i++) {
        if (i == 0 || writeBytesTotal[i] > 0) {
            write_size(_T("written"), writeBytesTotal[i]);
        }
    }
    return 0;
}

struct FAWEncode {
    FILE *fpin;
    std::vector<uint8_t> buffer;
    std::vector<uint8_t> out_buffer;
    std::vector<uint8_t> out_tmp;
    RGYFAWEncoder encoder;
    uint64_t readBytesTotal;
    FAWEncode();
    void init(FILE *fp, const RGYWAVHeader& wavheader, const bool use_pipe, const RGYFAWMode fawmode, const int delay);
};

FAWEncode::FAWEncode() :
    fpin(nullptr),
    buffer(),
    out_buffer(),
    out_tmp(),
    encoder(),
    readBytesTotal(0) {

}

void FAWEncode::init(FILE *fp, const RGYWAVHeader& wavheader, const bool use_pipe, const RGYFAWMode fawmode, const int delay) {
    fpin = fp;
    buffer.resize((use_pipe) ? 8 * 1024 : 4 * 1024 * 1024);
    encoder.init(&wavheader, fawmode, delay);
    readBytesTotal = 0;
}

static int run_encode(const RGYFAWMode fawmode, const std::array<int, 2>& delay, const std::array<tstring, 2>& input, const tstring& output) {
    std::vector<std::unique_ptr<FILE, decltype(&fclose)>> fp_in;
    for (auto& in : input) {
        if (!in.empty()) {
            auto fp = open_file(in, true);
            if (!fp) {
                return 0;
            }
            fp_in.push_back(std::move(fp));
        }
    }

    auto fp_out = open_file(output, false);
    if (!fp_out) {
        return 1;
    }

    const bool use_pipe = is_pipe(input[0].c_str()) || is_pipe(input[1].c_str()) || is_pipe(output.c_str());

    uint64_t writeBytesTotal = 0;

    RGYWAVHeader wavheader = { 0 };
    wavheader.init(2, 48000, (fawmode == RGYFAWMode::Half) ? sizeof(char) : sizeof(short), 0);
    {
        std::vector<uint8_t> wavheaderBytes = wavheader.createHeader();
        write_buffer(fp_out, output, writeBytesTotal, wavheaderBytes.data(), wavheaderBytes.size());
        // 4byte 0 で埋める (FAWは必ずこうなっている模様)
        std::vector<uint8_t> zero4(4, 0);
        write_buffer(fp_out, output, writeBytesTotal, zero4.data(), zero4.size());
    }

    std::vector<FAWEncode> reader(fp_in.size());
    for (size_t ifile = 0; ifile < fp_in.size(); ifile++) {
        // FAW Mixの場合、wavheaderInputはwavheaderと異なる (elemsizeが異なる)
        RGYWAVHeader wavheaderInput = { 0 };
        wavheaderInput.init(2, 48000, (fawmode == RGYFAWMode::Full) ? sizeof(short) : sizeof(char), 0);
        reader[ifile].init(fp_in[ifile].get(), wavheaderInput, use_pipe, (fp_in.size() > 1) ? RGYFAWMode::Half : fawmode, delay[ifile]);
    }

    if (reader.size() == 2) { // FAW mix
        std::vector<uint8_t> outfawmix;

        auto prev = std::chrono::system_clock::now();
        for (;;) {
            bool bothEOF = true;
            for (auto& r : reader) {
                const size_t readBytes = fread(r.buffer.data(), 1, r.buffer.size(), r.fpin);
                r.readBytesTotal += readBytes;
                bothEOF &= readBytes == 0;
                if (readBytes > 0) {
                    r.encoder.encode(r.out_buffer, r.buffer.data(), readBytes);
                    // out_tmp のほうに移動
                    const auto tmpsize = r.out_tmp.size();
                    if (r.out_buffer.size() > 0) {
                        r.out_tmp.resize(tmpsize + r.out_buffer.size());
                        memcpy(r.out_tmp.data() + tmpsize, r.out_buffer.data(), r.out_buffer.size());
                        r.out_buffer.clear();
                    }
                }
            }
            if (bothEOF) { // 両ファイル最後まで読み取ったら抜ける
                break;
            }

            // 短いほうに合わせる
            const auto process_data = std::min(reader[0].out_tmp.size(), reader[1].out_tmp.size());
            // FAW mixで出力
            outfawmix.resize(process_data * sizeof(uint16_t));
            uint16_t *const ptr_mix = (uint16_t *)outfawmix.data();
            const uint8_t *out_tmp0 = reader[0].out_tmp.data();
            const uint8_t *out_tmp1 = reader[1].out_tmp.data();
            for (size_t i = 0; i < process_data; i++) {
                const uint8_t v0 = out_tmp0[i] - 128;
                const uint8_t v1 = out_tmp1[i] - 128;
                ptr_mix[i] = ((((uint16_t)v0) << 8) | (uint16_t)v1);
            }
            write_buffer(fp_out, output, writeBytesTotal, outfawmix.data(), outfawmix.size());

            // 出力した部分を削除
            for (auto& r : reader) {
                const auto remain_bytes = r.out_tmp.size() - process_data;
                if (remain_bytes > 0) {
                    memmove(r.out_tmp.data(), r.out_tmp.data() + process_data, remain_bytes);
                }
                r.out_tmp.resize(remain_bytes);
            }

            // 進捗表示
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - prev).count() > 500) {
                write_size(_T("Writing"), writeBytesTotal, true);
                prev = now;
            }
        }

        // 最後まで処理
        for (auto& r : reader) {
            r.encoder.fin(r.out_buffer);
        }
        // 最後まで出力するため、長いほうに合わせる
        const auto process_data = std::max(reader[0].out_tmp.size(), reader[1].out_tmp.size());
        for (auto& r : reader) {
            r.out_tmp.resize(process_data, 0); // 短いほうは0で埋める
        }
        // FAW mixで出力
        outfawmix.resize(process_data * sizeof(uint16_t));
        uint16_t *const ptr_mix = (uint16_t *)outfawmix.data();
        const uint8_t *out_tmp0 = reader[0].out_tmp.data();
        const uint8_t *out_tmp1 = reader[1].out_tmp.data();
        for (size_t i = 0; i < process_data; i++) {
            const uint8_t v0 = out_tmp0[i] - 128;
            const uint8_t v1 = out_tmp1[i] - 128;
            ptr_mix[i] = ((((uint16_t)v0) << 8) | (uint16_t)v1);
        }
        write_buffer(fp_out, output, writeBytesTotal, outfawmix.data(), outfawmix.size());
    } else {
        auto& r = reader[0];
        auto readBytes = fread(r.buffer.data(), 1, r.buffer.size(), r.fpin);
        r.encoder.encode(r.out_buffer, r.buffer.data(), readBytes);
        write_buffer(fp_out, output, writeBytesTotal, r.out_buffer.data(), r.out_buffer.size());

        auto prev = std::chrono::system_clock::now();
        while ((readBytes = fread(r.buffer.data(), 1, r.buffer.size(), r.fpin)) > 0) {
            r.readBytesTotal += readBytes;
            r.encoder.encode(r.out_buffer, r.buffer.data(), readBytes);
            write_buffer(fp_out, output, writeBytesTotal, r.out_buffer.data(), r.out_buffer.size());

            // 進捗表示
            auto now = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - prev).count() > 500) {
                write_size(_T("Writing"), writeBytesTotal, true);
                prev = now;
            }
        }
        // 最後まで処理
        r.encoder.fin(r.out_buffer);
        write_buffer(fp_out, output, writeBytesTotal, r.out_buffer.data(), r.out_buffer.size());
    }

    // wavヘッダの上書き
    wavheader.data_size = (decltype(wavheader.data_size))std::min<uint64_t>(writeBytesTotal - WAVE_HEADER_SIZE, std::numeric_limits<decltype(wavheader.data_size)>::max());
    std::vector<uint8_t> wavheaderBytes = wavheader.createHeader();
    _fseeki64(fp_out.get(), 0, SEEK_SET);
    fwrite(wavheaderBytes.data(), 1, wavheaderBytes.size(), fp_out.get());

    _ftprintf(stderr, _T("\nFinished\n"));
    for (auto& r : reader) {
        write_size(_T("read    "), r.readBytesTotal);
    }
    write_size(_T("written"), writeBytesTotal);
    return 0;
}

static int run(const int mode, const RGYFAWMode fawmode, const std::array<int,2>& delay, const std::array<tstring, 2>& input, const std::array<tstring,2>& output) {
    if (mode == FAW_DEC) {
        return run_decode(fawmode, input[0], output);
    } else {
        return run_encode(fawmode, delay, input, output[0]);
    }
}

static int get_delay_from_filename(const tstring& filename, int *pos_start = nullptr, int *pos_fin = nullptr) {
    if (pos_start) *pos_start = -1;
    if (pos_fin) *pos_fin = -1;
    for (;;) {
        const auto ptr = _tcsstr(filename.c_str(), _T("DELAY "));
        if (!ptr) break;

        int value = 0;
        if (_stscanf_s(ptr, _T("DELAY %d"), &value) == 1) {
            if (pos_start) *pos_start = (int)(ptr - filename.c_str());
            const auto qtr = _tcsstr(ptr, _T("ms"));
            if (pos_fin) *pos_fin = (qtr) ? (*pos_start + (int)(qtr - ptr + _tcslen(_T("ms")))) : -1;
            return value;
        }
    }
    return 0;
}

#if defined(_WIN32) || defined(_WIN64)
static bool check_locale_is_ja() {
    const WORD LangID_ja_JP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    return GetUserDefaultLangID() == LangID_ja_JP;
}
#endif //#if defined(_WIN32) || defined(_WIN64)

int _tmain(int argc, const TCHAR **argv) {
#if defined(_WIN32) || defined(_WIN64)
    if (check_locale_is_ja()) {
        _tsetlocale(LC_ALL, _T("Japanese"));
    }
#endif //#if defined(_WIN32) || defined(_WIN64)
    if (argc <= 1) {
        print_help();
        return 0;
    }
    int iargoffset = 1;
    int mode = FAW_ENC;
    RGYFAWMode fawmode = RGYFAWMode::Full;
    std::array<int, 2> delay = { 0, 0 };
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
                delay[0] = std::stoi(argv[i] + 2);
                delay[1] = delay[0];
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

    std::array<tstring, 2> input;
    input[0] = argv[iargoffset];
    const auto inputpath = std::filesystem::path(input[0]);
    if (_tcsicmp(inputpath.extension().c_str(), _T(".wav")) == 0) {
        mode = FAW_DEC;
    } else if (_tcsicmp(inputpath.extension().c_str(), _T(".aac")) == 0) {
        mode = FAW_ENC;
    }
    if (delay[0] == 0) {
        if (mode == FAW_ENC) {
            delay[0] = get_delay_from_filename(input[0]);
        }
    } else {
        if (mode == FAW_DEC) {
            _ftprintf(stderr, _T("delay not supported with wav -> aac mode.\n"));
            return 1;
        }
    }

    std::array<tstring, 2> output;
    if (argc >= iargoffset+2) {
        const TCHAR *argstr = argv[iargoffset + 1];
        const auto argfilepath = std::filesystem::path(argstr);
        if (mode == FAW_ENC) {
            if (_tcsicmp(argfilepath.extension().c_str(), _T(".aac")) == 0) {
                input[1] = argstr;
                if (delay[1] == 0) {
                    delay[1] = get_delay_from_filename(input[1]);
                }
                fawmode = RGYFAWMode::Mix;
                if (argc >= iargoffset + 3) {
                    output[0] = argv[iargoffset + 2];
                }
            } else {
                output[0] = argstr;
            }
        } else { //FAW_DEC
            output[0] = argstr;
        }
    }
    if (output[0].empty()) {
        std::vector<TCHAR> buffer(input[0].size() + 128, _T('\0'));
        std::vector<TCHAR> tmp(input[0].size(), _T('\0'));
        _tcscpy_s(buffer.data(), buffer.size(), input[0].c_str());
        if (mode == FAW_ENC) {
            int pos_start = -1, pos_fin = -1;
            const int delay = get_delay_from_filename(buffer.data(), &pos_start, &pos_fin);
            if (pos_fin > 0) {
                _tcscpy_s(tmp.data(), tmp.size(), buffer.data() + pos_fin);
            }
            if (pos_start > 0) {
                _tcscpy_s(buffer.data() + pos_start, buffer.size() - pos_start, _T("DELAY 0ms"));
                if (pos_fin > 0) {
                    _tcscat_s(buffer.data(), buffer.size(), tmp.data());
                }
            }
        }
        auto outputpath = std::filesystem::path(buffer.data()).parent_path();
        outputpath /= std::filesystem::path(buffer.data()).stem();
        outputpath += (mode == FAW_DEC) ? _T("_out.aac") : _T("_out.wav");
        output[0] = outputpath;
    }
    if (!is_pipe(output[0].c_str())) {
        const auto outputpath = std::filesystem::path(output[0]);
        auto output2path = outputpath.parent_path();
        output2path /= outputpath.stem();
        output2path += _T("_track2");
        output2path += outputpath.extension();
        output[1] = output2path;
    }

    auto str_input = [](const tstring& input, const int delay) {
        tstring str;
        if (input.empty()) return str;

        str = (is_pipe(input.c_str())) ? _T("stdin") : input;
        if (delay != 0) {
            str += _T(" (");
#if _UNICODE
            str += std::to_wstring(delay);
#else
            str += std::to_string(delay);
#endif
            str += _T(" ms)");
        }
        return str;
    };

    _ftprintf(stderr, _T("fawutil %s\n"), VER_STR_FILEVERSION_TCHAR);
    _ftprintf(stderr, _T("mode:   %s\n"), (mode == FAW_DEC) ? _T("wav -> aac") : _T("aac -> wav"));
    _ftprintf(stderr, _T("input:  %s%s%s\n"), str_input(input[0], delay[0]).c_str(), (input[1].length() > 0 ? _T("\n        ") :_T("")), str_input(input[1], delay[1]).c_str());
    _ftprintf(stderr, _T("output: %s\n"), is_pipe(output[0].c_str()) ? _T("stdout") : output[0].c_str());
    return run(mode, fawmode, delay, input, output);
}
