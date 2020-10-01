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

#include "ioctl_asm.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlASM::IOCtlASM(Device* handle, uint64_t index, uint64_t instIdx, debug_ip_data* data)
          : ASM(handle, index, data),
            instance_index(instIdx)
{
  // Open ASM Device Driver File
  std::string subDev("axistream_mon");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, instance_index);

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

IOCtlASM::~IOCtlASM()
{
  close(driver_FD);
}

bool IOCtlASM::isOpened()
{
  if(-1 == driver_FD) {
    return false;
  }
  return true;
}

size_t IOCtlASM::startCounter()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlASM::startCounter " << std::endl;

  ioctl(driver_FD, ASM_IOC_RESET);
  ioctl(driver_FD, ASM_IOC_STARTCNT);
  return 0;
}

size_t IOCtlASM::stopCounter()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlASM::stopCounter " << std::endl;

  ioctl(driver_FD, ASM_IOC_STOPCNT);
  return 0;
}

size_t IOCtlASM::readCounter(xclCounterResults& counterResults, uint32_t s)
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlASM::readCounter " << std::endl;

  struct asm_counters counter = { 0 };
  ioctl(driver_FD, ASM_IOC_READCNT, &counter);

  counterResults.StrNumTranx[s]     = counter.num_tranx;
  counterResults.StrDataBytes[s]    = counter.data_bytes;
  counterResults.StrBusyCycles[s]   = counter.busy_cycles;
  counterResults.StrStallCycles[s]  = counter.stall_cycles;
  counterResults.StrStarveCycles[s] = counter.starve_cycles;

  // AXIS without TLAST is assumed to be one long transfer
  if(counterResults.StrNumTranx[s] == 0 && counterResults.StrDataBytes[s] > 0) {
    counterResults.StrNumTranx[s] = 1;
  }

  if(out_stream) {
    (*out_stream) << "Reading IOCtl AXI Stream Monitor... SlotNum : " << s << std::endl
                  << "Reading IOCtl AXI Stream Monitor... NumTranx : " << counterResults.StrNumTranx[s] << std::endl
                  << "Reading IOCtl AXI Stream Monitor... DataBytes : " << counterResults.StrDataBytes[s] << std::endl
                  << "Reading IOCtl AXI Stream Monitor... BusyCycles : " << counterResults.StrBusyCycles[s] << std::endl
                  << "Reading IOCtl AXI Stream Monitor... StallCycles : " << counterResults.StrStallCycles[s] << std::endl
                  << "Reading IOCtl AXI Stream Monitor... StarveCycles : " << counterResults.StrStarveCycles[s] << std::endl;
  }

  return 0;
}

size_t IOCtlASM::triggerTrace(uint32_t traceOption /* starttrigger*/)
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlASM::triggerTrace " << std::endl;

  ioctl(driver_FD, ASM_IOC_STARTTRACE, &traceOption);
  return 0;
}

int IOCtlASM::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlASM::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
