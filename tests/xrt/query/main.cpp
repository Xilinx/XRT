/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "xrt/xrt_device.h"

/**
 * Exercise all xrt::info::device query parameters as defined in
 * xrt_device.h
 */

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <bdf | device_index>\n";
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
  std::string device_index = "0";

  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
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
    throw std::runtime_error("No xclbin specified");

  auto device = xrt::device(device_index);
  auto xclbin = xrt::xclbin{xclbin_fnm};
  auto uuid = device.load_xclbin(xclbin);

  if (uuid != xclbin.get_uuid())
    throw std::runtime_error("Unexpected uuid error");

  std::cout << "device name:           " << device.get_info<xrt::info::device::name>() << "\n";
  std::cout << "device bdf:            " << device.get_info<xrt::info::device::bdf>() << "\n";
  std::cout << "device kdma:           " << device.get_info<xrt::info::device::kdma>() << "\n";
  std::cout << "device max freq:       " << device.get_info<xrt::info::device::max_clock_frequency_mhz>() << "\n";
  std::cout << "device m2m:            " << std::boolalpha << device.get_info<xrt::info::device::m2m>() << std::dec << "\n";
  std::cout << "device nodma:          " << std::boolalpha << device.get_info<xrt::info::device::nodma>() << std::dec << "\n";
  std::cout << "device interface uuid: " << device.get_info<xrt::info::device::interface_uuid>().to_string() << "\n";

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
