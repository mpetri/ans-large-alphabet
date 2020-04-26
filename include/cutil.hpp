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

template <typename t_a, typename t_b>
void REQUIRE_EQUAL(const t_a& a, const t_b& b, std::string name)
{
    if (a != b) {
        quit("%s not equal.", name.c_str());
    }
}

void REQUIRE_EQUAL(
    const uint32_t* a, const uint32_t* b, size_t n, std::string name)
{
    int errors = 0;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            errors++;
            if(errors == 5) {
                quit("%s not equal at position %lu/%lu -> expected=%u is=%u",
                name.c_str(), i, n - 1, a[i], b[i]);
            } else {
                fprintf(stderr,"%s not equal at position %lu/%lu -> expected=%u is=%u\n",
                name.c_str(), i, n - 1, a[i], b[i]);
            }
        }
    }
    if(errors != 0) {
        quit("NOT EQUAL!");
    }
}

bool REQUIRE_EQUAL(
    const uint32_t* a, const uint32_t* b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}
