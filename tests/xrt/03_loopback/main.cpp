/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <getopt.h>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>
#include <cstring>
#include <sys/mman.h>

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

static const int DATA_SIZE = 1024;

/**
 * Trivial loopback example which runs OpenCL loopback kernel. Does not use OpenCL
 * runtime but directly exercises the XRT driver API.
 */


const static struct option long_options[] = {
{"bitstream",       required_argument, 0, 'k'},
{"hal_logfile",     required_argument, 0, 'l'},
{"device",          required_argument, 0, 'd'},
{"verbose",         no_argument,       0, 'v'},
{"help",            no_argument,       0, 'h'},
{0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -l <hal_logfile>\n";
    std::cout << "  -d <device_index>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "";
    std::cout << "* If HAL driver is not specified, application will try to find the HAL driver\n";
    std::cout << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n";
    std::cout << "* Bitstream is required\n";
    std::cout << "* HAL logfile is optional but useful for capturing messages from HAL driver\n";
}

int main(int argc, char** argv)
{
    std::string sharedLibrary;
    std::string bitstreamFile;
    std::string halLogfile;
    int option_index = 0;
    unsigned index = 0;
    bool verbose = false;
    int c;
    while ((c = getopt_long(argc, argv, "s:k:l:a:c:d:vh", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 'k':
            bitstreamFile = optarg;
            break;
        case 'l':
            halLogfile = optarg;
            break;
        case 'd':
            index = std::atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        case 'v':
            verbose = true;
            break;
        default:
            printHelp();
            return -1;
        }
    }

    (void)verbose;

    if (bitstreamFile.size() == 0) {
        std::cout << "FAILED TEST\n";
        std::cout << "No bitstream specified\n";
        return -1;
    }

    if (halLogfile.size()) {
        std::cout << "Using " << halLogfile << " as HAL driver logfile\n";
    }

    std::cout << "HAL driver = " << sharedLibrary << "\n";
    std::cout << "Compiled kernel = " << bitstreamFile << "\n" << std::endl;

    try
    {
      if (index >= xclProbe())
        throw std::runtime_error("Cannot find device index specified");

      auto device = xrt::device(index);
      auto uuid = device.load_xclbin(bitstreamFile);

      auto loopback = xrt::kernel(device, uuid.get(), "loopback");
      auto bo0 = xrt::bo(device, DATA_SIZE, XCL_BO_FLAGS_NONE, loopback.group_id(0));  // handle 1
      auto bo1 = xrt::bo(device, DATA_SIZE, XCL_BO_FLAGS_NONE, loopback.group_id(1));  // handle 2

      auto bo1_map = bo1.map<char*>();
      std::fill(bo1_map, bo1_map + DATA_SIZE, 0);
      std::string testVector =  "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n";
      std::strcpy(bo1_map, testVector.c_str());
      bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);

      std::cout << "\nStarting kernel..." << std::endl;
      auto run = loopback(bo0, bo1, DATA_SIZE);
      run.wait();

      //Get the output;
      bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);
      auto bo0_map = bo0.map<char*>();

      if (std::memcmp(bo1_map, bo0_map, DATA_SIZE)) {
        std::cout << "FAILED TEST\n";
        std::cout << "Value read back does not match value written\n";
        return 1;
      }
    }
    catch (std::exception const& e)
    {
      std::cout << "Exception: " << e.what() << "\n";
      std::cout << "FAILED TEST\n";
      return 1;
    }

    std::cout << "PASSED TEST\n";
    return 0;
}
