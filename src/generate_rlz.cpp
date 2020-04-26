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

#define ELPP_THREAD_SAFE
#define ELPP_STL_LOGGING

#include <iostream>
#include <random>
#include <vector>

#include "cutil.hpp"
#include "qsufsort.hpp"
#include "util.hpp"

#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

#include "collection.hpp"
#include "rlz_utils.hpp"
#include "utils.hpp"

#include "indexes.hpp"

#include "logging.hpp"
INITIALIZE_EASYLOGGINGPP

po::variables_map parse_cmdargs(int argc, char const* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "produce help message")
        ("input,i",po::value<std::string>()->required(), "input file")
        ("collection,c",po::value<std::string>()->required(), "tmp collection directory")
        ("dict,d",po::value<uint32_t>()->required(), "dict size")
        ("text,t", "text instead of uint32_t output")
        ("output,o",po::value<std::string>()->required(), "output prefix")
        ("maxint,m",po::value<uint32_t>(), "maximum number of integers to write");
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

void print_progress(double percent)
{
    uint32_t bar_width = 70;
    std::cout << "[";
    uint32_t pos = percent * bar_width;
    for (uint32_t i = 0; i < bar_width; i++) {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }
    std::cout << "] " << uint32_t(percent * 100) << " %\r";
    std::cout.flush();
}

void create_collection(std::string input_file, std::string col_dir)
{

    utils::create_directory(col_dir);
    std::string output_file = col_dir + "/" + KEY_PREFIX + KEY_TEXT;
    if (utils::file_exists(output_file)) {
        LOG(INFO) << "Collection already exists.";
        return;
    }
    {
        // auto out = sdsl::write_out_buffer<8>::create(output_file);
        sdsl::int_vector_buffer<8> out(
            output_file, std::ios::out, 128 * 1024 * 1024);
        if (!utils::file_exists(input_file)) {
            LOG(FATAL) << "Input file " << input_file << " does not exist.";
        }
        LOG(INFO) << "Processing " << input_file;
        sdsl::int_vector_buffer<8> input(
            input_file, std::ios::in, 128 * 1024 * 1024, 8, true);
        auto itr = input.begin();
        auto end = input.end();
        auto replaced_zeros = 0;
        auto replaced_ones = 0;
        size_t processed = 0;
        size_t total = std::distance(itr, end);
        size_t one_percent = total * 0.01;
        while (itr != end) {
            auto sym = *itr;
            if (sym == 0) {
                sym = 0xFE;
                replaced_zeros++;
            }
            if (sym == 1) {
                replaced_ones++;
                sym = 0xFF;
            }
            out.push_back(sym);
            ++itr;
            ++processed;
            if (processed % one_percent == 0) {
                print_progress(double(processed) / double(total));
            }
        }
        std::cout << "\n";
        LOG(INFO) << "Replaced zeros = " << replaced_zeros;
        LOG(INFO) << "Replaced ones = " << replaced_ones;
        LOG(INFO) << "Copied " << out.size() << " bytes.";
    }
}

int main(int argc, char const* argv[])
{
    setup_logger(argc, argv);

    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_file = cmdargs["input"].as<std::string>();
    auto col_dir = cmdargs["collection"].as<std::string>();
    auto output_prefix = cmdargs["output"].as<std::string>();
    auto dict_size_mb = cmdargs["dict"].as<uint32_t>();
    auto dict_size_bytes = dict_size_mb * 1024 * 1024;
    auto write_text = cmdargs.count("text") != 0;
    auto file_name = output_prefix + "-RLZ-D" + std::to_string(dict_size_mb);
    uint32_t max_int = std::numeric_limits<uint32_t>::max();
    if (cmdargs.count("maxint") != 0) {
        max_int = cmdargs["maxint"].as<uint32_t>();
    }

    /* parse the collection */
    create_collection(input_file, col_dir);
    std::cout << "Parsing collection directory " << col_dir << std::endl;
    collection col(col_dir);

    /* create rlz index */
    std::vector<uint32_t> lens;
    std::vector<uint32_t> offsets;
    const uint32_t factorization_blocksize = 64 * 1024;
    {
        auto rlz_store = rlz_store_static<
            dict_uniform_sample_budget<default_dict_sample_block_size>,
            dict_prune_none, dict_index_csa<>, factorization_blocksize, false,
            factor_select_first,
            factor_coder_blocked<3, coder::zlib<9>, coder::zlib<9>,
                coder::zlib<9>>,
            block_map_uncompressed>::builder {}
                             .set_threads(16)
                             .set_dict_size(dict_size_bytes)
                             .build_or_load(col);

        auto fitr = rlz_store.factors_begin();
        auto fend = rlz_store.factors_end();
        while (fitr != fend) {
            auto cur_factor = *fitr;
            if (!cur_factor.is_literal) {
                lens.push_back(cur_factor.len);
                offsets.push_back(cur_factor.offset);
                if (lens.size() >= max_int)
                    break;
            }
            ++fitr;
        }
    }

    {
        timer t("(5) write RLZ file");
        if (write_text) {

            write_file_text(lens, file_name + "-FLENS.txt");
            write_file_text(offsets, file_name + "-FOFFSETS.txt");
        } else {
            write_file_u32(lens, file_name + "-FLENS.u32");
            write_file_u32(offsets, file_name + "-FOFFSETS.u32");
        }
    }
}
