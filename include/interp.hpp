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

#include <vector>

#include "bits.hpp"
#include "util.hpp"

struct interpolative_internal {

private:
    static inline void write_center_mid(
        bit_stream& os, uint64_t val, uint64_t u)
    {
        if (u == 1)
            return;
        auto b = bits::hi(u - 1) + 1ULL;
        uint64_t d = 2ULL * u - (1ULL << b);
        val = val + (u - (d >> 1));
        if (val > u)
            val -= u;
        uint32_t m = (1ULL << b) - u;
        if (val <= m) {
            os.put_int(val - 1ULL, b - 1ULL);
        } else {
            val += m;
            os.put_int((val - 1ULL) >> 1ULL, b - 1ULL);
            os.put_int((val - 1ULL) & 1ULL, 1ULL);
        }
    }
    static inline uint64_t read_center_mid(bit_stream& is, uint64_t u)
    {
        auto b = u == 1ULL ? 0ULL : bits::hi(u - 1ULL) + 1ULL;
        auto d = 2ULL * u - (1ULL << b);
        uint64_t val = 1ULL;
        if (u != 1) {
            uint64_t m = (1ULL << b) - u;
            val = is.get_int(b - 1) + 1;
            if (val > m) {
                val = (2ULL * val + is.get_int(1)) - m - 1ULL;
            }
        }
        val = val + (d >> 1ULL);
        if (val > u)
            val -= u;
        return val;
    }

    static inline void encode_interpolative(bit_stream& os,
        const uint32_t* in_buf, size_t n, size_t low, size_t high)
    {
        if (n == 0ULL)
            return;
        uint64_t h = (n + 1ULL) >> 1ULL;
        uint64_t n1 = h - 1ULL;
        uint64_t n2 = n - h;
        uint64_t v = in_buf[h - 1ULL] + 1ULL; // we don't encode 0

        write_center_mid(os, v - low - n1 + 1ULL, high - n2 - low - n1 + 1ULL);

        encode_interpolative(os, in_buf, n1, low, v - 1ULL);
        encode_interpolative(os, in_buf + h, n2, v + 1ULL, high);
    }

    static inline void decode_interpolative(
        bit_stream& is, uint32_t* out_buf, size_t n, size_t low, size_t high)
    {
        if (n == 0ULL)
            return;
        uint64_t h = (n + 1ULL) >> 1ULL;
        uint64_t n1 = h - 1ULL;
        uint64_t n2 = n - h;
        uint64_t v = low + n1 - 1ULL
            + read_center_mid(is, high - n2 - low - n1 + 1ULL);

        out_buf[h - 1] = v - 1; // we don't encode 0
        if (n1)
            decode_interpolative(is, out_buf, n1, low, v - 1ULL);
        if (n2)
            decode_interpolative(is, out_buf + h, n2, v + 1ULL, high);
    }

public:
    static inline size_t encode(
        uint32_t* f, const uint32_t* in_buf, size_t n, size_t u)
    {
        bit_stream os(f, true);
        size_t low = 1;
        size_t high = u + 1;
        encode_interpolative(os, in_buf, n, low, high);
        return os.flush();
    }

    static inline size_t decode(
        const uint32_t* f, uint32_t* out_buf, size_t n, size_t u)
    {
        bit_stream is((uint32_t*)f, false);
        size_t low = 1;
        size_t high = u + 1;
        decode_interpolative(is, out_buf, n, low, high);
        return is.u32_read();
    }
};
