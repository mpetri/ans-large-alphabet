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
#include <random>

#include "cutil.hpp"
#include "util.hpp"
#include "zipf_dist.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

po::variables_map parse_cmdargs(int argc, char const* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "produce help message")
        ("text,t", "text (default is uint32_t binary)")
        ("num,n",po::value<size_t>()->required(), "number of integers per file")
        ("output,o",po::value<std::string>()->required(), "output path");
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

template<class t_dist>
void generate_data(const t_dist& dist,size_t n,std::string out_file,bool write_text)
{
    std::string file_name = out_file;
    std::cout << "generating file " << file_name << std::endl;
    std::vector<uint32_t> nums;
    std::mt19937 gen(0);
    t_dist dst = dist;
    for(size_t i=0;i<n;i++) {
        nums.push_back(dst(gen));
    }
    if(write_text) write_file_text(nums,file_name + ".txt");
    else write_file_u32(nums,file_name + ".u32");
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto output_path = cmdargs["output"].as<std::string>();
    auto n = cmdargs["num"].as<size_t>();
    auto write_text = cmdargs.count("text") != 0;

    generate_data(std::uniform_int_distribution<uint32_t>(0,(1<<8) - 1),n,output_path + "/uniform08",write_text);
    generate_data(std::uniform_int_distribution<uint32_t>(0,(1<<12) - 1),n,output_path + "/uniform12",write_text);
    generate_data(std::uniform_int_distribution<uint32_t>(0,(1<<16) - 1),n,output_path + "/uniform16",write_text);
    generate_data(std::uniform_int_distribution<uint32_t>(0,(1<<20) - 1),n,output_path + "/uniform20",write_text);

    generate_data(std::geometric_distribution<uint32_t>(0.01),n,output_path + "/geom0.01",write_text);
    generate_data(std::geometric_distribution<uint32_t>(0.1),n,output_path + "/geom0.1",write_text);
    generate_data(std::geometric_distribution<uint32_t>(0.2),n,output_path + "/geom0.2",write_text);
    generate_data(std::geometric_distribution<uint32_t>(0.4),n,output_path + "/geom0.4",write_text);
    generate_data(std::geometric_distribution<uint32_t>(0.6),n,output_path + "/geom0.6",write_text);
    generate_data(std::geometric_distribution<uint32_t>(0.8),n,output_path + "/geom0.8",write_text);
    generate_data(std::geometric_distribution<uint32_t>(0.9),n,output_path + "/geom0.9",write_text);
    generate_data(std::geometric_distribution<uint32_t>(0.99),n,output_path + "/geom0.99",write_text);


    generate_data(zipf_distribution<uint32_t>(1<<12),n,output_path + "/zipf12",write_text);
    generate_data(zipf_distribution<uint32_t>(1<<20),n,output_path + "/zipf20",write_text);

}

