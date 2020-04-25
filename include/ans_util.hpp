#pragma once

#include "interp.hpp"
#include "vbyte.hpp"

std::vector<uint32_t> ans_load_interp(const uint8_t* in_u8)
{
    uint32_t max_sym = vbyte_decode_u32(in_u8);
    uint32_t frame_size = (1 << (*in_u8++));
    auto in_ptr_u32 = reinterpret_cast<const uint32_t*>(in_u8);
    std::vector<uint32_t> vec(max_sym+1);
    interpolative_internal::decode(in_ptr_u32,vec.data(),vec.size(),frame_size+vec.size()+1);
    uint32_t prev = vec[0];
    uint32_t max_norm_freq = 0;
    for(size_t sym=1;sym<=max_sym;sym++) {
        auto cur = vec[sym];
        vec[sym] = cur - prev - 1;
        max_norm_freq = std::max(max_norm_freq,vec[sym]);
        prev = cur;
    }
    return vec;
}

size_t ans_serialize_interp(std::vector<uint32_t>& vec,size_t frame_size,uint8_t*& out_u8)
{
    uint32_t max_sym = vec.size()-1;
    vbyte_encode_u32(out_u8,max_sym);
    *out_u8++ = log2(frame_size); // must be power of 2
    auto out_ptr_u32 = reinterpret_cast<uint32_t*>(out_u8);
    std::vector<uint32_t> increasing_freqs(vec.size());
    increasing_freqs[0] = vec[0];
    for(size_t sym=1;sym<=max_sym;sym++) {
        increasing_freqs[sym] = increasing_freqs[sym-1] +  vec[sym] + 1;
    }
    auto in_buf = increasing_freqs.data();
    auto bytes_written = interpolative_internal::encode(out_ptr_u32,in_buf,vec.size(),frame_size+vec.size()+1);
    out_u8 += bytes_written;
    return bytes_written;
}

uint64_t next_power_of_two(uint64_t x)
{
    if (x == 0) {
        return 1;
    }
    uint32_t res = 63 - __builtin_clzll(x);
    return (1ULL << (res + 1));
}

bool is_power_of_two(uint64_t x) { return ((x != 0) && !(x & (x - 1))); }

bool
scale_freqs(std::vector<uint32_t>& S,const std::vector<uint64_t>& F,std::vector<uint32_t>& mapping,int64_t M,size_t sigma,size_t freq_sum)
{
    double fratio = double(M)/double(freq_sum);
    for(size_t cur_sym=0;cur_sym<sigma;cur_sym++) {
        auto mapped_sym = mapping[cur_sym];
        double aratio = double(M)/double(freq_sum);
        double ratio  = (sigma-cur_sym)*fratio/sigma + cur_sym*aratio/sigma;
        S[mapped_sym] = (uint32_t) (0.5 + aratio*F[mapped_sym]);
        if(S[mapped_sym]==0)
            S[mapped_sym] = 1;
        M -= S[mapped_sym];
        freq_sum -= F[mapped_sym];
        if(M < 0)
            break;
    }
    return M != 0;
}


std::vector<uint32_t>
adjust_freqs(const std::vector<uint64_t>& freqs,uint32_t largest_sym,bool require_u16,uint32_t H_approx = 1)
{
    size_t sigma = 0;
    size_t freq_sum = 0;
    for(size_t i=0;i<freqs.size();i++) {
        freq_sum += freqs[i];
        sigma += (freqs[i] != 0);
    }
    size_t target_frame_size = sigma;
    if (!is_power_of_two(target_frame_size)) {
        target_frame_size = next_power_of_two(target_frame_size);
    }

    std::vector<std::pair<uint64_t,uint32_t>> sorted_freqs;
    for(size_t i=0;i<freqs.size();i++) {
        if((freqs[i] != 0)) sorted_freqs.emplace_back(freqs[i],i);
    }
    std::sort(sorted_freqs.begin(),sorted_freqs.end());
    std::vector<uint32_t> mapping(sigma);
    for(size_t i=0;i<sorted_freqs.size();i++) mapping[i] = sorted_freqs[i].second;

    auto H = entropy(freqs,freq_sum);
    std::vector<uint32_t> scaled(largest_sym+1,0);
    std::vector<uint32_t> prev(largest_sym+1,0);
    double approx_factor = 1.0 + double(H_approx)/double(1000);
    double threshold = H * approx_factor;
    uint32_t u16_limit = std::numeric_limits<uint16_t>::max();
    while(true) {
        if( scale_freqs(scaled,freqs,mapping,target_frame_size,sigma,freq_sum) ) {
            target_frame_size *= 2;
            continue;
        }
        auto max_norm_freq = *std::max_element(scaled.begin(),scaled.end());
        auto XH = cross_entropy(freqs,scaled);
        if(require_u16 && max_norm_freq >= u16_limit) {
            // std::cout << "abort due to u16 overflow" << std::endl;
            scaled = prev;
            break;
        }
        //std::cout << "sigma=" << sigma << " m=" << freq_sum << " M=" << target_frame_size << " H=" << H << " XH=" << XH << " max_freq= " << max_norm_freq << std::endl;
        if( XH < threshold) {
            break;
        }
        target_frame_size *= 2;
        prev = scaled;
    }

    return scaled;
}
