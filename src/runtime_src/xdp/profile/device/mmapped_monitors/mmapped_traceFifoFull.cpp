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
#include <unistd.h>
#include <string.h>

#include "mmapped_traceFifoFull.h"

namespace xdp {

MMappedTraceFifoFull::MMappedTraceFifoFull(Device* handle, uint64_t index, debug_ip_data* data)
                    : TraceFifoFull(handle, index, data)
{
/*
 * Base address of TraceFifoFull is actually not used in any mapped read/write.
 * Only unmanaged read with DMA is used.
 * So, open-mmap for TraceFifoFull is not needed.
 * This specialised class is only for consistency with other profile monitors which
 * use device driver files mmapped to virtual memory address space.
 */
#if 0
  // Open TraceFifoFull Device Driver File
  std::string subDev("trace_fifo_full");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, 0 /* a design can have atmost 1 TraceFifoFull*/);

  driver_FD = open(driverFileName.c_str(), O_RDWR);
  if(-1 == driver_FD) {
    showWarning("Could not open device file.");
    return;
  }

  // mmap opened device driver file
  mapped_device = (char*)mmap(NULL, PROFILE_IP_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, driver_FD, 0);
  if(mapped_device == MAP_FAILED) {
    showWarning("mmap failed for device file.");
    return;
  }
#endif
}

MMappedTraceFifoFull::~MMappedTraceFifoFull()
{
#if 0
  munmap(mapped_device, PROFILE_IP_SZ);
  close(driver_FD);
#endif
}

bool MMappedTraceFifoFull::isMMapped()
{
#if 0
  if(mapped_device == nullptr || mapped_device == MAP_FAILED) {
    return false;
  }
#endif
  return true;
}

int MMappedTraceFifoFull::read(uint64_t /*offset*/, size_t /*size*/, void* /*data*/)
{
  return 0;
#if 0
  if(!isMMapped()) {
    return 0;
  }
  memcpy((char*)data, mapped_device + offset, size);
  return size;
#endif
}

int MMappedTraceFifoFull::write(uint64_t /*offset*/, size_t /*size*/, void* /*data*/)
{
  return 0;
#if 0
  if(!isMMapped()) {
    return 0;
  }
  memcpy(mapped_device + offset, (char*)data, size);
  return size;
#endif
}
}
#endif
