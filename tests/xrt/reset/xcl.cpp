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

// To build manually:
// % g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o xcl.exe xcl.cpp -lxrt_coreutil -lxrt_core -pthread

// This test demonstrates application signal handler to
// catch xbutil reset, close device, and exit gracefully
// **** Shim level xclDeviceHandle for demo only, do not use. ****
#include "xrt.h"
#include "xrt/xrt_device.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <signal.h>

static std::mutex mutex;
static std::condition_variable cond;
static bool reset = false;

static xclDeviceHandle dhdl;

static void
usage()
{
  std::cout << "xcl.exe [-d <device>]\n";
}

// sigbus handler to capture 'xbutil reset'
static void
SigBusHandler(int sig)
{
  std::lock_guard<std::mutex> lk(mutex);
  std::cout  << "-> sig bus handler\n";

  // Create xrt::device from already opened xclDeviceHandle
  xrt::device d(dhdl);
  if (!d.get_info<xrt::info::device::offline>())
    throw std::runtime_error("Device is unexpectedly online");

  // Close device gracefully before existing on device reset
  std::cout << "Calling xclClose()\n";
  xclClose(dhdl);

  reset = true;
  cond.notify_all();
  std::cout  << "<- sig bus handler\n";
}

// sigint handler to capture 'ctrl-c'
static void
SigIntHandler(int sig)
{
  std::lock_guard<std::mutex> lk(mutex);
  std::cout  << "-> sig int handler\n";

  // Create xrt::device from already opened xclDeviceHandle
  xrt::device d(dhdl);
  if (d.get_info<xrt::info::device::offline>())
    throw std::runtime_error("Device is unexpectedly offline");

  // Close device gracefully before existing on ctrl-c
  std::cout << "Calling xclClose()\n";
  xclClose(dhdl);

  reset = true;
  cond.notify_all();
  std::cout  << "<- sig int handler\n";
}

static void
install()
{
  signal(SIGBUS, SigBusHandler);
  signal(SIGINT, SigIntHandler);
}

int
run(int argc, char* argv[])
{
  unsigned int device_index = 0;

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

    if (cur == "-d")
      device_index = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  install();
  dhdl = xclOpen(device_index, nullptr, XCL_QUIET);

  // Wait for reset
  std::unique_lock<std::mutex> lk(mutex);
  while (!reset)
    cond.wait(lk);
}

int main(int argc, char* argv[])
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
