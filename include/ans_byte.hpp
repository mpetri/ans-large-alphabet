#pragma once

#include "ans_util.hpp"
#include "interp.hpp"

namespace constants {
    const uint32_t MAX_SIGMA = 256;
    const uint32_t MAX_FRAME_SIZE = 4096;
    const uint32_t FRAME_FACTOR = 64;
    const uint64_t RADIX_LOG2 = 32;
    const uint64_t RADIX = 1ULL << RADIX_LOG2;
    const uint64_t K = 16;
}

struct enc_entry {
    uint16_t freq;
    uint16_t base;
    uint64_t sym_upper_bound;
};


std::array<uint16_t, constants::MAX_SIGMA>
adjust_freqs(const std::array<uint64_t, constants::MAX_SIGMA>& freqs)
{
    std::array<uint16_t, constants::MAX_SIGMA> adj_freqs{ 0 };
    size_t uniq_syms = 0;
    size_t initial_sum = 0;
    for(size_t i=0;i<constants::MAX_SIGMA;i++) {
        initial_sum += freqs[i];
        uniq_syms += freqs[i] != 0;
    }
    size_t target_frame_size = uniq_syms * constants::FRAME_FACTOR;
    if(target_frame_size > constants::MAX_FRAME_SIZE) {
        target_frame_size = constants::MAX_FRAME_SIZE;
    }
    if (!is_power_of_two(target_frame_size)) {
        target_frame_size = next_power_of_two(target_frame_size);
    }

    double c = double(target_frame_size) / double(initial_sum);
    size_t cur_frame_size = std::numeric_limits<uint64_t>::max();
    double fudge = 1.0;
    while (cur_frame_size > target_frame_size) {
        fudge -= 0.01;
        cur_frame_size = 0;
        for(size_t sym=0;sym<constants::MAX_SIGMA;sym++) {
            adj_freqs[sym] = (fudge * double(freqs[sym]) * c);
            if(adj_freqs[sym] == 0 && freqs[sym] != 0) adj_freqs[sym] = 1;
            cur_frame_size += adj_freqs[sym];
        }
    }
    int64_t excess = target_frame_size - cur_frame_size;
    for(size_t sym=0;sym<constants::MAX_SIGMA;sym++) {
        auto ncnt = adj_freqs[constants::MAX_SIGMA-sym-1];
        if(ncnt == 0) continue;
        double ratio = double(excess) / double(cur_frame_size);
        size_t adder = size_t(ratio * double(ncnt));
        if (adder > excess) {
            adder = excess;
        }
        excess -= adder;
        cur_frame_size -= ncnt;
        adj_freqs[constants::MAX_SIGMA-sym-1] += adder;
    }
    if (excess != 0) {
        size_t max_freq = 0;
        size_t max_sym = 0;
        for(size_t sym=0;sym<constants::MAX_SIGMA;sym++) {
            if(adj_freqs[sym] > max_freq) {
                max_freq = adj_freqs[sym];
                max_sym = sym;
            }
        }
        adj_freqs[max_sym] += excess;
    }
    return adj_freqs;
}

struct ans_byte_encode {
    static ans_byte_encode create(const uint8_t* in_u8,size_t n) {
        ans_byte_encode model;
        std::array<uint64_t, constants::MAX_SIGMA> freqs{ 0 };
        for(size_t i=0;i<n;i++) {
            freqs[in_u8[i]]++;
        }
        model.nfreqs = adjust_freqs(freqs);
        model.frame_size = std::accumulate(std::begin(model.nfreqs),std::end(model.nfreqs), 0);
        uint64_t cur_base = 0;
        uint64_t tmp = constants::K * constants::RADIX;
        for(size_t sym=0;sym<constants::MAX_SIGMA;sym++) {
            model.table[sym].freq = model.nfreqs[sym];
            model.table[sym].base = cur_base;
            model.table[sym].sym_upper_bound = tmp * model.nfreqs[sym];
            cur_base += model.nfreqs[sym];
        }
        model.lower_bound = constants::K * model.frame_size;
        return model;
    }

    size_t serialize(uint8_t*& out_u8) {
        auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_u8);
        std::array<uint32_t, constants::MAX_SIGMA> increasing_freqs;
        increasing_freqs[0] = nfreqs[0];
        for(size_t sym=1;sym<constants::MAX_SIGMA;sym++) {
            increasing_freqs[sym] = increasing_freqs[sym-1] +  nfreqs[sym] + 1;
        }
        auto in_buf = increasing_freqs.data();
        auto bytes_written = interpolative_internal::encode(out_ptr_u32,in_buf,constants::MAX_SIGMA,constants::MAX_FRAME_SIZE+constants::MAX_SIGMA);

        out_u8 += bytes_written;
    }

    void encode_symbol(uint64_t& state,uint8_t sym,uint8_t*& out_u8) {
        const auto& e = table[sym];
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

    std::array<uint16_t, constants::MAX_SIGMA> nfreqs = { 0 };
    std::array<enc_entry, constants::MAX_SIGMA> table = { 0 };
    uint64_t frame_size;
    uint64_t lower_bound;
};

struct dec_entry {
    uint16_t freq;
    uint16_t offset;
    uint8_t sym;
};

struct ans_byte_decode {

    static ans_byte_decode load(const uint8_t* in_u8) {
        ans_byte_decode model;
        auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_u8);
        std::array<uint32_t, constants::MAX_SIGMA> efreqs;
        interpolative_internal::decode(in_ptr_u32,model.nfreqs.data(),constants::MAX_SIGMA,constants::MAX_FRAME_SIZE+constants::MAX_SIGMA);
        uint32_t prev = model.nfreqs[0];
        for(size_t sym=1;sym<constants::MAX_SIGMA;sym++) {
            auto cur = model.nfreqs[sym];
            model.nfreqs[sym] = cur - prev - 1;
            prev = cur;
        }
        model.frame_size = std::accumulate(std::begin(model.nfreqs),std::end(model.nfreqs), 0ULL);
        model.frame_mask = model.frame_size - 1;
        model.frame_log2 = log2(model.frame_size);
        model.table.resize(model.frame_size);
        uint64_t tmp = constants::K * constants::RADIX;
        uint16_t cur_base = 0;
        for(size_t sym=0;sym<constants::MAX_SIGMA;sym++) {
            auto cur_freq = model.nfreqs[sym];
            for(uint16_t k=0;k<cur_freq;k++) {
                model.table[cur_base+k].freq = cur_freq;
                model.table[cur_base+k].sym = sym;
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

    uint8_t decode_sym(uint64_t& state,const uint8_t*& in_u8) {
        const auto& entry = table[state & frame_mask];
        state = uint64_t(entry.freq) * (state >> frame_log2) + uint64_t(entry.offset);
        if (state < lower_bound) {
            in_u8 -= sizeof(uint32_t);
            auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_u8);
            state = state << constants::RADIX_LOG2 | uint64_t(*in_ptr_u32);
        }
        return entry.sym;
    }

    std::array<uint32_t, constants::MAX_SIGMA> nfreqs = {0};
    uint64_t frame_size;
    uint64_t frame_mask;
    uint64_t frame_log2;
    uint64_t lower_bound;
    std::vector<dec_entry> table;
};

size_t
ans_byte_compress(void* dst, size_t dstCapacity,const void* src, size_t srcSize)
{
    auto in_u8 = reinterpret_cast<const uint8_t*>(src);
    auto ans_frame = ans_byte_encode::create(in_u8,srcSize);
    uint8_t* out_u8 = reinterpret_cast<uint8_t*>(dst);

    // serialize model
    ans_frame.serialize(out_u8);

    // start encoding
    uint64_t state_a = ans_frame.initial_state();
    uint64_t state_b = ans_frame.initial_state();
    uint64_t state_c = ans_frame.initial_state();
    uint64_t state_d = ans_frame.initial_state();

    size_t cur_sym = 0;
    while( (srcSize-cur_sym) % 4 != 0) {
        ans_frame.encode_symbol(state_a,in_u8[srcSize-cur_sym-1],out_u8);
        cur_sym += 1;
    }
    while (cur_sym != srcSize) {
        ans_frame.encode_symbol(state_a,in_u8[srcSize-cur_sym-1],out_u8);
        ans_frame.encode_symbol(state_b,in_u8[srcSize-cur_sym-2],out_u8);
        ans_frame.encode_symbol(state_c,in_u8[srcSize-cur_sym-3],out_u8);
        ans_frame.encode_symbol(state_d,in_u8[srcSize-cur_sym-4],out_u8);
        cur_sym += 4;
    }

    // flush final state
    ans_frame.flush_state(state_a,out_u8);
    ans_frame.flush_state(state_b,out_u8);
    ans_frame.flush_state(state_c,out_u8);
    ans_frame.flush_state(state_d,out_u8);

    return out_u8 - reinterpret_cast<uint8_t*>(dst);
}

void ans_byte_decompress(void* dst,  size_t to_decode,const void* cSrc, size_t cSrcSize)
{
    auto in_u8 = reinterpret_cast<const uint8_t*>(cSrc);
    auto ans_frame = ans_byte_decode::load(in_u8);
    in_u8 += cSrcSize;

    auto state_d = ans_frame.init_state(in_u8);
    auto state_c = ans_frame.init_state(in_u8);
    auto state_b = ans_frame.init_state(in_u8);
    auto state_a = ans_frame.init_state(in_u8);

    size_t cur_idx = 0;
    auto out_u8 = reinterpret_cast<uint8_t*>(dst);
    size_t fast_decode = to_decode - (to_decode % 4);
    while(cur_idx != fast_decode) {
        out_u8[cur_idx] = ans_frame.decode_sym(state_d,in_u8);
        out_u8[cur_idx+1] = ans_frame.decode_sym(state_c,in_u8);
        out_u8[cur_idx+2] = ans_frame.decode_sym(state_b,in_u8);
        out_u8[cur_idx+3] = ans_frame.decode_sym(state_a,in_u8);
        cur_idx += 4;
    }
    while(cur_idx != to_decode) {
        out_u8[cur_idx++] = ans_frame.decode_sym(state_a,in_u8);
    }
}