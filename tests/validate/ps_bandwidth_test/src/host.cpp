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
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
// XRT includes
#include "experimental/xrt_system.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p <path>\n";
    std::cout << "  -d <device> \n";
    std::cout << "  -l <loop_iter_cnt> \n";
    std::cout << "  -s <supported>\n";
    std::cout << "  -h <help>\n";
}

int main(int argc, char** argv) {
    std::string dev_id = "0";
    std::string test_path;
    std::string iter_cnt = "10000";
    std::string b_file = "/ps_bandwidth.xclbin";
    bool flag_s = false;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--path") == 0)) {
            test_path = argv[i + 1];
        } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--device") == 0)) {
            dev_id = argv[i + 1];
        } else if ((strcmp(argv[i], "-l") == 0) || (strcmp(argv[i], "--loop_iter_cnt") == 0)) {
            iter_cnt = argv[i + 1];
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

    std::string filename = "/platform.json";
    auto platform_json = boost::filesystem::path(test_path) / filename;

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
    auto bandwidth_kernel = xrt::kernel(device, uuid.get(), "bandwidth_kernel");

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
