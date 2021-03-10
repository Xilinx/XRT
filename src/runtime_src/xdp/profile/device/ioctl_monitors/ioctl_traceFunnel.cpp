/*
 * Copyright (C) 2020 Xilinx Inc - All rights reserved
 * Xilinx Debug & Profile (XDP) APIs
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

#ifndef _WIN32

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include <chrono>
#include <thread>

#include "ioctl_traceFunnel.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlTraceFunnel::IOCtlTraceFunnel(Device* handle, uint64_t index, debug_ip_data* data)
                    : TraceFunnel(handle, index, data)
{
  // Open TraceFunnel Device Driver File
  std::string subDev("trace_funnel");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, 0 /* a design can have atmost 1 TraceFunnel*/);

  driver_FD = open(driverFileName.c_str(), O_RDWR);
  uint32_t tries = 0;
  while(-1 == driver_FD && tries < 5) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    driver_FD = open(driverFileName.c_str(), O_RDWR);
    tries++;
  }

  if(-1 == driver_FD) {
    showWarning("Could not open device file.");
    return;
  }
}

IOCtlTraceFunnel::~IOCtlTraceFunnel()
{
  close(driver_FD);
}

bool IOCtlTraceFunnel::isOpened()
{
  if(-1 == driver_FD) {
    return false;
  }
  return true;
}

size_t IOCtlTraceFunnel::initiateClockTraining()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlTraceFunnel::initiateClockTraining " << std::endl;

  for(int i = 0; i < 2 ; i++) {
    uint64_t hostTimeStamp = getDevice()->getTraceTime();
    ioctl(driver_FD, TR_FUNNEL_IOC_TRAINCLK, &hostTimeStamp);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
  return 0;
}

void IOCtlTraceFunnel::reset()
{
  if(!isOpened()) {
    return;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlTraceFunnel::reset " << std::endl;

  ioctl(driver_FD, TR_FUNNEL_IOC_RESET);
}

int IOCtlTraceFunnel::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlTraceFunnel::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
