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

#include "xdp/profile/database/events/device_events.h"

namespace xdp {
  
  // **************************
  // Device event definitions
  // **************************

  VTFDeviceEvent::VTFDeviceEvent(uint64_t s_id, double ts, VTFEventType ty,
				 void* d) :
    VTFEvent(s_id, ts, ty), dev(d)
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
    fout << std::fixed << std::setprecision(6) << timestamp ;
  }

  KernelDeviceEvent::KernelDeviceEvent(uint64_t s_id, double ts, 
				       void* d) :
    VTFDeviceEvent(s_id, ts, KERNEL, d)
  {
  }

  KernelDeviceEvent::~KernelDeviceEvent()
  {
  }

  KernelStall::KernelStall(uint64_t s_id, double ts, void* d) :
    VTFDeviceEvent(s_id, ts, KERNEL_STALL, d)
  {
  }

  KernelStall::~KernelStall()
  {
  }

  KernelMemoryAccess::KernelMemoryAccess(uint64_t s_id, double ts, 
					 VTFEventType ty, void* d) :
    VTFDeviceEvent(s_id, ts, ty, d)
  {
  }

  KernelMemoryAccess::~KernelMemoryAccess()
  {
  }

  KernelStreamAccess::KernelStreamAccess(uint64_t s_id, double ts,
					 VTFEventType ty, void* d) :
    VTFDeviceEvent(s_id, ts, ty, d)
  {
  }

  KernelStreamAccess::~KernelStreamAccess()
  {
  }

  KernelStreamStall::KernelStreamStall(uint64_t s_id, double ts, 
					 void* d) :
    VTFDeviceEvent(s_id, ts, KERNEL_STREAM_STALL, d)
  {
  }

  KernelStreamStall::~KernelStreamStall()
  {
  }

  KernelStreamStarve::KernelStreamStarve(uint64_t s_id, double ts, 
					 void* d) :
    VTFDeviceEvent(s_id, ts, KERNEL_STREAM_STARVE, d)
  {
  }

  KernelStreamStarve::~KernelStreamStarve()
  {
  }

  HostRead::HostRead(uint64_t s_id, double ts, void* d) :
    VTFDeviceEvent(s_id, ts, HOST_READ, d)
  {
  }

  HostRead::~HostRead()
  {
  }

  HostWrite::HostWrite(uint64_t s_id, double ts, void* d) :
    VTFDeviceEvent(s_id, ts, HOST_WRITE, d)
  {
  }

  HostWrite::~HostWrite()
  {
  }

} // end namespace xdp
