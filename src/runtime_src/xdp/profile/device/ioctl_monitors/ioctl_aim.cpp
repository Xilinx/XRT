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

#include "ioctl_aim.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlAIM::IOCtlAIM(Device* handle, uint64_t index, uint64_t instIdx, debug_ip_data* data)
          : AIM(handle, index, data),
            instance_index(instIdx)
{
  // Open AIM Device Driver File
  std::string subDev("aximm_mon");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, instance_index);

  driver_FD = open(driverFileName.c_str(), O_RDWR);
  if(-1 == driver_FD) {
    showWarning("Could not open device file.");
    return;
  }
}

IOCtlAIM::~IOCtlAIM()
{
  close(driver_FD);
}

bool IOCtlAIM::isOpened()
{
  if(-1 == driver_FD) {
    return false;
  }
  return true;
}

size_t IOCtlAIM::startCounter()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlAIM::startCounter " << std::endl;

  ioctl(driver_FD, AIM_IOC_RESET);
  ioctl(driver_FD, AIM_IOC_STARTCNT);
  return 0;
}

size_t IOCtlAIM::stopCounter()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlAIM::stopCounter " << std::endl;

  ioctl(driver_FD, AIM_IOC_STOPCNT);
  return 0;
}

size_t IOCtlAIM::readCounter(xclCounterResults& counterResults, uint32_t s)
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlAIM::readCounter " << std::endl;

  uint32_t sampleInterval = 0;
  if (s==0 && getDevice()) {
    counterResults.SampleIntervalUsec = static_cast<float>(sampleInterval / (getDevice()->getDeviceClock()));
  }

  struct aim_counters counter = { 0 };
  ioctl(driver_FD, AIM_IOC_READCNT, &counter);

  counterResults.WriteBytes[s]      = counter.wr_bytes;
  counterResults.WriteTranx[s]      = counter.wr_tranx;
  counterResults.WriteLatency[s]    = counter.wr_latency;
  counterResults.ReadBytes[s]       = counter.rd_bytes;
  counterResults.ReadTranx[s]       = counter.rd_tranx;
  counterResults.ReadLatency[s]     = counter.rd_latency;
  counterResults.ReadBusyCycles[s]  = counter.rd_busy_cycles;
  counterResults.WriteBusyCycles[s] = counter.wr_busy_cycles;

  return 0;
}

size_t IOCtlAIM::triggerTrace(uint32_t traceOption /* starttrigger*/)
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlAIM::triggerTrace " << std::endl;

  ioctl(driver_FD, AIM_IOC_STARTTRACE, &traceOption);
  return 0;
}

int IOCtlAIM::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlAIM::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
