/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include <map>
#include <memory>
#include <vector>

#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"

namespace xrt {
  class device;
}

namespace xdp {

  // This plugin should be completely agnostic of what the host code profiling
  //  plugin is.  So, this should work with HAL profiling, OpenCL profiling, 
  //  LOP profiling, user events, and any other plugin.
  class HALDeviceOffloadPlugin : public PLDeviceOffloadPlugin
  {
  private:
    // In order to guarantee that we will be able to flush information
    //  from a device at any time (even if the user has closed their
    //  handles) we need to open all the devices and keep our own handles
    //  to them.
    std::vector<std::unique_ptr<xrt::device>> xrtDevices ;
    std::map<uint64_t, void*> deviceIdToHandle ;

    virtual void readTrace() ;

    // When trying to determine the path to the debug_ip_layout file,
    //  we need to call the C-interface function xclGetDebugIPlayoutPath
    //  function, which takes a preallocated char* and size.
    constexpr static int maxPathLength = 512 ;

  public:
    HALDeviceOffloadPlugin() ;
    ~HALDeviceOffloadPlugin() ;

    // Virtual functions from DeviceOffloadPlugin
    virtual void flushDevice(void* device) ;
    virtual void updateDevice(void* device) ;
  } ;

} // end namespace xdp

#endif
