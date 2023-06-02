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

#include <vector>
#include <array>
#include "rgy_faw.h"
#include "rgy_simd.h"

int64_t rgy_memmem_fawstart1_c(const void *data_, const int64_t data_size) {
    return rgy_memmem_c(data_, data_size, fawstart1.data(), fawstart1.size());
}

decltype(rgy_memmem_fawstart1_c)* get_memmem_fawstart1_func() {
#if defined(_M_IX86) || defined(_M_X64) || defined(__x86_64)
    const auto simd = get_availableSIMD();
#if defined(_M_X64) || defined(__x86_64)
    if ((simd & RGY_SIMD::AVX512BW) == RGY_SIMD::AVX512BW) return rgy_memmem_fawstart1_avx512bw;
#endif
    if ((simd & RGY_SIMD::AVX2) == RGY_SIMD::AVX2) return rgy_memmem_fawstart1_avx2;
#endif
    return rgy_memmem_fawstart1_c;
}

template<bool upperhalf>
static uint8_t faw_read_half(const uint16_t v) {
    uint8_t i = (upperhalf) ? (v & 0xff00) >> 8 : (v & 0xff);
    return i - 0x80;
}

template<bool ishalf, bool upperhalf>
void faw_read(uint8_t *dst, const uint8_t *src, const uint64_t outlen) {
    if (!ishalf) {
        memcpy(dst, src, outlen);
        return;
    }
    const uint16_t *srcPtr = (const uint16_t *)src;
    for (uint64_t i = 0; i < outlen; i++) {
        dst[i] = faw_read_half<upperhalf>(srcPtr[i]);
    }
}

static uint32_t faw_checksum_calc(const uint8_t *buf, const uint64_t len) {
    uint32_t _v4288 = 0;
    uint32_t _v48 = 0;
    for (uint64_t i = 0; i < len; i += 2) {
        uint32_t _v132 = *(uint16_t *)(buf + i);
        _v4288 += _v132;
        _v48 ^= _v132;
    }
    uint32_t res = (_v4288 & 0xffff) | ((_v48 & 0xffff) << 16);
    return res;
}

static uint32_t faw_checksum_read(const uint8_t *buf) {
    uint32_t v;
    memcpy(&v, buf, sizeof(v));
    return v;
}

int RGYAACHeader::sampleRateIdxToRate(const uint32_t idx) {
    static const int samplerateList[] = {
        96000,
        88200,
        64000,
        48000,
        44100,
        32000,
        24000,
        22050,
        16000,
        12000,
        11025,
        8000,
        7350,
        0
    };
    return samplerateList[std::min<uint32_t>(idx, _countof(samplerateList)-1)];
}

void RGYAACHeader::parse(const uint8_t *buf) {
    const uint8_t buf1 = buf[1];
    const uint8_t buf2 = buf[2];
    const uint8_t buf3 = buf[3];
    const uint8_t buf4 = buf[4];
    const uint8_t buf5 = buf[5];
    const uint8_t buf6 = buf[6];
    id              = (buf1 & 0x08) != 0;
    protection      = (buf1 & 0x01) != 0;
    profile         = (buf2 & 0xC0) >> 6;
    samplerate      = sampleRateIdxToRate((buf2 & 0x3C) >> 2);
    private_bit     = (buf2 & 0x02) >> 1;
    channel         = ((buf2 & 0x01) << 2) | ((buf3 & 0xC0) >> 6);
    original        = (buf3 & 0x20) != 0;
    home            = (buf3 & 0x10) != 0;
    copyright       = (buf3 & 0x08) != 0;
    copyright_start = (buf3 & 0x04) != 0;
    aac_frame_length     = ((buf3 & 0x03) << 11) | (buf4 << 3) | (buf5 >> 5);
    adts_buffer_fullness = ((buf5 & 0x1f) << 6) | (buf6 >> 2);
    no_raw_data_blocks_in_frame = buf6 & 0x03;
}

RGYFAWBitstream::RGYFAWBitstream() :
    buffer(),
    bufferLength(0),
    bufferOffset(0),
    bytePerWholeSample(0),
    inputSamples(0),
    outSamples(0),
    aacHeader() {

}

RGYFAWBitstream::~RGYFAWBitstream() {};

void RGYFAWBitstream::setBytePerSample(const int val) {
    bytePerWholeSample = val;
}

void RGYFAWBitstream::parseAACHeader(const uint8_t *buffer) {
    aacHeader.parse(buffer);
}

int RGYFAWBitstream::aacChannels() const {
    return aacHeader.channel;
}
int RGYFAWBitstream::aacFrameSize() const {
    return aacHeader.aac_frame_length;
}

void RGYFAWBitstream::addOffset(int64_t offset) {
    bufferLength -= offset;
    if (bufferLength == 0) {
        bufferOffset = 0;
    } else {
        bufferOffset += offset;
    }
}

void RGYFAWBitstream::addOutputSamples(uint64_t samples) {
    outSamples += samples;
}


void RGYFAWBitstream::append(const uint8_t *input, const uint64_t inputLength) {
    if (buffer.size() < bufferLength + inputLength) {
        buffer.resize(std::max(bufferLength + inputLength, buffer.size() * 2));
        if (bufferLength == 0) {
            bufferOffset = 0;
        }
        if (bufferOffset > 0) {
            memmove(buffer.data(), buffer.data() + bufferOffset, bufferLength);
            bufferOffset = 0;
        }
    } else if (buffer.size() < bufferOffset + bufferLength + inputLength) {
        if (bufferLength == 0) {
            bufferOffset = 0;
        }
        if (bufferOffset > 0) {
            memmove(buffer.data(), buffer.data() + bufferOffset, bufferLength);
            bufferOffset = 0;
        }
    }
    if (input != nullptr) {
        memcpy(buffer.data() + bufferOffset + bufferLength, input, inputLength);
    }
    bufferLength += inputLength;
    inputSamples += inputLength / bytePerWholeSample;
}

void RGYFAWBitstream::appendFAWHalf(const bool upper, const uint8_t *input, const uint64_t inputLength) {
    const auto prevSize = bufferLength;
    append(nullptr, inputLength);
    if (upper) {
        faw_read<true, true>(data() + prevSize, input, inputLength / 2);
    } else {
        faw_read<true, false>(data() + prevSize, input, inputLength / 2);
    }
}

void RGYFAWBitstream::clear() {
    bufferLength = 0;
    bufferOffset = 0;
    inputSamples = 0;
    outSamples = 0;
}

static const std::array<uint8_t, 16> aac_silent0 = {
    0xFF, 0xF9, 0x4C, 0x00, 0x02, 0x1F, 0xFC, 0x21,
    0x00, 0x49, 0x90, 0x02, 0x19, 0x00, 0x23, 0x80
};
static const std::array<uint8_t, 13> aac_silent1 = {
    0xFF, 0xF9, 0x4C, 0x40, 0x01, 0xBF, 0xFC, 0x00,
    0xC8, 0x40, 0x80, 0x23, 0x80
};
static const std::array<uint8_t, 16> aac_silent2 = {
    0xFF, 0xF9, 0x4C, 0x80, 0x02, 0x1F, 0xFC, 0x21,
    0x00, 0x49, 0x90, 0x02, 0x19, 0x00, 0x23, 0x80
};
static const std::array<uint8_t, 33> aac_silent6 = {
    0xFF, 0xF9, 0x4D, 0x80, 0x04, 0x3F, 0xFC, 0x00,
    0xC8, 0x00, 0x80, 0x20, 0x84, 0x01, 0x26, 0x40,
    0x08, 0x64, 0x00, 0x82, 0x30, 0x04, 0x99, 0x00,
    0x21, 0x90, 0x02, 0x18, 0x32, 0x00, 0x20, 0x08,
    0xE0
};

RGYFAWDecoder::RGYFAWDecoder() :
    wavheader(),
    fawmode(RGYFAWMode::Unknown),
    bufferIn(),
    bufferHalf0(),
    bufferHalf1(),
    funcMemMem(get_memmem_func()),
    funcMemMemFAWStart1(get_memmem_fawstart1_func()) {
}
RGYFAWDecoder::~RGYFAWDecoder() {

}

void RGYFAWDecoder::setWavInfo() {
    bufferIn.setBytePerSample(wavheader.number_of_channels * wavheader.bits_per_sample / 8);
    if (wavheader.bits_per_sample > 8) {
        bufferHalf0.setBytePerSample(wavheader.number_of_channels * wavheader.bits_per_sample / 16);
        bufferHalf1.setBytePerSample(wavheader.number_of_channels * wavheader.bits_per_sample / 16);
    }
}

int RGYFAWDecoder::init(const uint8_t *data) {
    int headerSize = wavheader.parseHeader(data);
    setWavInfo();
    return headerSize;
}

int RGYFAWDecoder::init(const RGYWAVHeader *data) {
    wavheader = *data;
    setWavInfo();
    return 0;
}

int RGYFAWDecoder::decode(RGYFAWDecoderOutput& output, const uint8_t *input, const uint64_t inputLength) {
    for (auto& b : output) {
        b.clear();
    }

    bool inputDataAppended = false;

    // FAWの種類を判別
    if (fawmode == RGYFAWMode::Unknown) {
        bufferIn.append(input, inputLength);
        inputDataAppended = true;

        int64_t ret0 = 0, ret1 = 0;
        if ((ret0 = funcMemMemFAWStart1(bufferIn.data(), bufferIn.size())) >= 0) {
            fawmode = RGYFAWMode::Full;
        } else if ((ret0 = funcMemMem(bufferIn.data(), bufferIn.size(), fawstart2.data(), fawstart2.size())) >= 0) {
            fawmode = RGYFAWMode::Half;
            bufferHalf0.appendFAWHalf(false, bufferIn.data(), bufferIn.size());
            bufferIn.clear();
        } else {
            bufferHalf0.appendFAWHalf(false, bufferIn.data(), bufferIn.size());
            bufferHalf1.appendFAWHalf(true,  bufferIn.data(), bufferIn.size());
            if (   (ret0 = funcMemMemFAWStart1(bufferHalf0.data(), bufferHalf0.size())) >= 0
                && (ret1 = funcMemMemFAWStart1(bufferHalf1.data(), bufferHalf1.size())) >= 0) {
                fawmode = RGYFAWMode::Mix;
                bufferIn.clear();
            } else {
                bufferHalf0.clear();
                bufferHalf1.clear();
            }
        }
    }
    if (fawmode == RGYFAWMode::Unknown) {
        return -1;
    }
    if (!inputDataAppended) {
        if (fawmode == RGYFAWMode::Full) {
            bufferIn.append(input, inputLength);
        } else if (fawmode == RGYFAWMode::Half) {
            bufferHalf0.appendFAWHalf(false, bufferIn.data(), bufferIn.size());
            bufferIn.clear();
        } else if (fawmode == RGYFAWMode::Mix) {
            bufferHalf0.appendFAWHalf(false, bufferIn.data(), bufferIn.size());
            bufferHalf1.appendFAWHalf(true, bufferIn.data(),  bufferIn.size());
            bufferIn.clear();
        }
        inputDataAppended = true;
    }

    // デコード
    if (fawmode == RGYFAWMode::Full) {
        decode(output[0], bufferIn);
    } else if (fawmode == RGYFAWMode::Half) {
        decode(output[0], bufferHalf0);
    } else if (fawmode == RGYFAWMode::Mix) {
        decode(output[0], bufferHalf0);
        decode(output[1], bufferHalf1);
    }
    return 0;
}

int RGYFAWDecoder::decode(std::vector<uint8_t>& output, RGYFAWBitstream& input) {
    while (input.size() > 0) {
        auto ret = decodeBlock(output, input);
        if (ret == 0) {
            break;
        }
    }
    return 0;
}

int RGYFAWDecoder::decodeBlock(std::vector<uint8_t>& output, RGYFAWBitstream& input) {
    int64_t posStart = funcMemMemFAWStart1(input.data(), input.size());
    if (posStart < 0) {
        return 0;
    }
    input.parseAACHeader(input.data() + posStart + fawstart1.size());

    int64_t posFin = funcMemMem(input.data() + posStart + fawstart1.size(), input.size() - posStart - fawstart1.size(), fawfin1.data(), fawfin1.size());
    if (posFin < 0) {
        return 0;
    }
    posFin += posStart + fawstart1.size(); // データの先頭からの位置に変更

    // pos_start から pos_fin までの間に、別のfawstart1がないか探索する
    while (posStart + (int64_t)fawstart1.size() < posFin) {
        auto ret = funcMemMemFAWStart1(input.data() + posStart + fawstart1.size(), posFin - posStart - fawstart1.size());
        if (ret < 0) {
            break;
        }
        posStart += ret + fawstart1.size();
        input.parseAACHeader(input.data() + posStart + fawstart1.size());
    }

    const int64_t blockSize = posFin - posStart - fawstart1.size() - 4 /*checksum*/;
    const uint32_t checksumCalc = faw_checksum_calc(input.data() + posStart + fawstart1.size(), blockSize);
    const uint32_t checksumRead = faw_checksum_read(input.data() + posFin - 4);
    // checksumとフレーム長が一致しない場合、そのデータは破棄
    if (checksumCalc != checksumRead || blockSize != input.aacFrameSize()) {
        input.addOffset(posFin + fawfin1.size());
        return 1;
    }

    // pos_start -> sample start
    const int64_t posStartSample = input.inputSampleStart() + posStart / input.bytePerSample();
    //fprintf(stderr, "Found block: %lld\n", posStartSample);

    // 出力が先行していたらdrop
    if (posStartSample + (AAC_BLOCK_SAMPLES / 2) < (int64_t)input.outputSamples()) {
        input.addOffset(posFin + fawfin1.size());
        return 1;
    }

    // 時刻ずれを無音データで補正
    while ((int64_t)input.outputSamples() + (AAC_BLOCK_SAMPLES/2) < posStartSample) {
        //fprintf(stderr, "Insert silence: %lld: %lld -> %lld\n", posStartSample, input.outputSamples(), input.outputSamples() + AAC_BLOCK_SAMPLES);
        addSilent(output, input);
    }

    // ブロックを出力に追加
    const uint64_t orig_size = output.size();
    output.resize(orig_size + blockSize);
    memcpy(output.data() + orig_size, input.data() + posStart + fawstart1.size(), blockSize);
    //fprintf(stderr, "Set block: %lld: %lld -> %lld\n", posStartSample, input.outputSamples(), input.outputSamples() + AAC_BLOCK_SAMPLES);

    input.addOutputSamples(AAC_BLOCK_SAMPLES);
    input.addOffset(posFin + fawfin1.size());
    return 1;
}

void RGYFAWDecoder::addSilent(std::vector<uint8_t>& output, RGYFAWBitstream& input) {
    auto ptrSilent = aac_silent0.data();
    auto dataSize = aac_silent0.size();
    switch (input.aacChannels()) {
    case 0:
        break;
    case 1:
        ptrSilent = aac_silent1.data();
        dataSize = aac_silent1.size();
        break;
    case 6:
        ptrSilent = aac_silent6.data();
        dataSize = aac_silent6.size();
        break;
    case 2:
    default:
        ptrSilent = aac_silent2.data();
        dataSize = aac_silent2.size();
        break;
    }
    const auto orig_size = output.size();
    output.resize(orig_size + dataSize);
    memcpy(output.data() + orig_size, ptrSilent, dataSize);
    input.addOutputSamples(AAC_BLOCK_SAMPLES);
}

void RGYFAWDecoder::fin(RGYFAWDecoderOutput& output) {
    for (auto& b : output) {
        b.clear();
    }
    if (fawmode == RGYFAWMode::Full) {
        fin(output[0], bufferIn);
    } else if (fawmode == RGYFAWMode::Half) {
        fin(output[0], bufferHalf0);
    } else if (fawmode == RGYFAWMode::Mix) {
        fin(output[0], bufferHalf0);
        fin(output[1], bufferHalf1);
    }
}

void RGYFAWDecoder::fin(std::vector<uint8_t>& output, RGYFAWBitstream& input) {
    //fprintf(stderr, "Fin sample: %lld\n", input.inputSampleFin());
    while (input.outputSamples() + (AAC_BLOCK_SAMPLES / 2) < input.inputSampleFin()) {
        //fprintf(stderr, "Insert silence: %lld -> %lld\n", input.outputSamples(), input.outputSamples() + AAC_BLOCK_SAMPLES);
        addSilent(output, input);
    }
}
