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
#include <deque>

#include "qsufsort.hpp"
#include "cutil.hpp"
#include "util.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split

namespace po = boost::program_options;
namespace fs = boost::filesystem;

po::variables_map parse_cmdargs(int argc, char const* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "produce help message")
        ("num,n",po::value<size_t>()->required(), "number of integers per file")
        ("input,i",po::value<std::string>()->required(), "input file")
        ("text,t", "text instead of uint32_t output")
        ("word,w", "word instead of bytes")
        ("output,o",po::value<std::string>()->required(), "output prefix");
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

std::vector<int>
word_parse(std::string input_file,size_t n)
{
    std::vector<int> T;
    auto file_content_u8 = read_file_u8(input_file);
    for(size_t i=0;i<file_content_u8.size();i++) file_content_u8[i] = std::tolower(file_content_u8[i]);
    std::string_view content_str((const char*)file_content_u8.data(), file_content_u8.size());
    std::vector<std::string> words;
    boost::split(words,content_str, boost::is_any_of(";, \n.?'()-\""), boost::token_compress_on);
    std::unordered_map<std::string,uint32_t> str2id;
    std::locale loc;
    for(size_t i=0;i<words.size();i++) {
        const std::string& cur_word = words[i];
        auto itr = str2id.find(cur_word);
        if(itr != str2id.end()) {
            T.push_back(itr->second);
        } else {
            T.push_back(str2id.size()+1);
            str2id[cur_word] = str2id.size()+1;
        }
        if(T.size() >= n)
            break;
    }
    T.push_back(0);
    return T;
}

std::vector<int>
byte_parse(std::string input_file,size_t n)
{
    std::vector<int> T;
    auto file_content_u8 = read_file_u8(input_file);
    size_t size = file_content_u8.size()+1;
    if(size > n+1) size = n+1;
    T.resize(n+1);
    T[T.size()-1] = 0;
    for(size_t i=0;i<n;i++) T[i] = file_content_u8[i];
    return T;
}

uint32_t
get_mtf_rank(std::deque<int>& alphabet,int sym)
{
    auto itr = std::find(std::begin(alphabet), std::end(alphabet), sym);
    uint32_t rank = (uint32_t) std::distance(std::begin(alphabet),itr);
    alphabet.erase(itr);
    alphabet.push_front(sym);
    return rank;
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_file = cmdargs["input"].as<std::string>();
    auto output_prefix = cmdargs["output"].as<std::string>();
    auto write_text = cmdargs.count("text") != 0;
    auto words = cmdargs.count("word") != 0;
    auto n = cmdargs["num"].as<size_t>();
    auto file_name = output_prefix;

    std::vector<int> T;
    {
        timer t("(1) parsing input");
        if(words) {
            file_name += "-WORD";
            T = word_parse(input_file,n);
        } else {
            file_name += "-CHAR";
            T = byte_parse(input_file,n);
        }
    }

    std::vector<int> text = T;
    std::vector<int> SA(T.size());
    const auto [min, max] = std::minmax_element(T.begin(),T.end()-1);
    {
        std::cout << "text size = " << T.size() << " min_sym = " << *min << " max_sym = " << *max << std::endl;
        timer t("(2) compute SA");
        suffixsort(T.data(),SA.data(),T.size()-1,*max+1,*min);
    }

    std::vector<int> BWT(T.size());
    {
        timer t("(3) compute BWT");
        for(size_t i=0;i<text.size();i++) {
            BWT[i] = SA[i] != 0 ? text[SA[i]-1] : text.back();
        }
    }
    SA.resize(0);
    size_t seq_len = text.size()-1;
    if(seq_len > n) seq_len = n;

    std::vector<uint32_t> MTF(seq_len);
    {
        timer t("(4) compute MTF");
        std::deque<int> alphabet;
        for(size_t i=0;i<=*max;i++) alphabet.push_back(i);
        for(size_t i=0;i<seq_len;i++) {
            auto sym = BWT[i];
            MTF[i] = get_mtf_rank(alphabet,sym);
        }
    }

    {

        std::vector<uint32_t> text_u32(seq_len);
        for(size_t i=0;i<seq_len;i++) text_u32[i] = text[i];
        timer t("(5) write WORD file");
        if(write_text) write_file_text(text_u32,file_name + ".txt");
        else write_file_u32(text_u32,file_name + ".u32");
    }
    {
        timer t("(6) write MTF file");
        if(write_text) write_file_text(MTF,file_name + "-BWTMTF.txt");
        else write_file_u32(MTF,file_name + "-BWTMTF.u32");
    }
}

