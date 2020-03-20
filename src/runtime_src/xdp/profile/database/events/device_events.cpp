/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include <iomanip>

#define XDP_SOURCE

#include "xdp/profile/database/events/device_events.h"

namespace xdp {
  
  // **************************
  // Device event definitions
  // **************************

  VTFDeviceEvent::VTFDeviceEvent(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId)
                : VTFEvent(s_id, ts, ty),
                  deviceId(devId),
                  deviceTimestamp(0)
  {
  }

  VTFDeviceEvent::~VTFDeviceEvent()
  {
  }

  void VTFDeviceEvent::dumpTimestamp(std::ofstream& fout)
  {
    // Device events are accurate up to nanoseconds.
    //  Timestamps are in milliseconds, so we should print up to 
    //  6 past the decimal point
    std::ios_base::fmtflags flags = fout.flags() ;
    fout << std::fixed << std::setprecision(6) << timestamp ;
    fout.flags(flags) ;
  }

  void VTFDeviceEvent::dump(std::ofstream& fout, uint32_t bucket)
  { 
    VTFEvent::dump(fout, bucket) ;
    fout << std::endl;
  } 

  KernelEvent::KernelEvent(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId, int32_t cuIdx)
                   : VTFDeviceEvent(s_id, ts, ty, devId),
					 cuId(cuIdx)
  {
  }

  KernelEvent::~KernelEvent()
  {
  }

  KernelDeviceEvent::KernelDeviceEvent(uint64_t s_id, double ts, uint64_t devId, int32_t cuIdx)
                   : KernelEvent(s_id, ts, KERNEL, devId, cuIdx)
  {
  }

  KernelDeviceEvent::~KernelDeviceEvent()
  {
  }

  KernelStall::KernelStall(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId)
             : KernelEvent(s_id, ts, ty, devId),
    // Until implemented, provide a default value for all members
    burstLength(0)
  {
  }

  KernelStall::~KernelStall()
  {
  }

  KernelMemoryAccess::KernelMemoryAccess(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId)
                    : KernelEvent(s_id, ts, ty, devId),
    // Until implemented, provide a default value for all members
    portName(0), memoryName(0), argumentNames(0), burstLength(0),
    numBytes(0)
  {
  }

  KernelMemoryAccess::~KernelMemoryAccess()
  {
  }

  KernelStreamAccess::KernelStreamAccess(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId)
                    : KernelEvent(s_id, ts, ty, devId),
    // Until implemented, provide a default value for all members
    portName(0), streamName(0), burstLength(0)
  {
  }

  KernelStreamAccess::~KernelStreamAccess()
  {
  }

  HostRead::HostRead(uint64_t s_id, double ts, uint64_t devId)
          : VTFDeviceEvent(s_id, ts, HOST_READ, devId)
  {
  }

  HostRead::~HostRead()
  {
  }

  HostWrite::HostWrite(uint64_t s_id, double ts, uint64_t devId)
           : VTFDeviceEvent(s_id, ts, HOST_WRITE, devId)
  {
  }

  HostWrite::~HostWrite()
  {
  }

} // end namespace xdp
