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

template<uint32_t fidelity>
uint32_t compute_bucket(uint32_t x)
{
    const uint32_t radix = 8;
    uint32_t radix_mask = ((1<<radix)-1);
    size_t offset = 0;
    size_t thres = 1 << (fidelity+radix-1);
    while(x >= thres) {
        auto digit = x & radix_mask;
        x = x >> radix;
        offset = offset + (1<<(fidelity-1)) * radix_mask;
    }
    return x + offset;
}

double kl_divergance(std::vector<double>& P,std::vector<double>& Q)
{
    double kl = 0.0;
    for(size_t i=0;i<P.size();i++) {
        if(P[i] != 0.0 && Q[i] != 0.0) kl += P[i] * log2(Q[i]/P[i]);
    }
    return -kl;
}

template<uint32_t fidelity>
void compute_skew(std::vector<uint32_t>& input,std::string name)
{
    std::vector<uint32_t> bucket_map(1<<27);
    for(size_t i=0;i<bucket_map.size();i++) {
        bucket_map[i] = compute_bucket<fidelity>(i);
    }
    std::vector<uint32_t> bucket_sizes;
    std::vector<uint32_t> bucket_min;
    std::vector<uint32_t> bucket_max;
    uint32_t prev = bucket_map[0];
    bucket_min.push_back(0);
    auto cur_size = 1;
    for(size_t i=1;i<bucket_map.size();i++) {
        auto cur = bucket_map[i];
        if(cur != prev) {
            bucket_max.push_back(i-1);
            bucket_min.push_back(i);
            bucket_sizes.push_back(cur_size);
            cur_size = 0;
        }
        cur_size++;
        prev = cur;
    }
    if(cur_size) {
        bucket_max.push_back(bucket_map.size()-1);
        bucket_sizes.push_back(cur_size);
    }

    std::vector<uint32_t> bucket_usage(bucket_sizes.size());
    for(size_t i=0;i<input.size();i++) {
        auto bucket_id = compute_bucket<fidelity>(input[i]);
        bucket_usage[bucket_id]++;
    }

    std::vector<std::vector<double>> real_dists(bucket_sizes.size());
    for(size_t i=0;i<input.size();i++) {
        auto bucket_id = compute_bucket<fidelity>(input[i]);
        if(real_dists[bucket_id].size()==0)
            real_dists[bucket_id].resize(bucket_sizes[bucket_id]);
        auto bucket_offset = input[i] - bucket_min[bucket_id];
        real_dists[bucket_id][bucket_offset] += 1.0;
    }
    size_t usage_cum_sum = 0;
    for(size_t i=0;i<bucket_sizes.size();i++) {
        if(bucket_usage[i]!=0) {
            double bits_uniform = - log2(double(1.0)/double(bucket_sizes[i]));
            double bits_real = 0.0;
            for(size_t j=0;j<real_dists[i].size();j++) {
                if(real_dists[i][j] != 0.0) {
                    double bits = - log2(real_dists[i][j]/double(bucket_usage[i]));
                    bits_real += real_dists[i][j] * bits;
                }
            }
            bits_real /= double(bucket_usage[i]);
            usage_cum_sum += bucket_usage[i];
            std::cout << name << ";"
                    << i << ";"
                    << fidelity << ";"
                    << bucket_min[i] << ";"
                    << bucket_max[i] << ";"
                    << bucket_sizes[i] << ";"
                    << bucket_usage[i] << ";"
                    << usage_cum_sum << ";"
                    << input.size() << ";"
                    << (bits_uniform - bits_real) << std::endl;
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

        compute_skew<1>(input_u32s,short_name);
        compute_skew<2>(input_u32s,short_name);
        compute_skew<3>(input_u32s,short_name);
        compute_skew<4>(input_u32s,short_name);
        compute_skew<5>(input_u32s,short_name);
        compute_skew<6>(input_u32s,short_name);
    }

    return EXIT_SUCCESS;
}
