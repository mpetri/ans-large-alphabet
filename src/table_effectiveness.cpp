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
void run(std::vector<std::vector<uint32_t>>& inputs)
{
    std::cout << t_compressor::name() << "  &" << std::endl;

    std::vector<double> BPIs;
    for(const auto& input : inputs) {
        std::vector<uint8_t> encoded_data(input.size()*8);
        std::vector<uint8_t> tmp_buf(input.size()*8);
        auto encoded_bytes = t_compressor::encode(input.data(),input.size(),encoded_data.data(),encoded_data.size(),tmp_buf.data());
        double BPI = double(encoded_bytes*8) / double(input.size());
        BPIs.push_back(BPI);
    }

    for(size_t i=0;i<BPIs.size();i++) {
        for(size_t j=0;j<i*4;j++) printf(" ");
        printf("%2.4f  ",BPIs[i]);
        if(i+1==BPIs.size()) printf("\\\\ \n");
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

    // run<entropy_only>(inputs);
    run<shuff>(inputs);
    run<arith>(inputs);
    // run<huffzero>(inputs);
    // run<fse>(inputs);
    // run<vbyte>(inputs);
    // run<optpfor<128>>(inputs);
    // run<vbytefse>(inputs);
    // run<vbytehuffzero>(inputs);
    // run<vbyteANS>(inputs);
    // run<ANSint>(inputs);
    // run<ANSmsb>(inputs);

    return EXIT_SUCCESS;
}
