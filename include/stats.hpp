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


struct comp_stats_t {
    size_t prelude_bytes = 0;
    size_t encode_bytes = 0;
    size_t prelude_time_ns = 0;
    size_t encode_time_ns = 0;
};

comp_stats_t& get_stats() {
    static comp_stats_t s;
    return s;
}

comp_stats_t reset_stats() {
    auto& s = get_stats();
    s.prelude_bytes = 0;
    s.encode_bytes = 0;
    s.prelude_time_ns = 0;
    s.encode_time_ns = 0;
    return s;
}


