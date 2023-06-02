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

#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include "rgy_faw.h"

int main(int argc, const char **argv) {
    const char *input = argv[1];
    const char *output = argv[2];

    uint8_t buffer[128 * 1024];
    FILE *fp_in = fopen(input, "rb");
    FILE *fp_out = fopen(output, "wb");

    size_t readBytes = fread(buffer, 1, sizeof(buffer), fp_in);

    RGYFAWDecoderOutput out_buffer;
    RGYFAWDecoder decoder;
    int header_size = decoder.init(buffer);
    decoder.decode(out_buffer, buffer + header_size, readBytes - header_size);
    if (out_buffer[0].size()) fwrite(out_buffer[0].data(), 1, out_buffer[0].size(), fp_out);

    while ((readBytes = fread(buffer, 1, sizeof(buffer), fp_in)) > 0) {
        decoder.decode(out_buffer, buffer, readBytes);
        if (out_buffer[0].size()) fwrite(out_buffer[0].data(), 1, out_buffer[0].size(), fp_out);
    }
    decoder.fin(out_buffer);
    if (out_buffer[0].size()) fwrite(out_buffer[0].data(), 1, out_buffer[0].size(), fp_out);

    fclose(fp_in);
    fclose(fp_out);
    return 0;
}
