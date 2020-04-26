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

#include "ans_util.hpp"

#ifdef RECORD_STATS
#include "stats.hpp"
#endif

namespace sint_constants {
const uint64_t RADIX_LOG2 = 32;
const uint64_t RADIX = 1ULL << RADIX_LOG2;
const uint64_t K = 16;
}

struct enc_entry_sint {
    uint32_t freq;
    uint32_t base;
    uint64_t sym_upper_bound;
};

template <uint32_t H_approx> struct ans_sint_encode {
    static ans_sint_encode create(const uint32_t* in_u32, size_t n)
    {
        ans_sint_encode model;
        uint32_t max_sym = 0;
        for (size_t i = 0; i < n; i++) {
            max_sym = std::max(in_u32[i], max_sym);
        }
        std::vector<uint64_t> freqs(max_sym + 1, 0);
        for (size_t i = 0; i < n; i++) {
            freqs[in_u32[i]]++;
        }
        model.nfreqs = adjust_freqs(freqs, max_sym, false, H_approx);
        model.frame_size = std::accumulate(
            std::begin(model.nfreqs), std::end(model.nfreqs), 0);
        uint64_t cur_base = 0;
        uint64_t tmp = constants::K * constants::RADIX;

        model.table.resize(max_sym + 1);
        for (size_t sym = 0; sym <= max_sym; sym++) {
            if (model.nfreqs[sym] == 0)
                continue;
            model.table[sym].freq = model.nfreqs[sym];
            model.table[sym].base = cur_base;
            model.table[sym].sym_upper_bound = tmp * model.nfreqs[sym];
            cur_base += model.nfreqs[sym];
        }
        model.lower_bound = constants::K * model.frame_size;
        return model;
    }

    size_t serialize(uint8_t*& out_u8)
    {
        return ans_serialize_interp(nfreqs, frame_size, out_u8);
    }

    void encode_symbol(uint64_t& state, uint32_t sym, uint8_t*& out_u8)
    {
        const auto& e = table[sym];
        if (state >= e.sym_upper_bound) {
            auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_u8);
            *out_ptr_u32 = state & 0xFFFFFFFF;
            out_u8 += sizeof(uint32_t);
            state = state >> constants::RADIX_LOG2;
        }
        state = ((state / e.freq) * frame_size) + (state % e.freq) + e.base;
    }
    uint64_t initial_state() const { return lower_bound; }

    void flush_state(uint64_t state, uint8_t*& out_u8)
    {
        auto out_ptr_u64 = reinterpret_cast<uint64_t*>(out_u8);
        *out_ptr_u64++ = state - lower_bound;
        out_u8 += sizeof(uint64_t);
    }

    std::vector<uint32_t> nfreqs;
    std::vector<enc_entry_sint> table;
    uint64_t frame_size;
    uint64_t lower_bound;
};

struct dec_entry_sint {
    uint32_t freq;
    uint32_t offset;
    uint32_t sym;
};

struct dec_entry_sint_small {
    uint16_t freq;
    uint16_t offset;
    uint32_t sym;
};

enum dec_table_type_sint { SINT_SMALL, SINT_LARGE };

struct ans_sint_decode {

    static ans_sint_decode load(const uint8_t* in_u8)
    {
        ans_sint_decode model;
        model.nfreqs = ans_load_interp(in_u8);
        auto max_norm_freq
            = *std::max_element(model.nfreqs.begin(), model.nfreqs.end());
        model.frame_size = std::accumulate(
            std::begin(model.nfreqs), std::end(model.nfreqs), 0);
        model.frame_mask = model.frame_size - 1;
        model.frame_log2 = log2(model.frame_size);
        auto max_sym = model.nfreqs.size() - 1;
        uint64_t tmp = constants::K * constants::RADIX;
        uint32_t cur_base = 0;
        if (max_norm_freq <= std::numeric_limits<uint16_t>::max()) {
            model.table.resize(model.frame_size * sizeof(dec_entry_sint_small));
            model.table_type = dec_table_type_sint::SINT_SMALL;
            auto table
                = reinterpret_cast<dec_entry_sint_small*>(model.table.data());
            for (size_t sym = 0; sym <= max_sym; sym++) {
                auto cur_freq = model.nfreqs[sym];
                for (uint32_t k = 0; k < cur_freq; k++) {
                    dec_entry_sint_small* entry = table + cur_base + k;
                    entry->freq = cur_freq;
                    entry->sym = sym;
                    entry->offset = k;
                }
                cur_base += model.nfreqs[sym];
            }
        } else {
            model.table.resize(model.frame_size * sizeof(dec_entry_sint));
            model.table_type = dec_table_type_sint::SINT_LARGE;
            auto table = reinterpret_cast<dec_entry_sint*>(model.table.data());
            for (size_t sym = 0; sym <= max_sym; sym++) {
                auto cur_freq = model.nfreqs[sym];
                if (cur_freq == 0)
                    continue;
                for (uint32_t k = 0; k < cur_freq; k++) {
                    dec_entry_sint* entry = table + cur_base + k;
                    table[cur_base + k].freq = cur_freq;
                    table[cur_base + k].sym = sym;
                    table[cur_base + k].offset = k;
                }
                dec_entry_sint* entry = table + cur_base;
                cur_base += model.nfreqs[sym];
            }
        }
        model.lower_bound = constants::K * model.frame_size;
        return model;
    }

    uint64_t init_state(const uint8_t*& in_u8)
    {
        in_u8 -= sizeof(uint64_t);
        auto in_ptr_u64 = reinterpret_cast<const uint64_t*>(in_u8);
        return *in_ptr_u64 + lower_bound;
    }

    template <class t_entry>
    uint32_t decode_sym(uint64_t& state, const uint8_t*& in_u8)
    {
        auto tbl = reinterpret_cast<const t_entry*>(table.data());
        const auto& entry = tbl[state & frame_mask];
        state = uint64_t(entry.freq) * (state >> frame_log2)
            + uint64_t(entry.offset);
        if (state < lower_bound) {
            in_u8 -= sizeof(uint32_t);
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_u8);
            state = state << constants::RADIX_LOG2 | uint64_t(*in_ptr_u32);
        }
        return entry.sym;
    }

    std::vector<uint32_t> nfreqs;
    uint64_t frame_size;
    uint64_t frame_mask;
    uint64_t frame_log2;
    uint64_t lower_bound;
    dec_table_type_sint table_type;
    std::vector<uint8_t> table;
};

template <uint32_t H_approx>
size_t ans_sint_compress(
    uint8_t* dst, size_t dstCapacity, const uint32_t* src, size_t srcSize)
{
    const uint32_t num_states = 4;
#ifdef RECORD_STATS
    auto start_compress = std::chrono::high_resolution_clock::now();
#endif

    auto in_u32 = reinterpret_cast<const uint32_t*>(src);
    auto ans_frame = ans_sint_encode<H_approx>::create(in_u32, srcSize);
    uint8_t* out_u8 = reinterpret_cast<uint8_t*>(dst);

    // serialize model
    ans_frame.serialize(out_u8);

    std::array<uint64_t, num_states> states;

    // start encoding
    for (uint32_t i = 0; i < num_states; i++)
        states[i] = ans_frame.initial_state();

#ifdef RECORD_STATS
    auto stop_prelude = std::chrono::high_resolution_clock::now();
    get_stats().prelude_bytes = out_u8 - reinterpret_cast<uint8_t*>(dst);
    get_stats().prelude_time_ns = (stop_prelude - start_compress).count();
#endif

    // std::cout << "encoding" << std::endl;
    size_t cur_sym = 0;
    while ((srcSize - cur_sym) % num_states != 0) {
        ans_frame.encode_symbol(
            states[0], in_u32[srcSize - cur_sym - 1], out_u8);
        cur_sym += 1;
    }
    while (cur_sym != srcSize) {
        ans_frame.encode_symbol(
            states[0], in_u32[srcSize - cur_sym - 1], out_u8);
        ans_frame.encode_symbol(
            states[1], in_u32[srcSize - cur_sym - 2], out_u8);
        ans_frame.encode_symbol(
            states[2], in_u32[srcSize - cur_sym - 3], out_u8);
        ans_frame.encode_symbol(
            states[3], in_u32[srcSize - cur_sym - 4], out_u8);
        cur_sym += num_states;
    }

    // flush final state
    for (uint32_t i = 0; i < num_states; i++)
        ans_frame.flush_state(states[i], out_u8);

#ifdef RECORD_STATS
    auto stop_compress = std::chrono::high_resolution_clock::now();
    get_stats().encode_bytes = (out_u8 - reinterpret_cast<uint8_t*>(dst))
        - get_stats().prelude_bytes;
    get_stats().encode_time_ns = (stop_compress - stop_prelude).count();
#endif

    return out_u8 - reinterpret_cast<uint8_t*>(dst);
}

void ans_sint_decompress(
    uint32_t* dst, size_t to_decode, const uint8_t* cSrc, size_t cSrcSize)
{
    const uint32_t num_states = 4;
    auto in_u8 = reinterpret_cast<const uint8_t*>(cSrc);
    auto ans_frame = ans_sint_decode::load(in_u8);
    in_u8 += cSrcSize;

    std::array<uint64_t, num_states> states;

    for (uint32_t i = 0; i < num_states; i++) {
        states[i] = ans_frame.init_state(in_u8);
    }
    size_t cur_idx = 0;
    auto out_u32 = reinterpret_cast<uint32_t*>(dst);
    size_t fast_decode = to_decode - (to_decode % num_states);
    if (ans_frame.table_type == dec_table_type_sint::SINT_SMALL) {
        while (cur_idx != fast_decode) {
            out_u32[cur_idx]
                = ans_frame.decode_sym<dec_entry_sint_small>(states[0], in_u8);
            out_u32[cur_idx + 1]
                = ans_frame.decode_sym<dec_entry_sint_small>(states[1], in_u8);
            out_u32[cur_idx + 2]
                = ans_frame.decode_sym<dec_entry_sint_small>(states[2], in_u8);
            out_u32[cur_idx + 3]
                = ans_frame.decode_sym<dec_entry_sint_small>(states[3], in_u8);
            cur_idx += num_states;
        }
        while (cur_idx != to_decode) {
            out_u32[cur_idx++] = ans_frame.decode_sym<dec_entry_sint_small>(
                states[num_states - 1], in_u8);
        }
    } else {
        while (cur_idx != fast_decode) {
            out_u32[cur_idx]
                = ans_frame.decode_sym<dec_entry_sint>(states[0], in_u8);
            out_u32[cur_idx + 1]
                = ans_frame.decode_sym<dec_entry_sint>(states[1], in_u8);
            out_u32[cur_idx + 2]
                = ans_frame.decode_sym<dec_entry_sint>(states[2], in_u8);
            out_u32[cur_idx + 3]
                = ans_frame.decode_sym<dec_entry_sint>(states[3], in_u8);
            cur_idx += num_states;
        }
        while (cur_idx != to_decode) {
            out_u32[cur_idx++] = ans_frame.decode_sym<dec_entry_sint>(
                states[num_states - 1], in_u8);
        }
    }
}