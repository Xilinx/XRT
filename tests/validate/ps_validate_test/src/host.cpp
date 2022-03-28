/**
* Copyright (C) 2019-2021 Xilinx, Inc
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
#include <boost/filesystem.hpp>
#include <algorithm>
#include <vector>
// XRT includes
#include "experimental/xrt_system.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
static const int COUNT = 1024;

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
    std::string b_file = "/ps_validate_bandwidth.xclbin";
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
    auto binaryFile = boost::filesystem::path(test_path) / b_file;
    std::ifstream infile(binaryFile.string());
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

    xrt::device device;
    auto pos = dev_id.find(":");
    if (pos == std::string::npos) {
        uint32_t device_index = stoi(dev_id);
        if (device_index >= num_devices) {
            std::cout << "The device_index provided using -d flag is outside the range of "
                         "available devices\n";
            return EXIT_FAILURE;
        }
        device = xrt::device(device_index);
    } else {
        device = xrt::device(dev_id);
    }

    auto uuid = device.load_xclbin(binaryFile.string());
    auto hello_world = xrt::kernel(device, uuid.get(), "hello_world");
    const size_t DATA_SIZE = COUNT * sizeof(int);
    auto bo0 = xrt::bo(device, DATA_SIZE, 2);
    auto bo1 = xrt::bo(device, DATA_SIZE, 2);
    auto bo0_map = bo0.map<int*>();
    auto bo1_map = bo1.map<int*>();
    std::fill(bo0_map, bo0_map + COUNT, 0);
    std::fill(bo1_map, bo1_map + COUNT, 0);

    // Fill our data sets with pattern
    int bufReference[COUNT];
    bo0_map[0] = 'h';
    bo0_map[1] = 'e';
    bo0_map[2] = 'l';
    bo0_map[3] = 'l';
    bo0_map[4] = 'o';
    for (int i = 5; i < COUNT; ++i) {
      bo0_map[i] = 0;
      bo1_map[i] = i;
      bufReference[i] = i;
    }
    
    bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);
    bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);
    
    auto run = hello_world(bo0, bo1, COUNT);
    run.wait();
    
    //Get the output;
    bo1.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);
    
    // Validate our results
    if (std::memcmp(bo1_map, bo0_map, DATA_SIZE)) {
      for(int i=0;i< COUNT;i++) {
	std::cout << "bo0[" << i << "] = " << bo0_map[i] << ", bo1[" << i << "] = " << bo1_map[i] << std::endl;
      }
      throw std::runtime_error("Value read back does not match reference");
      return EXIT_FAILURE;
    }
    std::cout << "TEST PASSED\n";
    return 0;
}
