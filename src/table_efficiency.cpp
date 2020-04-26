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

const int NUM_RUNS = 5;

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
void run(std::vector<std::vector<uint32_t>>& inputs)
{
    std::cout << "\\method{" << t_compressor::name() << "}  &" << std::endl;

    std::vector<double> enc_speed;
    std::vector<double> dec_speed;
    for(const auto& input : inputs) {
        // (1) encode
        std::vector<uint8_t> encoded_data(input.size()*8);
        std::vector<uint8_t> tmp_buf(input.size()*8);

        size_t encoded_bytes = 0;
        size_t encoding_time_ns_min = std::numeric_limits<size_t>::max();
        for(int i=0;i<NUM_RUNS;i++) {
            auto start_encode = std::chrono::high_resolution_clock::now();
            encoded_bytes = t_compressor::encode(input.data(),input.size(),encoded_data.data(),encoded_data.size(),tmp_buf.data());
            auto stop_encode = std::chrono::high_resolution_clock::now();
            auto encoding_time_ns = stop_encode - start_encode;
            encoding_time_ns_min = std::min((size_t)encoding_time_ns.count(),encoding_time_ns_min);
        }
        double encode_IPS = compute_ips(input.size(),encoding_time_ns_min);

        // (2) decode
        encoded_data.resize(encoded_bytes);
        std::vector<uint32_t> recover(input.size());
        size_t decode_time_ns_min = std::numeric_limits<size_t>::max();
        for(int i=0;i<NUM_RUNS;i++) {
            auto start_decode = std::chrono::high_resolution_clock::now();
            t_compressor::decode(encoded_data.data(),encoded_data.size(),recover.data(),recover.size(),tmp_buf.data());
            auto stop_decode = std::chrono::high_resolution_clock::now();
            auto decode_time_ns = stop_decode - start_decode;
            decode_time_ns_min = std::min((size_t)decode_time_ns.count(),decode_time_ns_min);
        }
        double decode_IPS = compute_ips(input.size(),decode_time_ns_min);

        // (3) verify
        REQUIRE_EQUAL(input.data(),recover.data(),input.size(),t_compressor::name());

        enc_speed.push_back(encode_IPS);
        dec_speed.push_back(decode_IPS);
    }

    for(size_t i=0;i<enc_speed.size();i++) {
        for(size_t j=0;j<i*4;j++) printf(" ");
        printf("%15.4f  &  %15.4f  ",enc_speed[i],dec_speed[i]);
        if(i+1==enc_speed.size()) printf("\\\\ \n\n");
        else printf("&\n");
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

    run<vbyte>(inputs);
    run<vbytehuffzero>(inputs);
    run<vbytefse>(inputs);
    run<optpfor<128>>(inputs);
    run<shuff>(inputs);
    run<arith>(inputs);
    run<ANSint>(inputs);
    run<ANSfold<1>>(inputs);
    run<ANSfold<5>>(inputs);
    run<ANSrfold<1>>(inputs);
    run<ANSrfold<5>>(inputs);

    return EXIT_SUCCESS;
}
