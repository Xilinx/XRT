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
static const int COUNT = 4096;

static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p <path>\n";
    std::cout << "  -d <device> \n";
    std::cout << "  -s <supported>\n";
    std::cout << "  -h <help>\n";
}

struct process_header
{
    size_t count;
};
#define MAX_DATA_LENGTH 16
struct process_data
{
    char name[MAX_DATA_LENGTH];
    char vsz[MAX_DATA_LENGTH];
    char stat[MAX_DATA_LENGTH];
    char etime[MAX_DATA_LENGTH];
    char cpu[MAX_DATA_LENGTH];
    char cpu_util[MAX_DATA_LENGTH];

    friend std::ostream& operator<<(std::ostream& out, const struct process_data& d)
    {
        out << d.name << " " << d.etime << " " << d.vsz 
            << " " << d.stat << " " << d.cpu << " " << d.cpu_util;
        return out;
    }
};

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

    auto uuid = device.load_xclbin(binaryfile);
    auto hello_world = xrt::kernel(device, uuid.get(), "hello_world");

    const size_t DATA_SIZE = COUNT * sizeof(char);
    auto bo0 = xrt::bo(device, DATA_SIZE, hello_world.group_id(0));
    auto bo0_map = bo0.map<char*>();
    std::fill(bo0_map, bo0_map + COUNT, 0);

    bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);

    auto run = hello_world(bo0, COUNT);
    run.wait();

    //Get the output;
    bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);
    struct process_header* header = (struct process_header*) bo0_map;
    std::cout << "Data Count: " << header->count << std::endl;
    struct process_data* data = (struct process_data*) (bo0_map + sizeof(struct process_header));
    for (int i = 0; i < header->count; i++) {
        std::cout << data[i] << std::endl;
    }
    return 0;
}
