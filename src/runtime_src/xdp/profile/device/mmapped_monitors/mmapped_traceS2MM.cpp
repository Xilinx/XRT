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

#include "mmapped_traceS2MM.h"

namespace xdp {

MMappedTraceS2MM::MMappedTraceS2MM(Device* handle, uint64_t index, uint64_t instIdx, debug_ip_data* data)
                : TraceS2MM(handle, index, data),
                  instance_index(instIdx)
{
  // Open TraceS2MM Device Driver File
  std::string subDev("trace_s2mm");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, instance_index);

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
}

MMappedTraceS2MM::~MMappedTraceS2MM()
{
  munmap(mapped_device, PROFILE_IP_SZ);
  close(driver_FD);
}

bool MMappedTraceS2MM::isMMapped()
{
  if(mapped_device == nullptr || mapped_device == MAP_FAILED) {
    return false;
  }
  return true;
}

int MMappedTraceS2MM::read(uint64_t offset, size_t size, void* data)
{
  if(!isMMapped()) {
    return 0;
  }
  memcpy((char*)data, mapped_device + offset, size);
  return size;
}

int MMappedTraceS2MM::write(uint64_t offset, size_t size, void* data)
{
  if(!isMMapped()) {
    return 0;
  }
  if(size == sizeof(uint32_t)) {
    // Special case for 4 bytes write to improve performance
    memcpy(mapped_device + offset, (uint32_t*)data, sizeof(uint32_t));
    return size;
  }
  size_t numWords = size / sizeof(uint32_t);
  size_t remBytes = size % sizeof(uint32_t);
  for(size_t i = 0; i < numWords ; i++) {
    memcpy(((uint32_t*)(mapped_device + offset))+i, ((uint32_t*)data) + i, sizeof(uint32_t));
  }
  if(remBytes) {
    memcpy(((uint32_t*)(mapped_device + offset))+numWords, ((uint32_t*)data) + numWords, remBytes);
  }
  return size;
}

}
#endif
