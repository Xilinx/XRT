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
#include <thread>
#include <chrono>

#include "ioctl_traceFifoLite.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlTraceFifoLite::IOCtlTraceFifoLite(Device* handle, uint64_t index, debug_ip_data* data)
                    : TraceFifoLite(handle, index, data)
{
  // Open TraceFifoLite Device Driver File
  std::string subDev("trace_fifo_lite");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, 0 /* a design can have atmost 1 TraceFifoLite*/);

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

IOCtlTraceFifoLite::~IOCtlTraceFifoLite()
{
  close(driver_FD);
}

bool IOCtlTraceFifoLite::isOpened()
{
  if(-1 == driver_FD) {
    return false;
  }
  return true;
}

size_t IOCtlTraceFifoLite::reset()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlTraceFifoLite::reset " << std::endl;

  ioctl(driver_FD, TR_FIFO_IOC_RESET);

  return 0;
}

uint32_t IOCtlTraceFifoLite::getNumTraceSamples()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlTraceFifoLite::getNumTraceSamples " << std::endl;

  uint32_t numBytes = 0;
  ioctl(driver_FD, TR_FIFO_IOC_GET_NUMBYTES, &numBytes);

  uint32_t numSamples = 0;
  numSamples = numBytes / (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH/8);

  if(out_stream)
    (*out_stream) << "  No. of trace samples = " << numSamples << std::endl;
  
  return numSamples;
} 

int IOCtlTraceFifoLite::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlTraceFifoLite::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
