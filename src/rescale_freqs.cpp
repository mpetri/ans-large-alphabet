#include <iostream>
#include <vector>
#include <algorithm>

#include "cutil.hpp"
#include "util.hpp"
#include "methods.hpp"

#include <boost/regex.hpp>
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

void rescale_freqs(std::vector<uint32_t>& scaled,const std::vector<uint64_t>& F,size_t M,size_t sigma,size_t freq_sum)
{
    double fratio = double(frame_size)/double(sigma);
    for(int64_t i=sigma-1;i>=0;i--) {
        double aratio = double(M)/double(freq_sum);
        double ratio  = i*fratio/sigma + (sigma-i)*aratio/sigma;
        S[i] = (uint32_t) (0.5 + ratio*F[i]);
        if(S[i]==0)
            S[i] = 1;
        M -= S[i];
        freq_sum -= F[i];
    }
}

void
rescale_freqs(std::vector<uint32_t>& in_u32,std::string name)
{
    size_t m = in_u32.size();

    // (0) compute entropy
    auto [input_entropy,sigma] = entropy(in_u32);

    uint32_t max_sym = 0;
    for(size_t i=0;i<m;i++) {
        max_sym = std::max(in_u32[i],max_sym);
    }
    std::vector<std::pair<int64_t,uint32_t>> freqs(max_sym+1,{0,0});
    for(size_t i=0;i<m;i++) {
        freqs[in_u32[i]].first--;
        freqs[in_u32[i]].second = in_u32[i];
    }

    std::sort(freqs.begin(),freqs.end());


    std::vector<uint32_t> F(sigma);
    for(size_t i=0;i<sigma;i++) {
        F[i] = - freqs[i].first;
    }
    size_t frame_factor = 1;
    while(frame_factor < 32) {
        std::vector<uint32_t> S(sigma);
        size_t frame_size = sigma * frame_factor;
        if (!is_power_of_two(frame_size)) {
            frame_size = next_power_of_two(frame_size);
        }
        size_t init_M = frame_size;
        int64_t in_len = m;
        double fratio = double(frame_size)/double(m);
        for(int64_t i=sigma-1;i>=0;i--) {
            double aratio = double(frame_size)/double(in_len);
            double ratio  = i*fratio/sigma + (sigma-i)*aratio/sigma;
            S[i] = (uint32_t) (0.5 + ratio*F[i]);
            if(S[i]==0)
                S[i] = 1;
            frame_size -= S[i];
            in_len -= F[i];
        }

        // compute prelude
        std::vector<uint32_t> prelude(max_sym+1,0);
        for(size_t i=0;i<sigma;i++) {
            auto mapped_sym = freqs[i].second;
            prelude[mapped_sym] = S[i];
        }
        std::vector<uint32_t> increasing_freqs(max_sym+1);
        increasing_freqs[0] = prelude[0];
        for(size_t sym=1;sym<max_sym;sym++) {
            increasing_freqs[sym] = increasing_freqs[sym-1] +  prelude[sym] + 1;
        }
        auto prelude_buf = increasing_freqs.data();
        std::vector<uint32_t> out_buf(init_M+max_sym*2);
        auto bytes_written = interpolative_internal::encode(out_buf.data(),prelude_buf,max_sym+1,init_M+max_sym) + 8;
        double prelude_bits = bytes_written * 8;
        double prelude_bpi = prelude_bits / double(m);

        auto XH = cross_entropy(F,S);
        double inefficiency = 100.0*(XH-input_entropy)/input_entropy;

        double inefficiency2 = 100.0*((XH+prelude_bpi)-input_entropy)/input_entropy;

        printf("%-15s\tM=%-12d\tH0=%2.2f\tXH=%2.2f\tINEFF=%2.2f\tPRELUDE_BPI=%2.2f\tTOTAL_BPI=%2.2f\tTOTAL_INEFF=%2.2f\n",
                name.c_str(),
                (int)init_M,
                input_entropy,
                XH,
                inefficiency,
                prelude_bpi,
                XH+prelude_bpi,
                inefficiency2
            );
        fflush(stdout);
        frame_factor++;
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

        rescale_freqs(input_u32s,short_name);

    }

    return EXIT_SUCCESS;
}
