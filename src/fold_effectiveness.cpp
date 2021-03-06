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
#include "methods.hpp"
#include "util.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

const int NUM_RUNS = 1;

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
void run(std::vector<uint32_t>& input, std::string input_name)
{
    // (0) compute entropy
    auto [input_entropy, sigma] = compute_entropy(input);

    // (1) encode
    std::vector<uint8_t> encoded_data(input.size() * 8);
    std::vector<uint8_t> tmp_buf(input.size() * 8);

    size_t encoded_bytes;
    size_t encoding_time_ns_min = std::numeric_limits<size_t>::max();
    for (int i = 0; i < NUM_RUNS; i++) {
        auto start_encode = std::chrono::high_resolution_clock::now();
        encoded_bytes = t_compressor::encode(input.data(), input.size(),
            encoded_data.data(), encoded_data.size(), tmp_buf.data());
        auto stop_encode = std::chrono::high_resolution_clock::now();
        auto encoding_time_ns = stop_encode - start_encode;
        encoding_time_ns_min
            = std::min((size_t)encoding_time_ns.count(), encoding_time_ns_min);
    }

    double BPI = double(encoded_bytes * 8) / double(input.size());
    double encode_IPS = compute_ips(input.size(), encoding_time_ns_min);
    double enc_ns_per_int = double(encoding_time_ns_min) / double(input.size());

    // (4) output stats
    printf("(%s,%2.4f)\n", t_compressor::name().c_str(), BPI);
    fflush(stdout);
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_dir = cmdargs["input"].as<std::string>();

    boost::regex input_file_filter(".*\\.u32");
    if (cmdargs.count("text")) {
        input_file_filter = boost::regex(".*\\.txt");
    }

    // single file also works!
    boost::filesystem::path p(input_dir);
    if (boost::filesystem::is_regular_file(p)) {
        input_file_filter = boost::regex(p.filename().string());
        input_dir = p.parent_path().string();
    }

    boost::filesystem::directory_iterator
        end_itr; // Default ctor yields past-the-end
    for (boost::filesystem::directory_iterator i(input_dir); i != end_itr;
         ++i) {
        if (!boost::filesystem::is_regular_file(i->status()))
            continue;
        boost::smatch what;
        if (!boost::regex_match(
                i->path().filename().string(), what, input_file_filter))
            continue;

        std::string file_name = i->path().string();
        std::vector<uint32_t> input_u32s;
        if (cmdargs.count("text")) {
            input_u32s = read_file_text(file_name);
        } else {
            input_u32s = read_file_u32(file_name);
        }
        std::string short_name = i->path().stem().string();

        run<ANSfold<1>>(input_u32s, short_name);
        run<ANSfold<2>>(input_u32s, short_name);
        run<ANSfold<3>>(input_u32s, short_name);
        run<ANSfold<4>>(input_u32s, short_name);
        run<ANSfold<5>>(input_u32s, short_name);
        run<ANSfold<6>>(input_u32s, short_name);
        run<ANSfold<7>>(input_u32s, short_name);
        run<ANSfold<8>>(input_u32s, short_name);

        run<ANSrfold<1>>(input_u32s, short_name);
        run<ANSrfold<2>>(input_u32s, short_name);
        run<ANSrfold<3>>(input_u32s, short_name);
        run<ANSrfold<4>>(input_u32s, short_name);
        run<ANSrfold<5>>(input_u32s, short_name);
        run<ANSrfold<6>>(input_u32s, short_name);
        run<ANSrfold<7>>(input_u32s, short_name);
        run<ANSrfold<8>>(input_u32s, short_name);
    }

    return EXIT_SUCCESS;
}
