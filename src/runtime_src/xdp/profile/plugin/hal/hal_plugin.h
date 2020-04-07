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

#ifndef HAL_PLUGIN_DOT_H
#define HAL_PLUGIN_DOT_H

#include <map>
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  // Forward declarations
  class DeviceIntf; 
  class DeviceTraceLogger; 
  class DeviceTraceOffload; 

  class HALPlugin : public XDPPlugin
  {
  private:
    std::map<uint64_t, DeviceIntf*> devices;
    std::map<uint64_t, DeviceTraceLogger*>  deviceTraceLoggers;
    std::map<uint64_t, DeviceTraceOffload*> deviceTraceOffloaders;

    void flushDevices() ;
    void continuousOffload() ;
    void resetDevice(uint64_t deviceId);

  public:
    XDP_EXPORT
    HALPlugin() ;

    XDP_EXPORT
    ~HALPlugin() ;

    XDP_EXPORT
    virtual void updateDevice(void* /*device*/, const void* /*binary*/);

    XDP_EXPORT
    virtual void writeAll(bool openNewFiles) ;
    XDP_EXPORT
    virtual void readDeviceInfo(void* device) ;
    XDP_EXPORT
    void flushDeviceInfo(void* device) ;

    XDP_EXPORT
    uint64_t getDeviceId(void* handle);
  } ;

}

#endif
