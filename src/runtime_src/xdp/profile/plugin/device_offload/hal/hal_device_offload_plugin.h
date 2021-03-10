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

#ifndef HAL_DEVICE_OFFLOAD_PLUGIN_DOT_H
#define HAL_DEVICE_OFFLOAD_PLUGIN_DOT_H

#include <vector>
#include <map>

#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"

namespace xdp {

  // This plugin should be completely agnostic of what the host code profiling
  //  plugin is.  So, this should work with HAL profiling, OpenCL profiling, 
  //  LOP profiling, user events, and any other plugin.
  // 
  // This plugin is only responsible for trace.  It has no responsibility
  //  to read counters unless it is in the service of trace.
  class HALDeviceOffloadPlugin : public DeviceOffloadPlugin
  {
  private:
    // In order to guarantee that we will be able to flush information
    //  from a device at any time (even if the user has closed their
    //  handles) we need to open all the devices and keep our own handles
    //  to them.
    std::vector<void*> deviceHandles ;
    std::map<uint64_t, void*> deviceIdToHandle ;

  public:
    XDP_EXPORT HALDeviceOffloadPlugin() ;
    XDP_EXPORT ~HALDeviceOffloadPlugin() ;

    // Virtual functions from XDPPlugin
    XDP_EXPORT virtual void writeAll(bool openNewFiles) ;

    // Virtual functions from DeviceOffloadPlugin
    XDP_EXPORT virtual void flushDevice(void* device) ;
    XDP_EXPORT virtual void updateDevice(void* device) ;
  } ;

} // end namespace xdp

#endif
