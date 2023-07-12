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
#include <algorithm>
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

#define WIDTH 8
#define HEIGHT 8 
#define SIZE (WIDTH * HEIGHT)

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
    std::string b_file;
    std::vector<std::string> dependency_paths;
    bool flag_s;

    boost::program_options::options_description options;
    options.add_options()
        ("help,h", "Print help messages")
        ("xclbin,x", boost::program_options::value<decltype(b_file)>(&b_file)->implicit_value("/lib/firmware/xilinx/ps_kernels/ps_aie.xclbin"), "Path to the xclbin file for the test")
        ("path,p", boost::program_options::value<decltype(test_path)>(&test_path)->required(), "Path to the platform resources")
        ("device,d", boost::program_options::value<decltype(dev_id)>(&dev_id)->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
        ("supported,s", boost::program_options::bool_switch(&flag_s), "Print supported or not")
        ("include,i" , boost::program_options::value<decltype(dependency_paths)>(&dependency_paths)->multitoken(), "Paths to xclbins required for this test")
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

    // Load dependency xclbins onto device if any
    for (const auto& path : dependency_paths) {
        auto retVal = validate_binary_file(path);
        if (retVal != EXIT_SUCCESS)
            return retVal;
        auto uuid = device.load_xclbin(path);
    }

    // Load ps kernel onto device
    auto retVal = validate_binary_file(b_file, flag_s);
    if (flag_s || retVal != EXIT_SUCCESS)
        return retVal;

    const int input_size_in_bytes = SIZE * sizeof(float);
    const int output_size_in_bytes = SIZE * sizeof(float);
    const int input_size_allocated = ((input_size_in_bytes / 4096) + ((input_size_in_bytes % 4096) > 0)) * 4096;
    const int output_size_allocated = ((output_size_in_bytes / 4096) + ((output_size_in_bytes % 4096) > 0)) * 4096;

    auto uuid = device.load_xclbin(b_file);
    auto aie_kernel = xrt::kernel(device,uuid, "aie_kernel");
    auto out_bo= xrt::bo(device, output_size_allocated, aie_kernel.group_id(2));
    auto out_bomapped = out_bo.map<float*>();
    memset(out_bomapped, 0, output_size_in_bytes);

    auto in_bo_a = xrt::bo(device, input_size_allocated, aie_kernel.group_id(0));
    auto in_bomapped_a = in_bo_a.map<float*>();
    auto in_bo_b = xrt::bo(device, input_size_allocated, aie_kernel.group_id(1));
    auto in_bomapped_b = in_bo_b.map<float*>();
	
    //setting input data
    std::vector<float> golden(SIZE);
    for (int i = 0; i < SIZE; i++){
        in_bomapped_a[i] = rand() % SIZE;
        in_bomapped_b[i] = rand() % SIZE;
    }
    for (int i = 0; i < HEIGHT ; i++) {
        for (int j = 0; j < WIDTH ; j++){
            golden[i*WIDTH+j] = 0;
            for (int k=0; k <WIDTH; k++)
	      golden[i*WIDTH+j] += in_bomapped_a[i*WIDTH + k] * in_bomapped_b[k+WIDTH * j];
        }
    } 

    in_bo_a.sync(XCL_BO_SYNC_BO_TO_DEVICE, input_size_in_bytes, 0);
    in_bo_b.sync(XCL_BO_SYNC_BO_TO_DEVICE, input_size_in_bytes, 0);

    int rval;

    auto run = aie_kernel(in_bo_a, in_bo_b, out_bo, input_size_in_bytes, output_size_in_bytes);
    run.wait();

    out_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, output_size_in_bytes, 0);
    
    int match = 0;
    for (int i = 0; i < SIZE; i++) {
        if (out_bomapped[i] != golden[i]) {
            printf("ERROR: Test failed! Error found in sample %d: golden: %f, hardware: %f\n", i, golden[i], out_bomapped[i]);
            match = 1;
            break;
        }
    }

    std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl; 

    return (match ? EXIT_FAILURE :  EXIT_SUCCESS);
}
