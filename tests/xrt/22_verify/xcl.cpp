/*
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************************
Simple verify test bridging from xclBufferHandles to xrt::bo for 
use with xrt::kernel arguments.

It is strongly discouraged to use xcl APIs, please write the 
application using XRT native APIs only (see main.cpp).

There are two test drivers in this file, one that use XRT C APIs (not
recommended) and one that use XRT C++ APIs.  

The sample host code shows how to convert a raw xclBufferHandle
to an xrt::bo object which is then used as a kernel argument.

% g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o xcl.exe xcl.cpp -lxrt_coreutil -lxrt_core -luuid -pthread
% xcl.exe -k verify.xclbin [--api <c | cpp>]  (default is cpp)
****************************************************************/
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

#include "xrt.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

static constexpr int ARRAY_SIZE = 20;
static constexpr int LENGTH = 20;
static constexpr char gold[] = "Hello World\n";

static void
usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n"
              << "  -k <bitstream>\n"
              << "  -d <index>\n"
              << "  -h\n\n"
              << "  [--api <c | cpp>]  Specify API style (default: cpp)\n"
              << "* Bitstream is required\n";
}

static std::vector<char>
read_xclbin(const std::string& fnm)
{
  if (fnm.empty())
    throw std::runtime_error("No xclbin specified");

  // load the file
  std::ifstream stream(fnm);
  if (!stream)
    throw std::runtime_error("Failed to open file '" + fnm + "' for reading");

  stream.seekg(0, stream.end);
  size_t size = stream.tellg();
  stream.seekg(0, stream.beg);

  std::vector<char> header(size);
  stream.read(header.data(), size);
  return header;
}


static void
run_c(xclDeviceHandle dhdl)
{
  auto device = xrtDeviceOpenFromXcl(dhdl);

  xuid_t xuid;
  xrtDeviceGetXclbinUUID(device, xuid);
  
  auto hello  = xrtPLKernelOpen(device, xuid, "hello:{hello_1}");
  auto bank = xrtKernelArgGroupId(hello, 0);
  
  // Allocate buffer object via xcl
  auto xclbo = xclAllocBO(dhdl, 1024, 0, XRT_BO_FLAGS_NONE | bank);
  auto xclbo_data = reinterpret_cast<char*>(xclMapBO(dhdl, xclbo, true));
  std::memset(xclbo_data, 0, 1024);
  xclSyncBO(dhdl, xclbo, XCL_BO_SYNC_BO_TO_DEVICE, 1024, 0);

  // Bridge to xrt::bo so that xrt::kernel can be used
  // In order to disambiguate the untyped xclBufferHandle,
  // it must be wrapped in typed xcl_buffer_handle.
  auto xrtbo = xrtBOAllocFromXcl(device, xclbo);

  // Start kernel run
  auto run = xrtKernelRun(hello, xrtbo);
  std::cout << "Kernel start command issued" << std::endl;
  std::cout << "Now wait until the kernel finish" << std::endl;

  // Wait for kernel to complete
  xrtRunWait(run);

  // None of the xrt objects are needed any more
  xrtRunClose(run);
  xrtBOFree(xrtbo);
  xrtKernelClose(hello);
  xrtDeviceClose(device);

  // Get the output from the xcl buffer
  std::cout << "Get the output data from the device" << std::endl;
  xclSyncBO(dhdl, xclbo, XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0);

  // Map BO
  std::cout << "RESULT: " << std::endl;
  for (unsigned i = 0; i < 20; ++i)
    std::cout << xclbo_data[i];
  std::cout << std::endl;
  if (!std::equal(std::begin(gold), std::end(gold), xclbo_data))
    throw std::runtime_error("Incorrect value obtained");

  // Finally free the xclBuffer
  xclFreeBO(dhdl, xclbo);
}

static void
run_cpp(xclDeviceHandle dhdl)
{
  auto device = xrt::device{dhdl};
  auto uuid = device.get_xclbin_uuid();

  auto hello = xrt::kernel(device, uuid.get(), "hello:{hello_1}");
  auto bank = hello.group_id(0);
  
  // Allocate buffer object via xcl
  auto xclbo = xclAllocBO(dhdl, 1024, 0, XRT_BO_FLAGS_NONE | bank);
  auto xclbo_data = reinterpret_cast<char*>(xclMapBO(dhdl, xclbo, true));
  std::memset(xclbo_data, 0, 1024);
  xclSyncBO(dhdl, xclbo, XCL_BO_SYNC_BO_TO_DEVICE, 1024, 0);

  // Bridge to xrt::bo so that xrt::kernel can be used
  // In order to disambiguate the untyped xclBufferHandle,
  // it must be wrapped in typed xcl_buffer_handle.
  auto bo = xrt::bo(device, xcl_buffer_handle{xclbo});

  auto run = hello(bo);
  std::cout << "Kernel start command issued" << std::endl;
  std::cout << "Now wait until the kernel finish" << std::endl;

  run.wait();

  // Get the output
  std::cout << "Get the output data from the device" << std::endl;
  xclSyncBO(dhdl, xclbo, XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0);

  // Map BO
  std::cout << "RESULT: " << std::endl;
  for (unsigned i = 0; i < 20; ++i)
    std::cout << xclbo_data[i];
  std::cout << std::endl;
  if (!std::equal(std::begin(gold), std::end(gold), xclbo_data))
    throw std::runtime_error("Incorrect value obtained");

  // Free the BO
  xclFreeBO(dhdl, xclbo);
}


static int
run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  unsigned int device_index = 0;
  bool cpp = true;

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
      device_index = std::stoi(arg);
    else if (cur == "--api" && arg == "c")
      cpp = false;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  // Use shim core APIs for opening device and loading xclbin
  auto xclbin = read_xclbin(xclbin_fnm);
  auto dhdl = xclOpen(device_index, nullptr, XCL_QUIET);
  xclLoadXclBin(dhdl, reinterpret_cast<const axlf*>(xclbin.data()));

  // Demonstrate how to go from shim level handes to XRT handles
  if (cpp)
    run_cpp(dhdl);
  else 
    run_c(dhdl);
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
  }
  catch (...) {
    std::cout << "TEST FAILED\n";
  }
  return 1;
}
