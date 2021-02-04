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

#ifndef DEVICE_OFFLOAD_PLUGIN_DOT_H
#define DEVICE_OFFLOAD_PLUGIN_DOT_H

#include <map>
#include <string>
#include <tuple>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/device_trace_offload.h"

namespace xdp {

  // Forward declarations
  class TraceLoggerCreatingDeviceEvents ;

  // This plugin should be completely agnostic of what the host code profiling
  //  plugin is.  So, this should work with HAL profiling, OpenCL profiling, 
  //  LOP profiling, user events, and any other plugin.
  // 
  // This plugin is only responsible for trace.  It has no responsibility
  //  to read or manipulate counters unless it is in the service of trace.
  // 
  // This is the base of all plugins that perform device offload.  It 
  //  handles common functionality for programs that come from HAL or
  //  OpenCL.
  class DeviceOffloadPlugin : public XDPPlugin
  {
  private:
    // These are the continuous offload configuration parameters as read
    //  from xrt.ini.
    bool continuous_trace ;
    unsigned int continuous_trace_interval_ms ;

  protected:
    // This is used to determine if each plugin instance
    //  has access to the device
    bool active ;

    // Each device offload plugin is responsible for offloading
    //  information from all devices.  This holds all the objects
    //  responsible for offloading data from all devices.
    typedef std::tuple<DeviceTraceOffload*, 
                       TraceLoggerCreatingDeviceEvents*,
                       DeviceIntf*> DeviceData ;

    std::map<uint64_t, DeviceData> offloaders;

    XDP_EXPORT void addDevice(const std::string& sysfsPath) ;
    XDP_EXPORT void configureDataflow(uint64_t deviceId, DeviceIntf* devInterface) ;
    XDP_EXPORT void configureFa(uint64_t deviceId, DeviceIntf* devInterface) ;
    XDP_EXPORT void configureCtx(uint64_t deviceId, DeviceIntf* devInterface) ;
    XDP_EXPORT void addOffloader(uint64_t deviceId, DeviceIntf* devInterface) ;
    XDP_EXPORT void configureTraceIP(DeviceIntf* devInterface) ;

    XDP_EXPORT void readCounters() ;

  public:
    XDP_EXPORT DeviceOffloadPlugin() ;
    XDP_EXPORT ~DeviceOffloadPlugin() ;

    virtual void writeAll(bool openNewFiles) ;

    virtual void flushDevice(void* device) = 0 ;
    virtual void updateDevice(void* device) = 0 ;

    virtual void broadcast(VPDatabase::MessageType msg, void* blob) ;

    void clearOffloader(uint64_t deviceId);
    void clearOffloaders();
  } ;

} // end namespace xdp

#endif
