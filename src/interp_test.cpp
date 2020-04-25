#include <iostream>
#include <vector>
#include <random>

#include "cutil.hpp"
#include "util.hpp"
#include "interp.hpp"
#include "bits.hpp"

inline void write_center_mid(
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

inline uint64_t read_center_mid(bit_stream& is, uint64_t u)
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

/* use the byte-aligned interp mechanism to code the entire integer buffer
*/
size_t interp_compress(
    uint8_t* obuff, /* output buffer */
    size_t osize, /* output buffer size */
    const uint32_t* ibuff, /* input buffer */
    size_t isize)
{ /* input buffer size */
    bit_stream os((uint32_t*)obuff,false);

    int i, p;
    uint64_t vp, vl, vr, v;
    size_t op = 0;

    std::vector<uint64_t> T(2 * isize - 1);

    for (i = 0; i < isize; i++) {
        T[i + isize - 1] = ibuff[i];
    }
    for (p = isize - 2; p >= 0; p--) {
        T[p] = T[2 * p + 1] + T[2 * p + 2] - 1;
    }
    write_center_mid(os,T[0],1ULL<<63);
    for (p = 0; p < isize - 1; p++) {
        vp = T[p];
        vl = T[2 * p + 1];
        vr = T[2 * p + 2];
        if (vl <= vr) {
            v = vr - vl + 1;
        } else {
            v = vl - vr;
        }
        // std::cout << "write " << v << " in " << vp << std::endl;
        write_center_mid(os,v,vp);
    }
    return os.flush();
}

/* decode an entire buffer of byte codes to regenerate the initial
   sequence of 32-bit integers that was the original input
*/
size_t interp_decompress(
    uint32_t* obuff, /* output buffer */
    size_t osize, /* number symbols to be decoded */
    const uint8_t* ibuff, /* input buffer */
    size_t isize)
{ /* size of input buffer */
    int i, p;
    uint64_t vp, vl, vr, v;
    size_t ip = 0;
    bit_stream is((uint32_t*)ibuff, false);

    std::vector<uint64_t> T(2 * osize - 1);
    T[0] = read_center_mid(is,1ULL<<63);
    for (p = 0; p < osize - 1; p++) {
        vp = T[p];
        v = read_center_mid(is,vp);
        if (((v + vp) & 1) == 0) {
            vl = 1 + ((vp - v) >> 1);
        } else {
            vl = (vp + v + 1) >> 1;
        }
        vr = vp - vl + 1;
        T[2 * p + 1] = vl;
        T[2 * p + 2] = vr;
    }
    for (i = 0; i < osize; i++) {
        obuff[i] = T[i + osize - 1];
    }
    return ip;
}

std::vector<uint32_t> create_clusted(size_t n,float small_geom_dist,float large_jump_prob,bool prefix_sum)
{
    std::vector<uint32_t> vec(n);
    std::mt19937 gen(0);
    std::geometric_distribution<> small_gap(small_geom_dist);
    std::normal_distribution<> large_gap(4096,4096);
    std::bernoulli_distribution large_jump(large_jump_prob);
    for(size_t i=0;i<n;i++) {
        bool do_large_jump = large_jump(gen);
        if(do_large_jump) {
            auto gap = large_gap(gen)+1;
            while(gap < 0) {
                gap = large_gap(gen)+1;
            }
            vec[i] = gap+1;
        } else {
            vec[i] = small_gap(gen)+1;
        }
    }
    if(prefix_sum) {
        for(size_t i=1;i<n;i++)
            vec[i] += vec[i-1];
        for(size_t i=1;i<n;i++) {
            if(vec[i-1] > vec[i]) {
                std::cerr << "NOT INCREASING!!! at " << i << " => " << vec[i-1] << " " << vec[i] <<  std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    return vec;
}

void print_vec(std::vector<uint32_t>& v)
{
    std::cout << "[";
    for(size_t i=0;i<v.size()-1;i++) std::cout << v[i] << ",";
    std::cout << v.back() << "]" << std::endl;
}


int main(int argc, char const* argv[])
{
    
    //print_vec(input_u32s);

    {
        std::vector<uint32_t> input_u32s = create_clusted(1000000,0.5,0.001,true);
        std::cout << "encoding " << input_u32s.size() << " increasing numbers in [" << input_u32s.front() << "," << input_u32s.back()+1 << "]" << std::endl;
        auto universe = input_u32s.back();
        std::vector<uint32_t> out_buf(input_u32s.size()*2);

        auto start_encode = std::chrono::high_resolution_clock::now();
        auto bytes_written = interpolative_internal::encode(out_buf.data(),input_u32s.data(),input_u32s.size(),universe);
        auto stop_encode = std::chrono::high_resolution_clock::now();
        auto encoding_time_ns = stop_encode - start_encode;
        double encode_IPS = compute_ips(input_u32s.size(),encoding_time_ns.count());

        std::vector<uint32_t> recover(input_u32s.size());
        auto start_decode = std::chrono::high_resolution_clock::now();
        interpolative_internal::decode(out_buf.data(),recover.data(),input_u32s.size(),universe);
        auto stop_decode = std::chrono::high_resolution_clock::now();
        auto decode_time_ns = stop_decode - start_decode;
        double decode_IPS = compute_ips(input_u32s.size(),decode_time_ns.count());

        std::cout << "RECURSIVE BPI = " << double(bytes_written*8) / double(input_u32s.size()) << " ENCODE MIPS = " << encode_IPS/1000000.0 << " DECODE MIPS = " << decode_IPS/1000000.0 <<std::endl;

        if(!REQUIRE_EQUAL(input_u32s.data(),recover.data(),input_u32s.size())) {
            std::cout << "DECODING ERROR!" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    {
        std::vector<uint32_t> input_u32s = create_clusted(100000000,0.5,0.001,false);
        std::vector<uint8_t> out_buf(input_u32s.size()*8);

        auto start_encode = std::chrono::high_resolution_clock::now();
        auto bytes_written = interp_compress(out_buf.data(),out_buf.size(),input_u32s.data(),input_u32s.size());
        auto stop_encode = std::chrono::high_resolution_clock::now();
        auto encoding_time_ns = stop_encode - start_encode;
        double encode_IPS = compute_ips(input_u32s.size(),encoding_time_ns.count());

        std::vector<uint32_t> recover(input_u32s.size());
        auto start_decode = std::chrono::high_resolution_clock::now();
        interp_decompress(recover.data(),recover.size(),out_buf.data(),bytes_written);
        auto stop_decode = std::chrono::high_resolution_clock::now();
        auto decode_time_ns = stop_decode - start_decode;
        double decode_IPS = compute_ips(input_u32s.size(),decode_time_ns.count());

        std::cout << "SEQ BPI = " << double(bytes_written*8) / double(input_u32s.size()) << " ENCODE MIPS = " << encode_IPS/1000000.0 << " DECODE MIPS = " << decode_IPS/1000000.0 <<std::endl;

        if(!REQUIRE_EQUAL(input_u32s.data(),recover.data(),input_u32s.size())) {
            std::cout << "DECODING ERROR!" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
