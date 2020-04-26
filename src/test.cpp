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


#include <cstdint>
#include <iostream>

std::pair<uint32_t,uint32_t> ans_fold_mapping(uint32_t x,uint32_t fidelity,uint32_t radix)
{
    uint32_t radix_mask = ((1<<radix)-1);
    size_t offset = 0;
    size_t thres = 1 << (fidelity+radix-1);
    uint32_t out_bytes = 0;
    while(x >= thres) {
        auto digit = x & radix_mask;
        out_bytes++;
        x = x >> radix;
        offset = offset + (1<<(fidelity-1)) * radix_mask;
    }
    return {x + offset,out_bytes};
}

std::pair<uint32_t,uint32_t> ans_fold_undo_mapping(uint32_t x_plus_offset,uint32_t fidelity,uint32_t radix)
{
    uint32_t div = (1<<(fidelity-1)) * ((1<<radix)-1);
    size_t thres = 1 << (fidelity+radix-1);
    if(x_plus_offset < thres)
        return {x_plus_offset,0};
    auto output_bytes = (x_plus_offset - thres) / div + 1;
    auto x_org = x_plus_offset - (div*output_bytes);
    return {x_org<<(radix*output_bytes),output_bytes};
}

int main() {
    uint32_t prev = -1;
    uint32_t fidelity = 2;
    uint32_t radix = 8;
    for(uint32_t x=0;x<std::numeric_limits<uint32_t>::max();x++) {
        auto [mapped,out_bytes] = ans_fold_mapping(x,fidelity,radix);
        auto [unmapped,unmapped_bytes] = ans_fold_undo_mapping(mapped,fidelity,radix);
        auto masked_x = x >> (out_bytes*radix);
        masked_x = masked_x << (out_bytes*radix);
        if(masked_x != unmapped) {
            std::cout << "x = " << x << " out_bytes = " << out_bytes
                << " masked_x = " << masked_x << " unmapped = " << unmapped << std::endl;
        }
        if(out_bytes != prev || x == std::numeric_limits<uint32_t>::max()-1) {
            std::cout << x << " -> " << mapped << " ["<<  out_bytes<< "] (" << unmapped_bytes << " output bytes)" << std::endl;
        }
        prev = out_bytes;
    }
}