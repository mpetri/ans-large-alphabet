// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include "util.hpp"

template <uint32_t i> uint8_t extract7bits(const uint32_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
}

template <uint32_t i> uint8_t extract7bitsmaskless(const uint32_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)));
}

void vbyte_encode_u32(std::vector<uint8_t>& out, uint32_t x)
{
    if (x < (1U << 7)) {
        out.push_back(static_cast<uint8_t>(x & 127));
    } else if (x < (1U << 14)) {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bitsmaskless<1>(x) & 127);
    } else if (x < (1U << 21)) {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bits<1>(x) | 128);
        out.push_back(extract7bitsmaskless<2>(x) & 127);
    } else if (x < (1U << 28)) {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bits<1>(x) | 128);
        out.push_back(extract7bits<2>(x) | 128);
        out.push_back(extract7bitsmaskless<3>(x) & 127);
    } else {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bits<1>(x) | 128);
        out.push_back(extract7bits<2>(x) | 128);
        out.push_back(extract7bits<3>(x) | 128);
        out.push_back(extract7bitsmaskless<4>(x) & 127);
    }
}

void vbyte_encode_u32(uint8_t*& out, uint32_t x)
{
    if (x < (1U << 7)) {
        *out++ = static_cast<uint8_t>(x & 127);
    } else if (x < (1U << 14)) {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bitsmaskless<1>(x) & 127;
    } else if (x < (1U << 21)) {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bits<1>(x) | 128;
        *out++ = extract7bitsmaskless<2>(x) & 127;
    } else if (x < (1U << 28)) {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bits<1>(x) | 128;
        *out++ = extract7bits<2>(x) | 128;
        *out++ = extract7bitsmaskless<3>(x) & 127;
    } else {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bits<1>(x) | 128;
        *out++ = extract7bits<2>(x) | 128;
        *out++ = extract7bits<3>(x) | 128;
        *out++ = extract7bitsmaskless<4>(x) & 127;
    }
}

uint32_t vbyte_decode_u32(const uint8_t*& input)
{
    uint32_t x = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t c = *input++;
        x += ((c & 127) << shift);
        if (!(c & 128)) {
            return x;
        }
        shift += 7;
    }
    return x;
}

size_t write_vbyte(FILE* f, uint32_t x)
{
    std::vector<uint8_t> buf;
    vbyte_encode_u32(buf, x);
    int ret = fwrite(buf.data(), 1, buf.size(), f);
    if (ret != (int)buf.size()) {
        quit("writing vbyte to file: %d != %d", ret, (int)buf.size());
    }
    return buf.size();
}

uint32_t read_vbyte(FILE* f)
{
    uint32_t x = 0;
    unsigned int shift = 0;
    uint8_t chr = 0;
    while (true) {
        int ret = fread(&chr, 1, 1, f);
        if (ret != 1) {
            quit("reading vbyte from file: %d != %d", ret, 1);
        }
        x += ((chr & 127) << shift);
        if (!(chr & 128)) {
            return x;
        }
        shift += 7;
    }
}
