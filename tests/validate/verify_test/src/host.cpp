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

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include <string.h>
#include <algorithm>
#define LENGTH 64

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

    // Get the device reference using the device argument
    auto device = xrt::device(dev_id);

    std::cout << "Trying to program device...\n";
    auto xclbin_uuid = device.load_xclbin(test_path.append(b_file));
    std::cout << "Device program successful!\n";

    auto krnl = xrt::kernel(device, xclbin_uuid, "verify");

    // Allocate the output buffer to hold the kernel ooutput
    auto output_buffer = xrt::bo(device, sizeof(char) * LENGTH, krnl.group_id(0));

    // Run the kernel and store its contents within the allocated output buffer
    auto run = krnl(output_buffer);
    run.wait();

    // Prepare local buffer
    char received_data[LENGTH] = {};

    // Acquire and read the buffer data
    output_buffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    output_buffer.read(received_data);

    // Compare received data against expected data
    std::string expected_data = "Hello World\n";
    if (std::equal(expected_data.begin(), expected_data.end(), received_data)) {
        std::cout << "TEST FAILED\n";
        throw std::runtime_error("Value read back does not match reference");
    }

    std::cout << "TEST PASSED\n";
    return 0;
}
