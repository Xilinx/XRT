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
#include <numeric>
#include <cstring>

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

#define ARRAY_SIZE 8

/**
 * Runs an OpenCL addone kernel adding one to every 8 unsigned long
 */

static void usage()
{
  std::cout << "usage: %s [options] -k <bitstream>\n\n"
            << "\n"
            << "  -k <bitstream>\n"
            << "  -d <index>\n"
            << "  -n <num of elements, default is 16>\n"
            << "  -v\n"
            << "  -h\n\n"
            << "* Bitstream is required\n";
}

static void
run(xrt::device& device, const xrt::uuid& uuid, size_t n_elements)
{
  auto addone = xrt::kernel(device, uuid.get(), "addone");

  using data_type = unsigned long;
  const size_t size = n_elements * ARRAY_SIZE;
  const size_t bytes = sizeof(data_type) * size;

  auto a = xrt::bo(device, bytes, addone.group_id(0));
  auto a_data = a.map<unsigned long*>();
  std::iota(a_data,a_data+size,0);
  a.sync(XCL_BO_SYNC_BO_TO_DEVICE , bytes, 0);

  auto b = xrt::bo(device, bytes, addone.group_id(1));

  auto run = addone(a, b, n_elements);
  run.wait();

  // verify
  b.sync(XCL_BO_SYNC_BO_FROM_DEVICE , bytes, 0);
  auto b_data = b.map<unsigned long*>();
  for (size_t idx=0; idx<size; ++idx) {
    auto expect = a_data[idx] + (idx%8 ? 0 : 1);
    if (b_data[idx] != expect)
      throw std::runtime_error
        ("b_data[" + std::to_string(idx) + "] = " + std::to_string(b_data[idx])
         + " expected " + std::to_string(expect));
  }
}


int run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  size_t num_elements = 16;
  unsigned int device_index = 0;
  bool verbose = false;

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
    else if (cur == "-n")
      num_elements = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);

  run(device, uuid, num_elements);

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
