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
#include "interp.hpp"
#include "util.hpp"

namespace reorder_fold_constants {
const uint64_t RADIX_LOG2 = 32;
const uint64_t RADIX = 1ULL << RADIX_LOG2;
const uint64_t K = 16;
}

struct enc_entry_reorder_fold {
    uint16_t freq;
    uint32_t base;
    uint64_t sym_upper_bound;
};

template <uint32_t fidelity> uint32_t ans_reorder_fold_mapping(uint32_t x)
{
    const uint32_t radix = 8;
    uint32_t radix_mask = ((1 << radix) - 1);
    size_t offset = 0;
    size_t thres = 1 << (fidelity + radix - 1);
    while (x >= thres) {
        auto digit = x & radix_mask;
        x = x >> radix;
        offset = offset + (1 << (fidelity - 1)) * radix_mask;
    }
    return x + offset;
}

template <uint32_t fidelity>
uint16_t ans_reorder_fold_mapping_and_exceptions(
    uint32_t x, uint8_t*& except_out)
{
    const uint32_t radix = 8;
    uint32_t radix_mask = ((1 << radix) - 1);
    size_t offset = 0;
    size_t thres = 1 << (fidelity + radix - 1);
    while (x >= thres) {
        *except_out++ = x & radix_mask;
        x = x >> radix;
        offset = offset + (1 << (fidelity - 1)) * radix_mask;
    }
    return x + offset;
}

// here we reorder the most frequent symbols in the input to the front
// so our mapping procedure performs better as it expects a decreasing
// frequency distribution: f_1 >= f_2 >= f_3 etc.
template <uint32_t fidelity> struct ans_reorder_fold_encode {
    static ans_reorder_fold_encode create(const uint32_t* in_u32, size_t n)
    {
        ans_reorder_fold_encode model;

        // 1) identify most frequent syms
        uint32_t unmapped_max_sym = 0;
        for (size_t i = 0; i < n; i++) {
            unmapped_max_sym = std::max(in_u32[i], unmapped_max_sym);
        }
        std::vector<std::pair<int64_t, uint32_t>> unmapped_freqs(
            unmapped_max_sym + 1);
        for (size_t i = 0; i < n; i++) {
            unmapped_freqs[in_u32[i]].first--;
            unmapped_freqs[in_u32[i]].second = in_u32[i];
        }
        std::sort(unmapped_freqs.begin(), unmapped_freqs.end());
        model.sigma = 0;
        for (size_t i = 0; i < unmapped_freqs.size(); i++) {
            if (unmapped_freqs[i].first == 0)
                break;
            model.sigma++;
        }
        size_t no_except_thres = 1 << (fidelity + 8 - 1);
        model.mapping.resize(unmapped_max_sym + 1);
        if (model.sigma < no_except_thres) {
            for (size_t i = 0; i < unmapped_max_sym + 1; i++) {
                model.mapping[i] = i;
            }
        } else {
            for (size_t i = 0; i <= unmapped_max_sym; i++) {
                model.mapping[i] = i + no_except_thres;
            }
            for (size_t i = 0; i < no_except_thres; i++) {
                model.mapping[unmapped_freqs[i].second] = i;
                model.most_frequent.push_back(unmapped_freqs[i].second);
            }
        }
        const uint32_t MAX_SIGMA = 1 << (fidelity + 8 + 1);
        std::vector<uint64_t> freqs(MAX_SIGMA, 0);
        uint32_t max_sym = 0;
        for (size_t i = 0; i < n; i++) {
            auto mapped_u32
                = ans_reorder_fold_mapping<fidelity>(model.mapping[in_u32[i]]);
            freqs[mapped_u32]++;
            max_sym = std::max(mapped_u32, max_sym);
        }
        model.nfreqs = adjust_freqs(freqs, max_sym, true);
        model.frame_size = std::accumulate(
            std::begin(model.nfreqs), std::end(model.nfreqs), 0);
        uint64_t cur_base = 0;
        uint64_t tmp = constants::K * constants::RADIX;
        model.table.resize(max_sym + 1);
        for (size_t sym = 0; sym < model.nfreqs.size(); sym++) {
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
        size_t no_except_thres = 1 << (fidelity + 8 - 1);
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_u8);
        size_t bytes_written = 0;
        if (sigma < no_except_thres) {
            *out_ptr_u32++ = 0;
            bytes_written += sizeof(uint32_t);
        } else {
            *out_ptr_u32++ = 1;
            bytes_written += sizeof(uint32_t);
            for (size_t i = 0; i < no_except_thres; i++) {
                *out_ptr_u32++ = most_frequent[i];
            }
            out_u8 += sizeof(uint32_t) * no_except_thres;
            bytes_written += sizeof(uint32_t) * no_except_thres;
        }
        out_u8 += sizeof(uint32_t);
        bytes_written += sizeof(uint32_t);
        auto interp_written_bytes
            = ans_serialize_interp(nfreqs, frame_size, out_u8);
        return interp_written_bytes + bytes_written;
    }

    void encode_symbol(uint64_t& state, uint32_t sym, uint8_t*& out_u8)
    {
        auto mapped_sym = ans_reorder_fold_mapping_and_exceptions<fidelity>(
            mapping[sym], out_u8);
        const auto& e = table[mapped_sym];
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
    std::vector<enc_entry_reorder_fold> table;
    std::vector<uint32_t> mapping;
    std::vector<uint32_t> most_frequent;
    uint64_t frame_size;
    uint64_t lower_bound;
    uint64_t sigma;
};

struct dec_entry_reorder_fold {
    uint16_t freq;
    uint16_t offset;
    uint32_t mapped_num;
};

uint32_t ans_reorder_fold_undo_mapping(
    const dec_entry_reorder_fold& entry, const uint8_t*& in_u8)
{
    uint32_t except_bytes = entry.mapped_num >> 30;
    uint32_t mapped_num = entry.mapped_num & 0x3FFFFFFF;
    static std::array<uint32_t, 4> except_mask
        = { 0x0, 0xFF, 0xFFFF, 0xFFFFFF };
    auto u32_ptr = in_u8 - except_bytes;
    auto except_u32 = reinterpret_cast<const uint32_t*>(u32_ptr);
    uint32_t num = mapped_num + (*except_u32 & except_mask[except_bytes]);
    in_u8 -= except_bytes;
    return num;
}

template <uint32_t fidelity>
uint32_t ans_reorder_fold_undo_mapping(
    std::vector<uint32_t>& most_frequent, uint32_t x_plus_offset)
{
    const uint32_t radix = 8;
    uint32_t div = (1 << (fidelity - 1)) * ((1 << radix) - 1);
    size_t thres = 1 << (fidelity + radix - 1);
    if (x_plus_offset < thres)
        return most_frequent[x_plus_offset] + thres;
    auto output_bytes = (x_plus_offset - thres) / div + 1;
    auto x_org = x_plus_offset - (div * output_bytes);
    return x_org << (radix * output_bytes);
}

template <uint32_t fidelity>
uint32_t ans_reorder_fold_exception_bytes(uint32_t x_plus_offset)
{
    const uint32_t radix = 8;
    uint32_t div = (1 << (fidelity - 1)) * ((1 << radix) - 1);
    size_t thres = 1 << (fidelity + radix - 1);
    if (x_plus_offset < thres)
        return 0;
    auto output_bytes = (x_plus_offset - thres) / div + 1;
    return output_bytes;
}

template <uint32_t fidelity> struct ans_reorder_fold_decode {

    static ans_reorder_fold_decode load(const uint8_t* in_u8)
    {
        ans_reorder_fold_decode model;
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_u8);
        in_u8 += sizeof(uint32_t);
        uint32_t do_reorder = *in_ptr_u32++;
        size_t no_except_thres = 1 << (fidelity + 8 - 1);
        std::vector<uint32_t> most_frequent(no_except_thres);
        if (do_reorder == 1) {
            for (size_t i = 0; i < no_except_thres; i++) {
                most_frequent[i] = *in_ptr_u32++;
            }
            in_u8 += no_except_thres * sizeof(uint32_t);
        } else {
            for (size_t i = 0; i < no_except_thres; i++) {
                most_frequent[i] = i;
            }
        }

        model.nfreqs = ans_load_interp(in_u8);
        model.frame_size = std::accumulate(
            std::begin(model.nfreqs), std::end(model.nfreqs), 0);
        model.frame_mask = model.frame_size - 1;
        model.frame_log2 = log2(model.frame_size);
        model.table.resize(model.frame_size);
        auto max_sym = model.nfreqs.size() - 1;
        uint64_t tmp = constants::K * constants::RADIX;
        uint32_t cur_base = 0;
        for (size_t sym = 0; sym <= max_sym; sym++) {
            auto cur_freq = model.nfreqs[sym];
            uint32_t except_bytes
                = ans_reorder_fold_exception_bytes<fidelity>(sym);
            uint32_t mapped_num
                = ans_reorder_fold_undo_mapping<fidelity>(most_frequent, sym)
                + (except_bytes << 30);
            for (uint32_t k = 0; k < cur_freq; k++) {
                model.table[cur_base + k].freq = cur_freq;
                model.table[cur_base + k].mapped_num = mapped_num;
                model.table[cur_base + k].offset = k;
            }
            cur_base += model.nfreqs[sym];
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

    uint32_t decode_sym(uint64_t& state, const uint8_t*& in_u8)
    {
        const uint32_t radix = 8;
        const size_t thres = 1 << (fidelity + radix - 1);
        const auto& entry = table[state & frame_mask];
        state = uint64_t(entry.freq) * (state >> frame_log2)
            + uint64_t(entry.offset);
        if (state < lower_bound) {
            in_u8 -= sizeof(uint32_t);
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_u8);
            state = state << constants::RADIX_LOG2 | uint64_t(*in_ptr_u32);
        }
        auto decoded_sym = ans_reorder_fold_undo_mapping(entry, in_u8);
        return decoded_sym - thres;
    }

    std::vector<uint32_t> nfreqs;
    uint64_t frame_size;
    uint64_t frame_mask;
    uint64_t frame_log2;
    uint64_t lower_bound;
    std::vector<dec_entry_reorder_fold> table;
};

template <uint32_t fidelity>
size_t ans_reorder_fold_compress(
    uint8_t* dst, size_t dstCapacity, const uint32_t* src, size_t srcSize)
{
    auto in_u32 = reinterpret_cast<const uint32_t*>(src);
    auto ans_frame = ans_reorder_fold_encode<fidelity>::create(in_u32, srcSize);
    uint8_t* out_u8 = reinterpret_cast<uint8_t*>(dst);

    // serialize model
    ans_frame.serialize(out_u8);

    // start encoding
    std::array<uint64_t, 4> states;
    states[0] = ans_frame.initial_state();
    states[1] = ans_frame.initial_state();
    states[2] = ans_frame.initial_state();
    states[3] = ans_frame.initial_state();

    size_t cur_sym = 0;
    while ((srcSize - cur_sym) % 4 != 0) {
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
        cur_sym += 4;
    }

    // flush final state
    ans_frame.flush_state(states[0], out_u8);
    ans_frame.flush_state(states[1], out_u8);
    ans_frame.flush_state(states[2], out_u8);
    ans_frame.flush_state(states[3], out_u8);

    return out_u8 - reinterpret_cast<uint8_t*>(dst);
}

template <uint32_t fidelity>
void ans_reorder_fold_decompress(
    uint32_t* dst, size_t to_decode, const uint8_t* cSrc, size_t cSrcSize)
{
    auto in_u8 = reinterpret_cast<const uint8_t*>(cSrc);
    auto ans_frame = ans_reorder_fold_decode<fidelity>::load(in_u8);
    in_u8 += cSrcSize;

    std::array<uint64_t, 4> states;
    states[3] = ans_frame.init_state(in_u8);
    states[2] = ans_frame.init_state(in_u8);
    states[1] = ans_frame.init_state(in_u8);
    states[0] = ans_frame.init_state(in_u8);

    size_t cur_idx = 0;
    auto out_u32 = reinterpret_cast<uint32_t*>(dst);
    size_t fast_decode = to_decode - (to_decode % 4);
    while (cur_idx != fast_decode) {
        out_u32[cur_idx] = ans_frame.decode_sym(states[3], in_u8);
        out_u32[cur_idx + 1] = ans_frame.decode_sym(states[2], in_u8);
        out_u32[cur_idx + 2] = ans_frame.decode_sym(states[1], in_u8);
        out_u32[cur_idx + 3] = ans_frame.decode_sym(states[0], in_u8);
        cur_idx += 4;
    }
    while (cur_idx != to_decode) {
        out_u32[cur_idx] = ans_frame.decode_sym(states[0], in_u8);
        cur_idx++;
    }
}
