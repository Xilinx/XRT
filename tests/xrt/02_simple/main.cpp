/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <sys/mman.h>

// XRT includes
#include "xrt.h"
#include "ert.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_xclbin.h"
#include "xclbin.h"

// lowlevel common include
#include "utils.h"

// This value is shared with worgroup size in kernel.cl
static const int COUNT = 1024;

const static struct option long_options[] = {
{"bitstream",       required_argument, 0, 'k'},
{"device",          required_argument, 0, 'd'},
{"verbose",         no_argument,       0, 'v'},
{"help",            no_argument,       0, 'h'},
{0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

static int runKernel(xclDeviceHandle handle, bool verbose, int first_mem, const uuid_t xclbinId)
{
  const size_t DATA_SIZE = COUNT * sizeof(int);

  auto simple = xrt::kernel(handle, xclbinId, "simple");
  auto bo0 = xrt::bo(handle, DATA_SIZE, XCL_BO_FLAGS_NONE, simple.group_id(0));
  auto bo1 = xrt::bo(handle, DATA_SIZE, XCL_BO_FLAGS_NONE, simple.group_id(1));
  auto bo0_map = bo0.map<int*>();
  auto bo1_map = bo1.map<int*>();
  std::fill(bo0_map, bo0_map + COUNT, 0);
  std::fill(bo1_map, bo1_map + COUNT, 0);

  // Fill our data sets with pattern
  int foo = 0x10;
  int bufReference[COUNT];
  for (int i = 0; i < COUNT; ++i) {
    bo0_map[i] = 0;
    bo1_map[i] = i;
    bufReference[i] = i + i * foo;
  }

  bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);
  bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);

  auto run = simple(bo0, bo1, 0x10);
  run.wait();

  //Get the output;
  std::cout << "Get the output data from the device" << std::endl;
  bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);

  // Validate our results
  if (std::memcmp(bo0_map, bufReference, DATA_SIZE))
    throw std::runtime_error("Value read back does not match reference");

  return 0;
}

int main(int argc, char** argv)
{
    std::string sharedLibrary;
    std::string bitstreamFile;
    std::string halLogfile;
    size_t alignment = 128;
    int option_index = 0;
    unsigned index = 0;
    unsigned cu_index = 0;
    bool verbose = false;
    bool ert = false;
    int c;

    while ((c = getopt_long(argc, argv, "k:d:vh", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 'k':
            bitstreamFile = optarg;
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
            return 1;
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
    std::cout << "Host buffer alignment = " << alignment << " bytes\n";
    std::cout << "Compiled kernel = " << bitstreamFile << "\n";


    try {
        xclDeviceHandle handle;
        uint64_t cu_base_addr = 0;
        int first_mem = -1;
        uuid_t xclbinId;

        if (initXRT(bitstreamFile.c_str(), index, halLogfile.c_str(), handle, cu_index, cu_base_addr, first_mem, xclbinId))
            return 1;

        if (first_mem < 0)
            return 1;

        runKernel(handle, verbose, first_mem, xclbinId);
        xclClose(handle);
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
