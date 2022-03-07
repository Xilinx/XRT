// Copyright (C) 2022 Xilinx, Inc
// SPDX-License-Identifier: Apache-2.0
#include <xrt/xrt_device.h>
#include <experimental/xrt_system.h>

#include <iostream>

/****************************************************************
Test driver for MCMD work-in-progress.
Build with CMake using parent directly build.sh on WSL
****************************************************************/

int
run()
{
  auto devices = xrt::system::enumerate_devices();
  std::cout << "number of devices: " << devices << "\n";
  xrt::device d(0);
  return 0;
}

int main()
{
  try {
    return run();
  }
  catch (const std::exception& ex) {
    std::cout << ex.what() << "\n";
  }
  return 1;
}
