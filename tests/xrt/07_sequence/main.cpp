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

#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

/**
 * Runs an OpenCL kernel which writes known 16 integers into a 64 byte
 * buffer. Does not use OpenCL runtime but directly exercises the HAL
 * driver API.
 */

static const int DATA_SIZE = 16;

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <device_index>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "";
    std::cout << "* Bitstream is required\n";
}

const unsigned goldenSequence[16] = {
    0X586C0C6C,
    'X',
    0X586C0C6C,
    'I',
    0X586C0C6C,
    'L',
    0X586C0C6C,
    'I',
    0X586C0C6C,
    'N',
    0X586C0C6C,
    'X',
    0X586C0C6C,
    '\0',
    0X586C0C6C,
    '\0'
};

int run(int argc, char** argv)
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

  auto mysequence = xrt::kernel(device, uuid.get(), "mysequence");

  auto bo = xrt::bo(device, DATA_SIZE*sizeof(unsigned int), mysequence.group_id(0));
  auto bo_mapped = bo.map<unsigned int*>();
  memset(bo_mapped, 0, DATA_SIZE*sizeof(unsigned int));
  bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE*sizeof(unsigned int), 0);

  auto run = mysequence(bo);
  run.wait();

  bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE*sizeof(unsigned int), 0);
  if (std::memcmp(goldenSequence, bo_mapped, DATA_SIZE*sizeof(unsigned int)))
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
