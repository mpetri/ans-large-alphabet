#include <iostream>
#include <vector>

#include "cutil.hpp"
#include "util.hpp"
#include "shuff.hpp"


int main(int argc, char const* argv[])
{
    auto file_name = argv[1];
    std::vector<uint32_t> input_u32s;
    input_u32s = read_file_text(file_name);

    std::vector<uint8_t> output_u8(input_u32s.size()*8);
    std::cout << "input size = " << input_u32s.size() << std::endl;

    auto written_bytes = shuff_compress(output_u8.data(),output_u8.size(),input_u32s.data(),input_u32s.size());
    std::cout << "written_bytes = " << written_bytes << std::endl;

    std::cout << double(written_bytes*8) / double(input_u32s.size()) << " BPI" << std::endl;

    std::vector<uint32_t> recover(input_u32s.size());
    shuff_decompress(recover.data(),input_u32s.size(),output_u8.data(),written_bytes);

    REQUIRE_EQUAL(input_u32s.data(),recover.data(),input_u32s.size(),"shuff");

    return EXIT_SUCCESS;
}
