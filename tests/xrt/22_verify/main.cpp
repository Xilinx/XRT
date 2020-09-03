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

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

/**
 * Runs an OpenCL kernel which writes "Hello World\n" into the buffer passed
 */

#define ARRAY_SIZE 20
////////////////////////////////////////////////////////////////////////////////

#define LENGTH (20)

////////////////////////////////////////////////////////////////////////////////

static const char gold[] = "Hello World\n";

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

static void
run(const xrt::device& device, const xrt::uuid& uuid, bool verbose)
{
  auto hello = xrt::kernel(device, uuid.get(), "hello:hello_1");

  auto bo = xrt::bo(device, 1024, 0, hello.group_id(0));
  auto bo_data = bo.map<char*>();
  std::fill(bo_data, bo_data + 1024, 0);
  bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, 1024,0);

  auto run = hello(bo);
  std::cout << "Kernel start command issued" << std::endl;
  std::cout << "Now wait until the kernel finish" << std::endl;

  run.wait();

  //Get the output;
  std::cout << "Get the output data from the device" << std::endl;
  bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0);

  std::cout << "RESULT: " << std::endl;
  for (unsigned i = 0; i < 20; ++i)
    std::cout << bo_data[i];
  std::cout << std::endl;
  if (!std::equal(std::begin(gold), std::end(gold), bo_data))
    throw std::runtime_error("Incorrect value obtained");
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
