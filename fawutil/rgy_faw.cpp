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

template<bool upperhalf>
static uint8_t faw_read_half(uint16_t v) {
    uint8_t i = (upperhalf) ? (v & 0xff00) >> 8 : (v & 0xff);
    return i - 0x80;
}

template<bool ishalf, bool upperhalf>
void faw_read(uint8_t *dst, const uint8_t *src, const int outlen) {
    if (!ishalf) {
        memcpy(dst, src, outlen);
        return;
    }
    const uint16_t *srcPtr = (const uint16_t *)src;
    for (int i = 0; i < outlen; i++) {
        dst[i] = faw_read_half<upperhalf>(srcPtr[i]);
    }
}

static uint32_t faw_checksum_calc(const uint8_t *buf, const int len) {
    uint32_t _v4288 = 0;
    uint32_t _v48 = 0;
    for (int i = 0; i < len; i += 2) {
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

static inline int64_t memmem_c(const void *data_, const int64_t data_size, const void *target_, const int64_t target_size) {
    const uint8_t *data = (const uint8_t *)data_;
    for (int64_t i = 0; i <= data_size - target_size; i++) {
        if (memcmp(data + i, target_, target_size) == 0) {
            return i;
        }
    }
    return -1;
}

int RGYAACHeader::sampleRateIdxToRate(const uint32_t idx) {
    switch (idx) {
    case 0x00: return 96000;
    case 0x01: return 88200;
    case 0x02: return 64000;
    case 0x03: return 48000;
    case 0x04: return 44100;
    case 0x05: return 32000;
    case 0x06: return 24000;
    case 0x07: return 22050;
    case 0x08: return 16000;
    case 0x09: return 12000;
    case 0x0A: return 11025;
    case 0x0B: return 8000;
    case 0x0C: return 7350;
    default: return 0;
    }
}

void RGYAACHeader::parse(const uint8_t *buf) {
    id              = (buf[1] & 0x08) != 0;
    protection      = (buf[1] & 0x01) != 0;
    profile         = (buf[2] & 0xC0) >> 6;
    samplerate      = sampleRateIdxToRate((buf[2] & 0x3C) >> 2);
    private_bit     = (buf[2] & 0x02) >> 1;
    channel         = ((buf[2] & 0x01) << 2) | ((buf[3] & 0xC0) >> 6);
    original        = (buf[3] & 0x20) != 0;
    home            = (buf[3] & 0x10) != 0;
    copyright       = (buf[3] & 0x08) != 0;
    copyright_start = (buf[3] & 0x04) != 0;
    aac_frame_length     = ((buf[3] & 0x03) << 11) | (buf[4] << 3) | (buf[5] >> 5);
    adts_buffer_fullness = ((buf[5] & 0x1f) << 6) | (buf[6] >> 2);
    no_raw_data_blocks_in_frame = buf[6] & 0x03;
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

static const std::array<uint8_t, 8> fawstart1  = {
    0x72, 0xF8, 0x1F, 0x4E, 0x07, 0x01, 0x00, 0x00
};
static const std::array<uint8_t, 16> fawstart2 = {
    0x00, 0xF2, 0x00, 0x78, 0x00, 0x9F, 0x00, 0xCE,
    0x00, 0x87, 0x00, 0x81, 0x00, 0x80, 0x00, 0x80
};
static const std::array<uint8_t, 12> fawfin1 = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x45, 0x4E, 0x44, 0x00
};
static const std::array<uint8_t, 24> fawfin2 = {
    0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
    0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
    0x00, 0xC5, 0x00, 0xCE, 0x00, 0xC4, 0x00, 0x80
};

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
    bufferHalf1() {
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
        if ((ret0 = memmem_c(bufferIn.data(), bufferIn.size(), fawstart1.data(), fawstart1.size())) >= 0) {
            fawmode = RGYFAWMode::Full;
        } else if ((ret0 = memmem_c(bufferIn.data(), bufferIn.size(), fawstart2.data(), fawstart2.size())) >= 0) {
            fawmode = RGYFAWMode::Half;
            bufferHalf0.appendFAWHalf(false, bufferIn.data(), bufferIn.size());
            bufferIn.clear();
        } else {
            bufferHalf0.appendFAWHalf(false, bufferIn.data(), bufferIn.size());
            bufferHalf1.appendFAWHalf(true,  bufferIn.data(), bufferIn.size());
            if (   (ret0 = memmem_c(bufferHalf0.data(), bufferHalf0.size(), fawstart1.data(), fawstart1.size())) >= 0
                && (ret1 = memmem_c(bufferHalf1.data(), bufferHalf1.size(), fawstart1.data(), fawstart1.size())) >= 0) {
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
    int64_t posStart = memmem_c(input.data(), input.size(), fawstart1.data(), fawstart1.size());
    if (posStart < 0) {
        return 0;
    }
    input.parseAACHeader(input.data() + posStart + fawstart1.size());

    int64_t posFin = memmem_c(input.data() + posStart + fawstart1.size(), input.size() - posStart - fawstart1.size(), fawfin1.data(), fawfin1.size());
    if (posFin < 0) {
        return 0;
    }
    posFin += posStart + fawstart1.size(); // データの先頭からの位置に変更

    // pos_start から pos_fin までの間に、別のfawstart1がないか探索する
    while (posStart + fawstart1.size() < posFin) {
        auto ret = memmem_c(input.data() + posStart + fawstart1.size(), posFin - posStart - fawstart1.size(), fawstart1.data(), fawstart1.size());
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
    if (posStartSample + (AAC_BLOCK_SAMPLES / 2) < input.outputSamples()) {
        input.addOffset(posFin + fawfin1.size());
        return 1;
    }

    // 時刻ずれを無音データで補正
    while (input.outputSamples() + (AAC_BLOCK_SAMPLES/2) < posStartSample) {
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
