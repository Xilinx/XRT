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

#include "ioctl_traceS2MM.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlTraceS2MM::IOCtlTraceS2MM(Device* handle, uint64_t index, uint64_t instIdx, debug_ip_data* data)
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
}

IOCtlTraceS2MM::~IOCtlTraceS2MM()
{
  close(driver_FD);
}

bool IOCtlTraceS2MM::isOpened()
{
  if(-1 == driver_FD) {
    return false;
  }
  return true;
}

void IOCtlTraceS2MM::init(uint64_t bo_size, int64_t bufaddr)
{
  if(!isOpened()) {
    return;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlTraceS2MM::init " << std::endl;

  struct ts2mm_config cfg = { bo_size, static_cast<uint64_t>(bufaddr), false };
  ioctl(driver_FD, TR_S2MM_IOC_START, &cfg);
}

void IOCtlTraceS2MM::reset()
{
  if(!isOpened()) {
    return;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlTraceS2MM::reset " << std::endl;

  ioctl(driver_FD, TR_S2MM_IOC_RESET);

  mPacketFirstTs = 0;
  mPartialTs = 0;
  mModulus = 0;
  mclockTrainingdone = false;
}

uint64_t IOCtlTraceS2MM::getWordCount()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlTraceS2MM::getWordCount " << std::endl;

  uint64_t wordCnt = 0;
  ioctl(driver_FD, TR_S2MM_IOC_GET_WORDCNT, &wordCnt);

  return wordCnt;
}

int IOCtlTraceS2MM::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlTraceS2MM::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
