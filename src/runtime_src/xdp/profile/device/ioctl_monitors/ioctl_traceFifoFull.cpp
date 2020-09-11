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

#include "ioctl_traceFifoFull.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlTraceFifoFull::IOCtlTraceFifoFull(Device* handle, uint64_t index, debug_ip_data* data)
                    : TraceFifoFull(handle, index, data)
{
/*
 * Base address of TraceFifoFull is actually not used in any mapped read/write.
 * Only unmanaged read with DMA is used.
 * So, open+ioctl for TraceFifoFull is not needed.
 * This specialised class is only for consistency with other profile monitors which
 * use device driver files and ioctls to read/write registers
 */

#if 0
  // Open TraceFifoFull Device Driver File
  std::string subDev("trace_fifo_full");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, 0 /* a design can have atmost 1 TraceFifoFull*/);

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
#endif
}

IOCtlTraceFifoFull::~IOCtlTraceFifoFull()
{
#if 0
  close(driver_FD);
#endif
}

bool IOCtlTraceFifoFull::isOpened()
{
  return true;
}

int IOCtlTraceFifoFull::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlTraceFifoFull::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
