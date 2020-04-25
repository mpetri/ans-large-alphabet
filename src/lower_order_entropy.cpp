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
void run(std::vector<uint32_t>& input,std::string input_name)
{
    // (0) compute entropy
    auto [input_entropy,sigma] = compute_entropy(input);


    // (1) encode
    std::vector<uint8_t> encoded_data(input.size()*8);
    std::vector<uint8_t> tmp_buf(input.size()*8);

    size_t encoded_bytes;
    size_t encoding_time_ns_min = std::numeric_limits<size_t>::max();
    for(int i=0;i<NUM_RUNS;i++) {
        auto start_encode = std::chrono::high_resolution_clock::now();
        encoded_bytes = t_compressor::encode(input.data(),input.size(),encoded_data.data(),encoded_data.size(),tmp_buf.data());
        auto stop_encode = std::chrono::high_resolution_clock::now();
        auto encoding_time_ns = stop_encode - start_encode;
        encoding_time_ns_min = std::min((size_t)encoding_time_ns.count(),encoding_time_ns_min);
    }

    double BPI = double(encoded_bytes*8) / double(input.size());
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

    // (4) output stats
    printf("%25.25s\t\t%15u\t\t%15u\t\t%18.18s\t\t%2.4f\t\t%2.4f\t\t%10.0f\t\t%10.0f\t\t\n",
            input_name.c_str(),
            (uint32_t) input.size(),
            (uint32_t) sigma,
            t_compressor::name().c_str(),
            input_entropy,
            BPI,
            encode_IPS,
            decode_IPS);
    fflush(stdout);
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_dir = cmdargs["input"].as<std::string>();

    boost::regex input_file_filter( ".*\\.u32" );
    if (cmdargs.count("text")) {
        input_file_filter = boost::regex( ".*\\.txt" );
    }

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
        std::vector<uint32_t> input_u32s;
        if (cmdargs.count("text")) {
            input_u32s = read_file_text(file_name);
        } else {
            input_u32s = read_file_u32(file_name);
        }
        std::string short_name = i->path().stem().string();

        for(size_t k=1;k<=32;k++) {
            std::vector<uint32_t> vec;

            for(size_t i=0;i<input_u32s.size();i++) {
                vec.push_back(input_u32s[i] & ((1ULL<<k)-1));
            }

            auto [input_entropy,sigma] = compute_entropy(vec);
            printf("%s;%u;%u;%2.4f\n",short_name.c_str(),(unsigned int)sigma,(unsigned int)k,input_entropy);
        }
    }

    return EXIT_SUCCESS;
}
