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

#ifndef DEVICE_EVENTS_DOT_H
#define DEVICE_EVENTS_DOT_H

#include "xdp/profile/database/events/vtf_event.h"

#include "xdp/config.h"

namespace xdp {

  enum KernelStallType
  {
    EXTERNAL_MEMORY_STALL,
    DATAFLOW_STALL,
    PIPE_STALL,
    UNKNOWN_STALL
  } ;

  // **********************
  // Device events
  // **********************
  class VTFDeviceEvent : public VTFEvent
  {
  private:
    void* dev ; // Either a device handle or an xrt::device

    VTFDeviceEvent() = delete ;

  protected:
    virtual void dumpTimestamp(std::ofstream& fout) ;

  public:
    XDP_EXPORT VTFDeviceEvent(uint64_t s_id, double ts, VTFEventType ty, 
			      void* d) ;
    XDP_EXPORT ~VTFDeviceEvent() ;

    virtual bool isDeviceEvent() { return false ; }
    virtual void* getDevice()    { return dev ; } 
  } ;

  class KernelDeviceEvent : public VTFDeviceEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t workgroupConfiguration ;
    uint64_t cuName ;

    KernelDeviceEvent() = delete ;
  public:
    XDP_EXPORT KernelDeviceEvent(uint64_t s_id, double ts, void* d) ;
    XDP_EXPORT ~KernelDeviceEvent() ;
  } ;

  class KernelStall : public VTFDeviceEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t cuName ;
    KernelStallType stallType ;
    uint16_t burstLength ;

    KernelStall() = delete ;
  public:
    XDP_EXPORT KernelStall(uint64_t s_id, double ts, void* d) ;
    XDP_EXPORT ~KernelStall() ;
  } ;

  class KernelMemoryAccess : public VTFDeviceEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t cuName ;
    uint64_t portName ;
    uint64_t memoryName ;
    uint64_t argumentNames ;
    uint16_t burstLength ;
    uint16_t numBytes ;

    KernelMemoryAccess() = delete ;
  public:
    XDP_EXPORT KernelMemoryAccess(uint64_t s_id, double ts, VTFEventType ty, 
				  void* d) ;
    XDP_EXPORT ~KernelMemoryAccess() ;
  } ;

  class KernelStreamAccess : public VTFDeviceEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t cuName ;
    uint64_t portName ;
    uint64_t streamName ;
    uint16_t burstLength ;

    KernelStreamAccess() = delete ;
  public:
    XDP_EXPORT KernelStreamAccess(uint64_t s_id, double ts, VTFEventType ty, 
				  void* d) ;
    XDP_EXPORT ~KernelStreamAccess() ;
  } ;

  class KernelStreamStall : public VTFDeviceEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t cuName ;
    uint64_t portName ;
    uint64_t streamName ;

    KernelStreamStall() = delete ;
  public:
    XDP_EXPORT KernelStreamStall(uint64_t s_id, double ts, void* d) ;
    XDP_EXPORT ~KernelStreamStall() ;
  } ;

  class KernelStreamStarve : public VTFDeviceEvent
  {
  private:
    uint64_t deviceName ;
    uint64_t binaryName ;
    uint64_t kernelName ;
    uint64_t cuName ;
    uint64_t portName ;
    uint64_t streamName ;

    KernelStreamStarve() = delete ;
  public:
    XDP_EXPORT KernelStreamStarve(uint64_t s_id, double ts, void* d) ;
    XDP_EXPORT ~KernelStreamStarve() ;
  } ;

  class HostRead : public VTFDeviceEvent
  {
  private:
    HostRead() = delete ;
  public:
    XDP_EXPORT HostRead(uint64_t s_id, double ts, void* d) ;
    XDP_EXPORT ~HostRead() ;
  } ;

  class HostWrite : public VTFDeviceEvent
  {
  private:
    HostWrite() = delete ;
  public:
    XDP_EXPORT HostWrite(uint64_t s_id, double ts, void* d) ;
    XDP_EXPORT ~HostWrite() ;
  } ;

} // end namespace xdp

#endif
