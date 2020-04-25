#include <iostream>
#include <vector>

#include "cutil.hpp"
#include "util.hpp"
#include "methods.hpp"

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

const int NUM_RUNS = 3;

po::variables_map parse_cmdargs(int argc, char const* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "produce help message")
        ("text,t", "text input (default is uint32_t binary)")
        ("input,i",po::value<std::string>()->required(), "the input dir");
    // clang-format on
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            exit(EXIT_SUCCESS);
        }
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc;
        std::cerr << "Missing required option: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch (po::error& e) {
        std::cout << desc;
        std::cerr << "Error parsing cmdargs: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    return vm;
}

template <class t_compressor>
void run(std::vector<std::vector<uint32_t>>& inputs,std::vector<std::string>& input_names,std::vector<size_t>& block_sizes)
{
    for(size_t i=0;i<inputs.size();i++) {
        const auto& input = inputs[i];
        const auto& input_name = input_names[i];
        std::vector<uint8_t> encoded_data(input.size()*16);
        std::vector<uint8_t> tmp_buf(input.size()*16);

        uint32_t max_sym = *std::max_element(input.begin(),input.end());

        std::vector<double> BPIs;
        for(auto block_size : block_sizes) {
            size_t num_blocks = input.size() / block_size;
            size_t last_block = input.size() % block_size;
            auto in_ptr = input.data();
            auto enc_ptr = encoded_data.data();
            size_t enc_size = encoded_data.size();
            size_t total_enc_bytes = 0;

            std::vector<uint32_t> remapped_block_data(block_size);
            std::vector<uint32_t> remapped_alphabet(max_sym+1,0);
            std::vector<uint32_t> block_alphabet;
            for(size_t j=0;j<num_blocks;j++) {
                for(size_t k=0;k<remapped_alphabet.size();k++) remapped_alphabet[k] = 0;

                for(size_t k=0;k<block_size;k++) {
                    remapped_alphabet[in_ptr[k]] = 1;
                }
                if(remapped_alphabet[0] == 1) block_alphabet.push_back(0);
                for(size_t k=1;k<=max_sym;k++) {
                    if(remapped_alphabet[k] == 1) block_alphabet.push_back(k);
                    remapped_alphabet[k] += remapped_alphabet[k-1];
                }

                for(size_t k=0;k<block_size;k++) {
                    remapped_block_data[k] = remapped_alphabet[in_ptr[k]];
                }

                for(size_t k=1;k<block_alphabet.size();k++) {
                    block_alphabet[k] += block_alphabet[k-1];
                }


                uint32_t* enc_ptr_u32 = (uint32_t*) enc_ptr;
                *enc_ptr_u32++ = block_alphabet.size();
                *enc_ptr_u32++ = block_alphabet.back()+1;
                auto bytes_written = interpolative_internal::encode(enc_ptr_u32,block_alphabet.data(),block_alphabet.size(),block_alphabet.back()+1);
                enc_ptr+= (bytes_written + 8);
                enc_size -=  (bytes_written + 8);

                if(block_alphabet.size() != 1) {
                    auto encoded_bytes = t_compressor::encode(remapped_block_data.data(),block_size,enc_ptr,enc_size,tmp_buf.data());
                    total_enc_bytes += encoded_bytes;
                    enc_size -= encoded_bytes;
                    enc_ptr += encoded_bytes;
                }
                in_ptr += block_size;
                block_alphabet.clear();
            }
            if(last_block) {

                std::vector<uint32_t> block_alphabet;

                std::vector<uint32_t> remapped_alphabet(max_sym+1,0);
                for(size_t k=0;k<last_block;k++) {
                    remapped_alphabet[in_ptr[k]] = 1;
                }
                if(remapped_alphabet[0] == 1) block_alphabet.push_back(0);
                for(size_t k=1;k<=max_sym;k++) {
                    if(remapped_alphabet[k] == 1) block_alphabet.push_back(k);
                    remapped_alphabet[k] += remapped_alphabet[k-1];
                }

                for(size_t k=0;k<last_block;k++) {
                    remapped_block_data[k] = remapped_alphabet[in_ptr[k]];
                }

                for(size_t k=1;k<block_alphabet.size();k++) {
                    block_alphabet[k] += block_alphabet[k-1];
                }

                uint32_t* enc_ptr_u32 = (uint32_t*) enc_ptr;
                *enc_ptr_u32++ = block_alphabet.size();
                *enc_ptr_u32++ = block_alphabet.back()+1;


                auto bytes_written = interpolative_internal::encode(enc_ptr_u32,block_alphabet.data(),block_alphabet.size(),block_alphabet.back()+1);
                enc_ptr+= (bytes_written + 8);
                total_enc_bytes += (bytes_written + 8);
                if(block_alphabet.size() != 1) {
                    auto encoded_bytes = t_compressor::encode(remapped_block_data.data(),last_block,enc_ptr,enc_size,tmp_buf.data());
                    total_enc_bytes += encoded_bytes;
                }
            }

            double BPI = double(total_enc_bytes*8) / double(input.size());
            std::cout << input_name << ";"
                      << t_compressor::name() << ";"
                      << block_size << ";"
                      << BPI << std::endl;
        }
    }
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_dir = cmdargs["input"].as<std::string>();

    boost::regex input_file_filter( ".*\\.u32" );
    if (cmdargs.count("text")) {
        input_file_filter = boost::regex( ".*\\.txt" );
    }

    std::vector<std::string> input_files;

    // single file also works!
    boost::filesystem::path p(input_dir);
    if (boost::filesystem::is_regular_file(p)) {
        input_file_filter = boost::regex( p.filename().string() );
        input_dir = p.parent_path().string();
    }

    boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
    for( boost::filesystem::directory_iterator i( input_dir ); i != end_itr; ++i )
    {
        if( !boost::filesystem::is_regular_file( i->status() ) ) continue;
        boost::smatch what;
        if( !boost::regex_match( i->path().filename().string(), what, input_file_filter ) ) continue;

        std::string file_name = i->path().string();
        input_files.push_back(file_name);
    }

    std::sort(input_files.begin(),input_files.end());

    std::vector<std::vector<uint32_t>> inputs;
    for(auto input_file : input_files) {
        std::vector<uint32_t> input;
        if(cmdargs.count("text")) input = read_file_text(input_file);
        else input = read_file_u32(input_file);
        inputs.push_back(input);
    }

    size_t cur = 128;
    std::vector<size_t> block_sizes;
    for(size_t i=0;i<21;i++) {
        block_sizes.push_back(cur);
        cur *= 2;
    }

    // run<shuff>(inputs,input_files,block_sizes);
    // run<vbytefse>(inputs,input_files,block_sizes);
    // run<vbytehuffzero>(inputs,input_files,block_sizes);
    run<ANSint>(inputs,input_files,block_sizes);
    run<ANSmsb>(inputs,input_files,block_sizes);

    return EXIT_SUCCESS;
}
