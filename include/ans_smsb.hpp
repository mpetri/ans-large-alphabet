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

// code used to create Fig. 12 in the paper which shows trade-offs for different approximation ratios

#include "ans_util.hpp"
#include "interp.hpp"
#include "util.hpp"

#ifdef RECORD_STATS
#include "stats.hpp"
#endif

namespace smsb_constants {
    const uint32_t MAX_SIGMA = 1280;
    const uint64_t RADIX_LOG2 = 32;
    const uint64_t RADIX = 1ULL << RADIX_LOG2;
    const uint64_t K = 16;
}

struct enc_entry_smsb {
    uint32_t freq;
    uint32_t base;
    uint64_t sym_upper_bound;
};

uint32_t ans_smsb_mapping(uint32_t x)
{
    if (x <= 256)
        return x;
    if (x <= (1 << 16))
        return (x >> 8) + 256;
    if (x <= (1 << 24))
        return (x >> 16) + 512;
    return (x >> 24) + 768;
}

uint16_t ans_smsb_mapping_and_exceptions(uint32_t x, uint8_t*& except_out)
{
    if (x <= 256) {
        return x;
    }
    if (x <= (1 << 16)) {
        // one exception byte
        *except_out++ = x & 0xFF;
        return (x >> 8) + 256;
    }
    if (x <= (1 << 24)) {
        // two exception byte
        *except_out++ = x & 0xFF;
        *except_out++ = (x >> 8) & 0xFF;
        return (x >> 16) + 512;
    }
    // three exception byte
    *except_out++ = x & 0xFF;
    *except_out++ = (x >> 8) & 0xFF;
    *except_out++ = (x >> 16) & 0xFF;
    return (x >> 24) + 768;
}

template<uint32_t H_approx>
struct ans_smsb_encode {
    static ans_smsb_encode create(const uint32_t* in_u32,size_t n) {
        ans_smsb_encode model;
        std::vector<uint64_t> freqs(smsb_constants::MAX_SIGMA,0);
        uint32_t max_sym = 0;
        for(size_t i=0;i<n;i++) {
            auto mapped_u32 = ans_smsb_mapping(in_u32[i]);
            freqs[mapped_u32]++;
            max_sym = std::max(mapped_u32,max_sym);
        }
        model.nfreqs = adjust_freqs(freqs,max_sym,true,H_approx);
        model.frame_size = std::accumulate(std::begin(model.nfreqs),std::end(model.nfreqs), 0);
        uint64_t cur_base = 0;
        uint64_t tmp = constants::K * constants::RADIX;
        model.table.resize(max_sym+1);
        for(size_t sym=0;sym<model.nfreqs.size();sym++) {
            model.table[sym].freq = model.nfreqs[sym];
            model.table[sym].base = cur_base;
            model.table[sym].sym_upper_bound = tmp * model.nfreqs[sym];
            cur_base += model.nfreqs[sym];
        }
        model.lower_bound = constants::K * model.frame_size;
        return model;
    }

    size_t serialize(uint8_t*& out_u8) {
        return ans_serialize_interp(nfreqs,frame_size,out_u8);
    }

    void encode_symbol(uint64_t& state,uint32_t sym,uint8_t*& out_u8) {
        auto mapped_sym = ans_smsb_mapping_and_exceptions(sym,out_u8);
        const auto& e = table[mapped_sym];
        if (state >= e.sym_upper_bound) {
            auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_u8);
            *out_ptr_u32 = state & 0xFFFFFFFF;
            out_u8 += sizeof(uint32_t);
            state = state >> constants::RADIX_LOG2;
        }
        state = ((state / e.freq) * frame_size) + (state % e.freq) + e.base;
    }
    uint64_t initial_state() const {
        return lower_bound;
    }

    void flush_state(uint64_t state,uint8_t*& out_u8) {
        auto out_ptr_u64 = reinterpret_cast<uint64_t*>(out_u8);
        *out_ptr_u64++ = state - lower_bound;
        out_u8 += sizeof(uint64_t);
    }

    std::vector<uint32_t> nfreqs;
    std::vector<enc_entry_smsb> table;
    uint64_t frame_size;
    uint64_t lower_bound;
};

#pragma pack(push, 1)
struct dec_entry_smsb {
    uint32_t freq;
    uint32_t offset;
    uint32_t mapped_num;
};
#pragma pack(pop)

inline uint32_t ans_smsb_undo_mapping(const dec_entry_smsb& entry, const uint8_t*& in_u8)
{
    uint32_t except_bytes = entry.mapped_num >> 30;
    uint32_t mapped_num = entry.mapped_num & 0x3FFFFFFF;
    static std::array<uint32_t,4> except_mask = {0x0,0xFF,0xFFFF,0xFFFFFF};
    auto u32_ptr = in_u8-except_bytes;
    auto except_u32 = reinterpret_cast<const uint32_t*>(u32_ptr);
    mapped_num = mapped_num + (*except_u32 & except_mask[except_bytes]);
    in_u8 -= except_bytes;
    return mapped_num;
}

uint32_t ans_smsb_undo_mapping(uint32_t x)
{
    if (x <= 256)
        return x;
    if (x <= 512)
        return ((x - 256) << 8);
    if (x <= 768)
        return ((x - 512) << 16);
    return ((x - 768) << 24);
}

uint32_t ans_smsb_exception_bytes(uint32_t x)
{
    if (x <= 256)
        return 0;
    if (x <= 512)
        return 1;
    if (x <= 768)
        return 2;
    return 3;
}

struct ans_smsb_decode {
    static ans_smsb_decode load(const uint8_t* in_u8) {
        ans_smsb_decode model;
        model.nfreqs = ans_load_interp(in_u8);
        model.frame_size = std::accumulate(std::begin(model.nfreqs),std::end(model.nfreqs), 0);
        model.frame_mask = model.frame_size - 1;
        model.frame_log2 = log2(model.frame_size);
        model.table.resize(model.frame_size);
        auto max_sym = model.nfreqs.size() - 1;
        uint64_t tmp = constants::K * constants::RADIX;
        uint32_t cur_base = 0;
        for(size_t sym=0;sym<=max_sym;sym++) {
            auto cur_freq = model.nfreqs[sym];
            for(uint32_t k=0;k<cur_freq;k++) {
                uint32_t except_bytes = ans_smsb_exception_bytes(sym);
                model.table[cur_base+k].freq = cur_freq;
                model.table[cur_base+k].mapped_num = ans_smsb_undo_mapping(sym) + (except_bytes << 30);
                model.table[cur_base+k].offset = k;
            }
            cur_base += model.nfreqs[sym];
        }
        model.lower_bound = constants::K * model.frame_size;
        return model;
    }

    uint64_t init_state(const uint8_t*& in_u8) {
        in_u8 -= sizeof(uint64_t);
        auto in_ptr_u64 = reinterpret_cast<const uint64_t*>(in_u8);
        return *in_ptr_u64 + lower_bound;
    }

    uint32_t decode_sym(uint64_t& state,const uint8_t*& in_u8) {
        const auto& entry = table[state & frame_mask];
        state = uint64_t(entry.freq) * (state >> frame_log2) + uint64_t(entry.offset);
        if (state < lower_bound) {
            in_u8 -= sizeof(uint32_t);
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_u8);
            state = state << constants::RADIX_LOG2 | uint64_t(*in_ptr_u32);
        }
        auto decoded_sym = ans_smsb_undo_mapping(entry, in_u8);
        return decoded_sym;
    }

    std::vector<uint32_t> nfreqs;
    uint64_t frame_size;
    uint64_t frame_mask;
    uint64_t frame_log2;
    uint64_t lower_bound;
    std::vector<dec_entry_smsb> table;
};

template<uint32_t H_approx>
size_t ans_smsb_compress(uint8_t* dst, size_t dstCapacity,const uint32_t* src, size_t srcSize)
{
    const uint32_t num_states = 4;
#ifdef RECORD_STATS
    auto start_compress = std::chrono::high_resolution_clock::now();
#endif

    auto in_u32 = reinterpret_cast<const uint32_t*>(src);
    auto ans_frame = ans_smsb_encode<H_approx>::create(in_u32,srcSize);
    uint8_t* out_u8 = reinterpret_cast<uint8_t*>(dst);

    // serialize model
    ans_frame.serialize(out_u8);

    std::array<uint64_t,num_states> states;

    // start encoding
    for(uint32_t i=0;i<num_states;i++)
        states[i] = ans_frame.initial_state();

#ifdef RECORD_STATS
    auto stop_prelude = std::chrono::high_resolution_clock::now();
    get_stats().prelude_bytes = out_u8 - reinterpret_cast<uint8_t*>(dst);
    get_stats().prelude_time_ns = (stop_prelude - start_compress).count();
#endif


    size_t cur_sym = 0;
    while( (srcSize-cur_sym) % num_states != 0) {
        ans_frame.encode_symbol(states[0],in_u32[srcSize-cur_sym-1],out_u8);
        cur_sym += 1;
    }
    while (cur_sym != srcSize) {
        ans_frame.encode_symbol(states[0],in_u32[srcSize-cur_sym-1],out_u8);
        ans_frame.encode_symbol(states[1],in_u32[srcSize-cur_sym-2],out_u8);
        ans_frame.encode_symbol(states[2],in_u32[srcSize-cur_sym-3],out_u8);
        ans_frame.encode_symbol(states[3],in_u32[srcSize-cur_sym-4],out_u8);
        cur_sym += num_states;
    }

    // flush final state
    for(uint32_t i=0;i<num_states;i++)
        ans_frame.flush_state(states[i],out_u8);

#ifdef RECORD_STATS
    auto stop_compress = std::chrono::high_resolution_clock::now();
    get_stats().encode_bytes = (out_u8 - reinterpret_cast<uint8_t*>(dst)) - get_stats().prelude_bytes;
    get_stats().encode_time_ns = (stop_compress - stop_prelude).count();
#endif

    return out_u8 - reinterpret_cast<uint8_t*>(dst);
}

void ans_smsb_decompress(uint32_t* dst,  size_t to_decode,const uint8_t* cSrc, size_t cSrcSize)
{
    const uint32_t num_states = 4;
    auto in_u8 = reinterpret_cast<const uint8_t*>(cSrc);
    auto ans_frame = ans_smsb_decode::load(in_u8);
    in_u8 += cSrcSize;

    std::array<uint64_t,num_states> states;

    for(uint32_t i=0;i<num_states;i++) {
        states[i] = ans_frame.init_state(in_u8);
    }

    size_t cur_idx = 0;
    auto out_u32 = reinterpret_cast<uint32_t*>(dst);
    size_t fast_decode = to_decode - (to_decode % num_states);
    while(cur_idx != fast_decode) {
        out_u32[cur_idx] = ans_frame.decode_sym(states[0],in_u8);
        out_u32[cur_idx+1] = ans_frame.decode_sym(states[1],in_u8);
        out_u32[cur_idx+2] = ans_frame.decode_sym(states[2],in_u8);
        out_u32[cur_idx+3] = ans_frame.decode_sym(states[3],in_u8);
        cur_idx += num_states;
    }
    while(cur_idx != to_decode) {
        out_u32[cur_idx++] = ans_frame.decode_sym(states[num_states-1],in_u8);
    }
}