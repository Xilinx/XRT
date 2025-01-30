// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc.  All rights reserved.
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include <iostream>
#include <stdexcept>
#include <string>

#include "xrt/xrt_device.h"

// Exercise some xrt::info::device query parameters as defined in
// xrt_device.h
//
// % g++ -g -std=c++17 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o query.exe main.cpp -lxrt_coreutil -luuid -pthread

static void
usage()
{
    std::cout << "usage: %s [options]\n\n";
    std::cout << "  -d <bdf | device_index>\n";
    std::cout << "  [-j] # dump all json queries\n";
    std::cout << "  -h\n\n";
    std::cout << "";
}

static int
run(int argc, char** argv)
{
  if (argc < 2) {
    usage();
    return 1;
  }

  std::string device_index = "0";
  bool json_queries = false;

  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }

    if (arg[0] == '-') {
      cur = arg;

      // No argument switches
      if (cur == "-j")
        json_queries = true;

      continue;
    }

    // Switch arguments
    if (cur == "-d")
      device_index = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  auto device = xrt::device(device_index);

  if (json_queries) {
    std::cout << "device host json info ==========================================\n";
    std::cout << device.get_info<xrt::info::device::host>();
    std::cout << "device platform json info ==========================================\n";
    std::cout << device.get_info<xrt::info::device::platform>();
  }

  // Equality implemented in 2.14
  auto device2 = xrt::device{device_index};
  if (device2 != device) {
#if XRT_VERSION_CODE >= XRT_VERSION(2,14)
    throw std::runtime_error("Equality check failed");
#else
    std::cout << "device equality not implemented in XRT("
              << XRT_MAJOR(XRT_VERSION_CODE) << ","
              << XRT_MINOR(XRT_VERSION_CODE) << ")\n";
#endif
  }

  return 0;
}

int
main(int argc, char** argv)
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
