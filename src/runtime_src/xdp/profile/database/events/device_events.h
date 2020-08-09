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
    uint64_t deviceTimestamp;	// actual device timestamp from HW

    /* Type dependent Profile Monitor Index
     * Event type indicates whether the index is for AM, AIM or ASM
     */ 
    uint32_t monitorId;

    VTFDeviceEvent() = delete ;

  protected:
    virtual void dumpTimestamp(std::ofstream& fout) ;

  public:
    XDP_EXPORT VTFDeviceEvent(uint64_t s_id, double ts, VTFEventType ty,
                              uint64_t devId, uint32_t monId);
    XDP_EXPORT ~VTFDeviceEvent() ;

    XDP_EXPORT virtual void dump(std::ofstream& fout, uint32_t bucket);

    virtual bool     isDeviceEvent() { return true ; }
    virtual uint64_t getDevice()     { return deviceId ; }

    virtual void     setDeviceTimestamp(uint64_t deviceTime) { deviceTimestamp = deviceTime; }
    virtual uint64_t getDeviceTimestamp() { return deviceTimestamp; }

    virtual uint32_t getMonitorId() { return monitorId; }
    virtual int32_t  getCUId()      { return -1; }
  } ;

  class KernelEvent : public VTFDeviceEvent
  {
  protected:
    int32_t cuId;

    KernelEvent() = delete ;
  public:
    XDP_EXPORT KernelEvent(uint64_t s_id, double ts, VTFEventType ty,
                           uint64_t devId, uint32_t monId, int32_t cuIdx);
    XDP_EXPORT ~KernelEvent();

    virtual int32_t getCUId() { return cuId; }
  };

  class KernelStall : public KernelEvent
  {
  private:
    uint16_t burstLength ;

    KernelStall() = delete ;
  public:
    XDP_EXPORT KernelStall(uint64_t s_id, double ts, VTFEventType ty,
                           uint64_t devId, uint32_t monId, int32_t cuIdx);
    XDP_EXPORT ~KernelStall();
  } ;

  class DeviceMemoryAccess : public VTFDeviceEvent
  {
  private:
    int32_t  cuId;
    uint64_t portName ;
    uint64_t memoryName ;
    uint64_t argumentNames ;
    uint16_t burstLength ;
    uint16_t numBytes ;

    DeviceMemoryAccess() = delete ;
  public:
    XDP_EXPORT DeviceMemoryAccess(uint64_t s_id, double ts, VTFEventType ty,
                                  uint64_t devId, uint32_t monId, int32_t cuIdx = -1);
    XDP_EXPORT ~DeviceMemoryAccess();

    virtual int32_t getCUId() { return cuId; }

    void setBurstLength(uint16_t length) { burstLength = length; }
  } ;

  class DeviceStreamAccess : public VTFDeviceEvent
  {
  private:
    int32_t  cuId;
    uint64_t portName ;
    uint64_t streamName ;
    uint16_t burstLength ;

    DeviceStreamAccess() = delete ;
  public:
    XDP_EXPORT DeviceStreamAccess(uint64_t s_id, double ts, VTFEventType ty,
                                  uint64_t devId, uint32_t monId, int32_t cuIdx = -1);
    XDP_EXPORT ~DeviceStreamAccess();

    virtual int32_t getCUId() { return cuId; }
  } ;

  class HostRead : public VTFDeviceEvent
  {
  private:
    HostRead() = delete ;
  public:
    XDP_EXPORT HostRead(uint64_t s_id, double ts, uint64_t devId, uint32_t monId) ;
    XDP_EXPORT ~HostRead() ;
  } ;

  class HostWrite : public VTFDeviceEvent
  {
  private:
    HostWrite() = delete ;
  public:
    XDP_EXPORT HostWrite(uint64_t s_id, double ts, uint64_t devId, uint32_t monId) ;
    XDP_EXPORT ~HostWrite() ;
  } ;

} // end namespace xdp

#endif
