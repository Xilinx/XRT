// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
/****************************************************************
This example illustrates the use of xrt::hw_context for working with
multiple xclbins

% g++ -g -std=c++17 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o simple.exe simple.cpp -lxrt_coreutil -luuid -pthread
****************************************************************/
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
# pragma warning ( disable : 4267 )
#endif

// Kernel specifics
// void addone (__global ulong8 *a, __global ulong8 * b, unsigned int  elements)
// addone(a, b, ELEMENTS)
// The kernel is compiled with 8 CUs same connectivity.
static constexpr size_t elements = 16;
static constexpr size_t array_size = 8;
static constexpr size_t maxcus = 8;
static constexpr size_t data_size = elements * array_size;
static size_t compute_units = maxcus;

static void
usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <device_index>\n";
  std::cout << "";
  std::cout << "  [--cus <number>]: number of cus to use (default: 8) (max: 8)\n";
  std::cout << "";
}

static std::string
get_kernel_name(size_t cus)
{
  std::string k("addone:{");
  for (int i=1; i<cus; ++i)
    k.append("addone_").append(std::to_string(i)).append(",");
  k.append("addone_").append(std::to_string(cus)).append("}");
  return k;
}

static int
run(int argc, char** argv)
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::vector<xrt::xclbin> xclbins;
  unsigned int device_index = 0;
  size_t cus  = 1;

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

    if (cur == "-d")
      device_index = std::stoi(arg);
    else if (cur == "-k")
      xclbins.push_back(xrt::xclbin{arg});
    else if (cur == "--cus")
      cus = std::stoi(arg);
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  xrt::device device{device_index};

  // Register all xclbins
  for (const auto& xclbin : xclbins)
    device.register_xclbin(xclbin);

  compute_units = cus = std::min(cus, compute_units);
  std::string kname = get_kernel_name(cus);

  std::vector<xrt::run> runs;
  for (const auto& xclbin : xclbins) {
    xrt::hw_context hwctx{device, xclbin.get_uuid()};
    xrt::kernel kernel{hwctx, kname};
    xrt::bo a(hwctx, data_size * sizeof(unsigned long), kernel.group_id(0));
    xrt::bo b(hwctx, data_size * sizeof(unsigned long), kernel.group_id(1));
    runs.push_back(kernel(a, b, elements));
  }

  for (auto& run : runs)
    run.wait();

  return 0;
}

int
main(int argc, char* argv[])
{
  try {
    return run(argc,argv);
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << "\n";
  }
  catch (...) {
    std::cout << "TEST FAILED\n";
  }

  return 1;
}
