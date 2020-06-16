/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

static const int DATA_SIZE = 4096;

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

  if (device_index >= xclProbe())
    throw std::runtime_error("Cannot find device index (" + std::to_string(device_index) + ") specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);

  // Create kernel.  The kernel has ((reqd_work_group_size(16, 1, 1)))
  // It will be iterated for each work group.
  auto swizzle = xrt::kernel(device, uuid.get(), "vectorswizzle");

  // Create a parent BO for kernel input data
  auto bo = xrt::bo(device, DATA_SIZE*sizeof(int), 0, swizzle.group_id(0));
  auto bo_mapped = bo.map<int*>();

  //Populate the input and reference vectors.
  int reference[DATA_SIZE];
  for (int i = 0; i < DATA_SIZE; i++) {
    bo_mapped[i] = i;
    int val = 0;
    if(i%4==0)  val = i+2;
    if(i%4==1)  val = i+2;
    if(i%4==2)  val = i-2;
    if(i%4==3)  val = i-2;
    reference[i] = val;
  }

  // Sync all data to device, kernel will be run in groups using sub-buffers
  bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE*sizeof(int), 0);

  // Create a run object without starting kernel
  auto run = xrt::run(swizzle);

  const size_t global[1] = {DATA_SIZE / 4}; // int4 vector count global range
  const size_t local[1] = {16}; // 16 int4 processed per work group
  const size_t group_size = global[0] / local[0];

  // Run swizzle with 16 (local[0]) elements at a time
  // Each element is an int4 (sizeof(int) * 4 bytes)
  // Create sub buffer to offset kernel argument in parent buffer
  const size_t local_size_bytes = local[0] * sizeof(int) * 4;
  for (size_t id = 0; id < group_size; id++) {
    auto subbo = xrt::bo(bo, local_size_bytes, local_size_bytes * id);
    run.set_arg(0, subbo);
    run.start();
    run.wait();
  }

  //Get the output;
  bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE , DATA_SIZE*4, 0);

  if (std::memcmp(bo_mapped, reference, DATA_SIZE*4))
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
