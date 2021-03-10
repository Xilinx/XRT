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

#include "ioctl_am.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlAM::IOCtlAM(Device* handle, uint64_t index, uint64_t instIdx, debug_ip_data* data)
          : AM(handle, index, data),
            instance_index(instIdx)
{
  // Open AM Device Driver File
  std::string subDev("accel_mon");
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

IOCtlAM::~IOCtlAM()
{
  close(driver_FD);
}

bool IOCtlAM::isOpened()
{
  if(-1 == driver_FD) {
    return false;
  }
  return true;
}

size_t IOCtlAM::startCounter()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlAM::startCounter " << std::endl;

  ioctl(driver_FD, AM_IOC_RESET);
  ioctl(driver_FD, AM_IOC_STARTCNT);
  return 0;
}

size_t IOCtlAM::stopCounter()
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlAM::stopCounter " << std::endl;

  ioctl(driver_FD, AM_IOC_STOPCNT);
  return 0;
}

size_t IOCtlAM::readCounter(xclCounterResults& counterResults, uint32_t s)
{
  if(!isOpened()) {
    return 0;
  }

  if (!m_enabled)
    return 0;
 
  if(out_stream)
    (*out_stream) << " IOCtlAM::readCounter " << std::endl;


  if(out_stream) {
    (*out_stream) << "IOCtlAM :: Accelerator Monitor config : "
                  << " 64 bit support : " << has64bit()
                  << " Dataflow support : " << hasDataflow()
                  << " Stall support : " << hasStall()
                  << std::endl;
  }

  struct am_counters counters = { 0 };
  ioctl(driver_FD, AM_IOC_READCNT, &counters);

  counterResults.CuExecCount[s] = counters.end_count;
  counterResults.CuExecCycles[s] = counters.exec_cycles;
  counterResults.CuMinExecCycles[s] = counters.min_exec_cycles;
  counterResults.CuMaxExecCycles[s] = counters.max_exec_cycles;

  if(hasDataflow()) {
    counterResults.CuBusyCycles[s] = counters.busy_cycles;
    counterResults.CuMaxParallelIter[s] = counters.max_parallel_iterations;
  } else {
    counterResults.CuBusyCycles[s] = counterResults.CuExecCycles[s];
    counterResults.CuMaxParallelIter[s] = 1;
  }
    

  if(out_stream) {
    (*out_stream) << "Reading IOCtl Accelerator Monitor... " << std::endl 
                  << "SlotNum : " << s << std::endl
                  << "CuExecCount : " << counterResults.CuExecCount[s] << std::endl
                  << "CuExecCycles : " << counterResults.CuExecCycles[s] << std::endl
                  << "CuMinExecCycles : " << counterResults.CuMinExecCycles[s] << std::endl
                  << "CuMaxExecCycles : " << counterResults.CuMaxExecCycles[s] << std::endl
                  << "CuBusyCycles : " << counterResults.CuBusyCycles[s] << std::endl
                  << "CuMaxParallelIter : " << counterResults.CuMaxParallelIter[s] << std::endl;
  }

  if(hasStall()) {
    counterResults.CuStallIntCycles[s] = counters.stall_int_cycles;
    counterResults.CuStallStrCycles[s] = counters.stall_str_cycles;
    counterResults.CuStallExtCycles[s] = counters.stall_ext_cycles;
  }

  if(out_stream) {
    (*out_stream) << "Stall Counters enabled : " << std::endl
                  << "CuStallIntCycles : " << counterResults.CuStallIntCycles[s] << std::endl
                  << "CuStallStrCycles : " << counterResults.CuStallStrCycles[s] << std::endl
                  << "CuStallExtCycles : " << counterResults.CuStallExtCycles[s] << std::endl;
  }
  return 0;
}

void IOCtlAM::disable()
{
  m_enabled = false;
  // Disable all trace
  ioctl(driver_FD, AM_IOC_STOPTRACE);
}

void IOCtlAM::configureDataflow(bool cuHasApCtrlChain)
{
  // this ipConfig only tells whether the corresponding CU has ap_control_chain :
  // could have been just a property on the monitor set at compile time (in debug_ip_layout)
  if(!cuHasApCtrlChain)
    return;

  uint32_t option = 1 ; // cuHasApCtrlChain == true here
  ioctl(driver_FD, AM_IOC_CONFIGDFLOW, &option);

  if(out_stream) {
    (*out_stream) << "Dataflow enabled on slot : " << getName() << std::endl;
  }
}


size_t IOCtlAM::triggerTrace(uint32_t traceOption /* starttrigger*/)
{
  if(!isOpened()) {
    return 0;
  }
 
  if(out_stream)
    (*out_stream) << " IOCtlAM::triggerTrace " << std::endl;

  ioctl(driver_FD, AM_IOC_STARTTRACE, &traceOption);
  return 0;
}

int IOCtlAM::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlAM::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
