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
#include <vector>
#include <stdexcept>
#include <string>
#include <cstring>

#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

static const int DATA_SIZE = 1024;

/**
 * Trivial loopback example which runs OpenCL loopback kernel. Does not use OpenCL
 * runtime but directly exercises the XRT driver API.
 */

static void usage()
{
    std::cout << "usage: %s [options] -k <xclbin>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <bdf | index> (default: 0)\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "";
    std::cout << "* Bitstream is required\n";
}

int run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  bool verbose = false;
  std::string device_index = "0";

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
      device_index = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);

  auto loopback = xrt::kernel(device, uuid.get(), "loopback");
  auto bo0 = xrt::bo(device, DATA_SIZE, loopback.group_id(0));  // handle 1
  auto bo1 = xrt::bo(device, DATA_SIZE, loopback.group_id(1));  // handle 2

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

  if (std::memcmp(bo1_map, bo0_map, DATA_SIZE))
    throw std::runtime_error("Value read back does not match value written");

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
