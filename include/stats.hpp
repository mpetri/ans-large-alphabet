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
}


