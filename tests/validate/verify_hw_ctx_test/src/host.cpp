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
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
// XRT includes
#include "experimental/xrt_system.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
static const int COUNT = 1024;

static constexpr char gold[] = "Hello World\n";


static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p <path>\n";
    std::cout << "  -d <device> \n";
    std::cout << "  -s <supported>\n";
    std::cout << "  -h <help>\n";
}

int main(int argc, char** argv) {
    std::string dev_id = "0";
    std::string test_path;
    std::string b_file = "/verify.xclbin";
    bool flag_s = false;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--path") == 0)) {
            test_path = argv[i + 1];
        } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--device") == 0)) {
            dev_id = argv[i + 1];
        } else if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--supported") == 0)) {
            flag_s = true;
        } else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            printHelp();
            return 1;
        }
    }

    if (test_path.empty()) {
        std::cout << "ERROR : please provide the platform test path to -p option\n";
        return EXIT_FAILURE;
    }
    std::string binaryfile = test_path + b_file;
    std::ifstream infile(binaryfile);
    if (flag_s) {
        if (!infile.good()) {
            std::cout << "\nNOT SUPPORTED" << std::endl;
            return EOPNOTSUPP;
        } else {
            std::cout << "\nSUPPORTED" << std::endl;
            return EXIT_SUCCESS;
        }
    }

    if (!infile.good()) {
        std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    }

    auto num_devices = xrt::system::enumerate_devices();

    auto device = xrt::device {dev_id};

    auto uuid = device.register_xclbin(xrt::xclbin{binaryfile});
    xrt::hw_context ctx{device, uuid};
    auto hello_world = xrt::kernel(ctx, "verify");
    const size_t DATA_SIZE = COUNT * sizeof(int);
    auto bo = xrt::bo(device, DATA_SIZE, hello_world.group_id(0));
    auto bo_data = bo.map<char*>();
    std::fill(bo_data, bo_data + 1024, 0);
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, 1024,0);

    auto run = hello_world(bo);
    std::cout << "Kernel start command issued" << std::endl;
    std::cout << "Now wait until the kernel finish" << std::endl;

    //Get the output;
    std::cout << "Get the output data from the device" << std::endl;
    bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0);

    std::cout << "RESULT: " << std::endl;
    for (unsigned i = 0; i < 20; ++i)
	    std::cout << bo_data[i];
    
    std::cout << std::endl;
    if (!std::equal(std::begin(gold), std::end(gold), bo_data)) {
	std::cout << "Incorrect value obtained" << std::endl;
    	std::cout << "TEST FAILED\n";

	return EXIT_FAILURE;
    }

    std::cout << "TEST PASSED\n";
    return 0;
}
