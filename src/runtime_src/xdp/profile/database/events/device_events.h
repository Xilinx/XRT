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

  // **********************
  // Device events
  // **********************
  class VTFDeviceEvent : public VTFEvent
  {
  private:
    uint64_t deviceId ; // Either a device handle or an xrt::device
    double   deviceTimestamp;

    VTFDeviceEvent() = delete ;

  protected:
    virtual void dumpTimestamp(std::ofstream& fout) ;

  public:
    XDP_EXPORT VTFDeviceEvent(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId);
    XDP_EXPORT ~VTFDeviceEvent() ;

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket);

    virtual bool isDeviceEvent() { return true ; }
    virtual uint64_t getDevice() { return deviceId ; }

    virtual void   setDeviceTimestamp(double deviceTime) { deviceTimestamp = deviceTime; }
    virtual double getDeviceTimestamp() { return deviceTimestamp; }
  } ;

  class KernelEvent : public VTFDeviceEvent
  {
  protected:
    int32_t cuId;

    KernelEvent() = delete ;
  public:
    XDP_EXPORT KernelEvent(uint64_t s_id, double ts, VTFEventType ty, 
                   uint64_t devId, int32_t cuIdx = 0);
    XDP_EXPORT ~KernelEvent() ;
    virtual int32_t getCUId() { return cuId;} 
  };

  class KernelDeviceEvent : public KernelEvent
  {
  private:
    KernelDeviceEvent() = delete ;
  public:
    XDP_EXPORT KernelDeviceEvent(uint64_t s_id, double ts, uint64_t devId, int32_t cuIdx);
    XDP_EXPORT ~KernelDeviceEvent() ;
  } ;

  class KernelStall : public KernelEvent
  {
  private:
    uint16_t burstLength ;

    KernelStall() = delete ;
  public:
    XDP_EXPORT KernelStall(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId) ;
    XDP_EXPORT ~KernelStall() ;
  } ;

  class KernelMemoryAccess : public KernelEvent
  {
  private:
    uint64_t portName ;
    uint64_t memoryName ;
    uint64_t argumentNames ;
    uint16_t burstLength ;
    uint16_t numBytes ;

    KernelMemoryAccess() = delete ;
  public:
    XDP_EXPORT KernelMemoryAccess(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId);
    XDP_EXPORT ~KernelMemoryAccess() ;

    void setBurstLength(uint16_t length) { burstLength = length; }
  } ;

  class KernelStreamAccess : public KernelEvent
  {
  private:
    uint64_t portName ;
    uint64_t streamName ;
    uint16_t burstLength ;

    KernelStreamAccess() = delete ;
  public:
    XDP_EXPORT KernelStreamAccess(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId);
    XDP_EXPORT ~KernelStreamAccess() ;
  } ;

  class HostRead : public VTFDeviceEvent
  {
  private:
    HostRead() = delete ;
  public:
    XDP_EXPORT HostRead(uint64_t s_id, double ts, uint64_t devId) ;
    XDP_EXPORT ~HostRead() ;
  } ;

  class HostWrite : public VTFDeviceEvent
  {
  private:
    HostWrite() = delete ;
  public:
    XDP_EXPORT HostWrite(uint64_t s_id, double ts, uint64_t devId) ;
    XDP_EXPORT ~HostWrite() ;
  } ;

} // end namespace xdp

#endif
