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

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

// XRT includes
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

// This value is shared with worgroup size in kernel.cl
static const int COUNT = 1024;

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  [-d <index>]       (default: 0)\n";
    std::cout << "  [-v]\n";
    std::cout << "  [-h]\n\n";
    std::cout << "* Bitstream is required\n";
}

static int run(const xrt::device& device, const xrt::uuid& uuid, bool verbose)
{
  const size_t DATA_SIZE = COUNT * sizeof(int);

  auto simple = xrt::kernel(device, uuid.get(), "simple");
  auto bo0 = xrt::bo(device, DATA_SIZE, simple.group_id(0));
  auto bo1 = xrt::bo(device, DATA_SIZE, simple.group_id(1));
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

int
run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  bool verbose = false;
  unsigned int device_index = 0;

  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }
    else if (arg == "-v") {
      verbose = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "-d")
      device_index = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);
  run(device, uuid, verbose);
  return 0;
}

int main(int argc, char** argv)
{
  try {
    auto ret = run(argc, argv);
    std::cout << "PASSED TEST\n";
    return ret;
  }
  catch (std::exception const& e) {
    std::cout << "Exception: " << e.what() << "\n";
    std::cout << "FAILED TEST\n";
    return 1;
  }

  std::cout << "PASSED TEST\n";
  return 0;
}
