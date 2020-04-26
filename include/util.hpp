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


#include <chrono>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <numeric>

#include "constants.hpp"

using namespace std::chrono;

inline void* align1(
    size_t __align, size_t __size, void*& __ptr, size_t& __space) noexcept
{
    const auto __intptr = reinterpret_cast<uintptr_t>(__ptr);
    const auto __aligned = (__intptr - 1u + __align) & -__align;
    const auto __diff = __aligned - __intptr;
    if ((__size + __diff) > __space)
        return nullptr;
    else {
        __space -= __diff;
        return __ptr = reinterpret_cast<void*>(__aligned);
    }
}

inline void* aligned_alloc(std::size_t alignment, std::size_t size)
{
    if (alignment < std::alignment_of<void*>::value) {
        alignment = std::alignment_of<void*>::value;
    }
    std::size_t n = size + alignment - 1;
    void* p1 = 0;
    void* p2 = std::malloc(n + sizeof p1);
    if (p2) {
        p1 = static_cast<char*>(p2) + sizeof p1;
        (void)align1(alignment, size, p1, n);
        *(static_cast<void**>(p1) - 1) = p2;
    }
    return p1;
}

inline void aligned_free(void* ptr)
{
    if (ptr) {
        void* p = *(static_cast<void**>(ptr) - 1);
        std::free(p);
    }
}

struct timer {
    high_resolution_clock::time_point start;
    std::string name;
    timer(const std::string& _n)
        : name(_n)
    {
        std::cerr << "START => " << name << std::endl;
        start = high_resolution_clock::now();
    }
    ~timer()
    {
        auto stop = high_resolution_clock::now();
        std::cerr << "STOP => " << name << " - "
                  << duration_cast<milliseconds>(stop - start).count() / 1000.0f
                  << " sec" << std::endl;
    }
};

int fprintff(FILE* f, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfprintf(f, format, args);
    va_end(args);
    fflush(f);
    return ret;
}

void quit(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);
    va_end(args);
    if (errno != 0) {
        fprintf(stderr, ": %s\n", strerror(errno));
    } else {
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    exit(EXIT_FAILURE);
}

void output_list_to_stdout(uint32_t* list, uint32_t n)
{
    printf("%u\n", n);
    for (uint32_t j = 0; j < n; j++) {
        printf("%u\n", list[j]);
    }
}

FILE* fopen_or_fail(std::string file_name, const char* mode)
{
    FILE* out_file = fopen(file_name.c_str(), mode);
    if (!out_file) {
        quit("opening output file %s failed", file_name.c_str());
    }
    return out_file;
}

void fclose_or_fail(FILE* f)
{
    int ret = fclose(f);
    if (ret != 0) {
        quit("closing file failed");
    }
}

std::vector<uint8_t> read_file_u8(std::string file_name)
{
    std::vector<uint8_t> content;
    auto f = fopen_or_fail(file_name, "rb");
    auto cur = ftell(f);
    fseek(f, 0, SEEK_END);
    auto end = ftell(f);
    size_t file_size = (end - cur);
    content.resize(file_size);
    fseek(f, cur, SEEK_SET);
    size_t ret = fread(content.data(), sizeof(uint8_t), file_size, f);
    if (ret != file_size) {
        quit("reading file content failed: %d", ret);
    }
    fclose_or_fail(f);
    return content;
}


std::vector<uint32_t> read_file_text(std::string file_name)
{
    std::vector<uint32_t> content;
    auto fd = fopen_or_fail(file_name, "r");
    uint32_t num;
    while (fscanf(fd, "%u\n", &num) == 1) {
        content.push_back(num);
    }
    fclose_or_fail(fd);
    return content;
}

std::vector<uint32_t> read_file_u32(std::string file_name)
{
    std::vector<uint32_t> content;
    auto f = fopen_or_fail(file_name, "rb");
    auto cur = ftell(f);
    fseek(f, 0, SEEK_END);
    auto end = ftell(f);
    size_t file_size = (end - cur);
    size_t file_size_u32 = file_size / sizeof(uint32_t);
    if (file_size % sizeof(uint32_t) != 0) {
        quit("reading file content failed: file size % 32bit != 0");
    }
    content.resize(file_size_u32);
    fseek(f, cur, SEEK_SET);
    size_t ret = fread(content.data(), sizeof(uint32_t), file_size_u32, f);
    if (ret != file_size_u32) {
        quit("reading file content failed: %d", ret);
    }
    fclose_or_fail(f);
    return content;
}

void compact_alphabet(std::vector<uint32_t>& vec)
{
    uint32_t max_elem = *std::max_element(vec.begin(), vec.end());
    std::vector<uint8_t> elems(max_elem+1);
    for(const auto& v : vec) elems[v] = 1;
    uint32_t cur = 0;
    for(auto& v : elems) {
        if(v != 0) {
            v = cur;
            cur++;
        }
    }
    for(auto& x : vec) x = elems[x];
}

void probability_sort(std::vector<uint32_t>& vec)
{
    uint32_t max_elem = *std::max_element(vec.begin(), vec.end());
    std::vector<std::pair<int64_t,uint64_t>> elems(max_elem+1);
    for(size_t i=0;i<elems.size();i++) {
        elems[i].second = i;
        elems[i].first = 0;
    }
    for(const auto& v : vec) elems[v].first--;
    std::sort(elems.begin(),elems.end());
    std::vector<uint32_t> remap(max_elem+1);
    for(size_t i=0;i<elems.size();i++) {
        remap[elems[i].second] = i;
    }
    for(auto& x : vec) x = remap[x]+1;
}

void
write_file_text(std::vector<uint32_t>& nums,std::string file_name)
{
    std::vector<uint32_t> content;
    auto fd = fopen_or_fail(file_name, "w");
    for(auto& num : nums) {
        fprintf(fd,"%u\n",num);
    }
    fclose_or_fail(fd);
}

void
write_file_u32(std::vector<uint32_t>& nums,std::string file_name)
{
    auto f = fopen_or_fail(file_name, "wb");
    auto ret = fwrite(nums.data(),sizeof(uint32_t),nums.size(),f);
    if (ret != nums.size()) {
        quit("reading file content failed: %d", ret);
    }
    fclose_or_fail(f);
}


std::pair<double,size_t> compute_entropy(const uint32_t* input,size_t n)
{
    std::unordered_map<uint32_t, uint64_t> freqs;
    for (size_t i=0;i<n;i++) {
        uint32_t num = input[i];
        freqs[num] += 1;
    }
    double H0 = 0.0;
    double nn = n;
    for (const auto& c : freqs) {
        double p = double(c.second) / nn;
        H0 += (p * log2(p));
    }
    return {-H0,freqs.size()};
}

std::pair<double,size_t> compute_entropy(std::vector<uint32_t>& input)
{
    return compute_entropy(input.data(),input.size());
}


template<class X>
double entropy(const std::vector<X>& freqs,size_t freqs_sum)
{
    double H0 = 0.0;
    double n = freqs_sum;
    for (const auto& f : freqs) {
        if(f != 0) {
            double p = double(f) / n;
            H0 += (p * log2(p));
        }
    }
    return -H0;
}

template<class X,class Y>
double cross_entropy(const std::vector<X>& P,const std::vector<Y>& Q)
{
    double H0 = 0.0;
    double n = std::accumulate(std::begin(P),std::end(P), 0);
    double m = std::accumulate(std::begin(Q),std::end(Q), 0);
    for (size_t i=0;i<P.size();i++) {
        if(P[i] != 0 && Q[i] != 0) {
            double p = double(P[i]) / n;
            double q = double(Q[i]) / m;
            H0 += (p * log2(q));
        }
    }
    return -H0;
}

double compute_mips(size_t num_ints,size_t num_ns)
{
    double million_ints = double(num_ints) / 1000000.0;
    double seconds = double(num_ns) / double(1e9);
    return million_ints / seconds;
}

double compute_ips(size_t num_ints,size_t num_ns)
{
    double seconds = double(num_ns) / double(1e9);
    return double(num_ints) / seconds;
}