/**
* Copyright (C) 2022 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/
#include <boost/program_options.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
// XRT includes
#include "experimental/xrt_system.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

static int
validate_binary_file(const std::string& binaryfile, bool print = false)
{
    std::ifstream infile(binaryfile);
    if (!infile.good()) {
        if (print)
            std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    } else {
        if (print)
            std::cout << "\nSUPPORTED" << std::endl;
        return EXIT_SUCCESS;
    }
}

int main(int argc, char** argv) {
    std::string dev_id = "0";
    std::string test_path;
    std::string iter_cnt;
    std::string b_file;
    bool flag_s;

    boost::program_options::options_description options;
    options.add_options()
        ("help,h", "Print help messages")
        ("xclbin,x", boost::program_options::value<decltype(b_file)>(&b_file)->implicit_value("/lib/firmware/xilinx/ps_kernels/ps_bandwidth.xclbin"), "Path to the xclbin file for the test")
        ("path,p", boost::program_options::value<decltype(test_path)>(&test_path)->required(), "Path to the platform resources")
        ("device,d", boost::program_options::value<decltype(dev_id)>(&dev_id)->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
        ("supported,s", boost::program_options::bool_switch(&flag_s), "Print supported or not")
        ("include,i" , boost::program_options::value<decltype(depedency_paths)>(&depedency_paths)->multitoken(), "Paths to xclbins required for this test")
        ("loop_iter_cnt,l", boost::program_options::value<decltype(iter_cnt)>(&iter_cnt)->implicit_value("10000"), "The number of iterations the test will sample over")
    ;

    boost::program_options::variables_map vm;
    try {
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), vm);
        if (vm.count("help")) {
            std::cout << options << std::endl;
            return EXIT_SUCCESS;
        }
        boost::program_options::notify(vm);
    } catch (boost::program_options::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        std::cout << options << std::endl;
        return EXIT_FAILURE;
    }

    auto num_devices = xrt::system::enumerate_devices();

    auto device = xrt::device {dev_id};

    auto uuid = device.load_xclbin(b_file);
    auto bandwidth_kernel = xrt::kernel(device, uuid, "bandwidth_kernel");

    auto max_throughput_bo = xrt::bo(device, 4096, bandwidth_kernel.group_id(1));
    auto max_throughput = max_throughput_bo.map<double*>();

    int reps = stoi(iter_cnt);

    std::fill(max_throughput,max_throughput+(4096/sizeof(double)),0);

    max_throughput_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, 4096, 0);
    
    auto run = bandwidth_kernel(reps,max_throughput_bo);
    run.wait();
    
    max_throughput_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, 4096, 0);

    std::cout << "Throughput (Type: DDR) : " << max_throughput[0] << "MB/s\n";

    std::cout << "TEST PASSED\n";

    return EXIT_SUCCESS;
}
