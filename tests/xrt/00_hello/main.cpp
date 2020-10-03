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
#include <vector>
#include <cstdlib>

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"
/**
 * Trivial loopback example which runs OpenCL loopback kernel. Does not use OpenCL
 * runtime but directly exercises the XRT driver API.
 */


static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <device_index>\n";
    std::cout << "  -c <name of compute unit in xclbin>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "";
    std::cout << "* Bitstream is required\n";
    std::cout << "* Name of compute unit from loaded xclbin is required\n";
}

static void
sync_test(const xrt::device& device, int32_t grpidx)
{
  std::string testVector =  "hello\nthis is Xilinx sync BO read write test\n:-)\n";
  const size_t data_size = testVector.size();

  auto bo = xrt::bo(device, data_size, 0, grpidx);
  auto bo_data = bo.map<char*>();
  std::copy_n(testVector.begin(), data_size, bo_data);
  bo.sync(XCL_BO_SYNC_BO_TO_DEVICE , data_size , 0);
  bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE , data_size, 0);
  if (!std::equal(testVector.begin(), testVector.end(), bo_data))
    throw std::runtime_error("Value read back from sync bo does not match value written");
}

static void
copy_test(const xrt::device& device, size_t bytes, int32_t grpidx)
{
  auto bo1 = xrt::bo(device, bytes, 0, grpidx);
  auto bo1_data = bo1.map<char*>();
  std::generate_n(bo1_data, bytes, []() { return std::rand() % 256; });
  
  bo1.sync(XCL_BO_SYNC_BO_TO_DEVICE , bytes , 0);

  auto bo2 = xrt::bo(device, bytes, 0, grpidx);
  bo2.copy(bo1, bytes);
  bo2.sync(XCL_BO_SYNC_BO_FROM_DEVICE , bytes, 0);
  auto bo2_data = bo2.map<char*>();
  if (!std::equal(bo1_data, bo1_data + bytes, bo2_data))
    throw std::runtime_error("Value read back from copy bo does not match value written");
}

static void
register_test(const xrt::kernel& kernel, int argno)
{
  try {
    auto offset = kernel.offset(argno);
    // Throws unless Runtime.rw_shared=true
    // Note, that xclbin must also be loaded with rw_shared=true
    auto val = kernel.read_register(offset);
    std::cout << "value at 0x" << std::hex << offset << " = 0x" << val << std::dec << "\n";
  }
  catch (const std::exception& ex) {
    std::cout << "Expected failed kernel register read (" << ex.what() << ")\n";
  }      
}

int run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  std::string cu_name = "dummy";
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
    else if (cur == "-c")
      cu_name = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);
  auto kernel = xrt::kernel(device, uuid, cu_name);
  auto grpidx = kernel.group_id(0);

  sync_test(device, grpidx);
  copy_test(device, 4096, grpidx);

  // Copy through host not 64 byte aligned
  copy_test(device, 40, grpidx);

  // Test of kernel.read_register (expected failure)
  register_test(kernel, 0);

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
