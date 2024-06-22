/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/pl_device_trace_offload.h"

namespace xdp {

  // Forward declarations
  class PLDeviceTraceLogger;

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
  class PLDeviceOffloadPlugin : public XDPPlugin
  {
  private:
    // These are the continuous offload configuration parameters as read
    //  from xrt.ini.
    bool device_trace;
    bool continuous_trace ;
    unsigned int trace_buffer_offload_interval_ms ;
    bool m_enable_circular_buffer = false;

  protected:
    // Each device offload plugin is responsible for offloading
    //  information from all devices.  This holds all the objects
    //  responsible for offloading data from all devices.
    typedef std::tuple<PLDeviceTraceOffload*,
                       PLDeviceTraceLogger*,
                       PLDeviceIntf*> DeviceData ;

    std::map<uint64_t, DeviceData> offloaders;

    void addDevice(const std::string& sysfsPath) ;
    void configureDataflow(uint64_t deviceId, PLDeviceIntf* devInterface) ;
    void configureFa(uint64_t deviceId, PLDeviceIntf* devInterface) ;
    void configureCtx(uint64_t deviceId, PLDeviceIntf* devInterface) ;
    void addOffloader(uint64_t deviceId, PLDeviceIntf* devInterface) ;
    void configureTraceIP(PLDeviceIntf* devInterface) ;
    void startContinuousThreads(uint64_t deviceId) ;

    void readCounters() ;
    virtual void readTrace() = 0 ;
    void checkTraceBufferFullness(PLDeviceTraceOffload* offloader, uint64_t deviceId) ;
    bool flushTraceOffloader(PLDeviceTraceOffload* offloader);

  public:
    PLDeviceOffloadPlugin() ;
    virtual ~PLDeviceOffloadPlugin() = default ;

    virtual void writeAll(bool openNewFiles) ;

    virtual void flushDevice(void* device) = 0 ;
    virtual void updateDevice(void* device) = 0 ;

    virtual void broadcast(VPDatabase::MessageType msg, void* blob) ;

    void clearOffloader(uint64_t deviceId);
    void clearOffloaders();
  } ;

} // end namespace xdp

#endif
