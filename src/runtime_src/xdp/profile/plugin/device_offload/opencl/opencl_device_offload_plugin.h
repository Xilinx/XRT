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

#ifndef OPENCL_DEVICE_OFFLOAD_PLUGIN_DOT_H
#define OPENCL_DEVICE_OFFLOAD_PLUGIN_DOT_H

#include <memory>
#include <set>

// Includes from xilinxopencl
#include "xocl/core/device.h"

// Includes from xdp
#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"

namespace xdp {

  // This is the device offload plugin instantiated from the OpenCL layer.
  class OpenCLDeviceOffloadPlugin : public DeviceOffloadPlugin
  {
  private:
    // I have to keep a shared pointer to the platform to make sure
    //  no xrt_xocl::device objects are deleted before we read them at 
    //  the end of execution.
    std::shared_ptr<xocl::platform> platform ;

    // The devices that need to be flushed at program end (if the
    //  host application did not correctly clean them up)
    std::set<uint64_t> deviceIdsToBeFlushed ;

    void updateOpenCLInfo(uint64_t deviceId) ;
    void updateSWEmulationGuidance() ;

    XDP_EXPORT virtual void readTrace() ;

  public:
    XDP_EXPORT OpenCLDeviceOffloadPlugin() ;
    XDP_EXPORT ~OpenCLDeviceOffloadPlugin() ;

    // Virtual functions from XDPPlugin
    XDP_EXPORT virtual void writeAll(bool openNewFiles) ;

    // Virtual functions from DeviceOffloadPlugin
    XDP_EXPORT virtual void flushDevice(void* device) ;
    XDP_EXPORT virtual void updateDevice(void* device) ;
  } ;

} // end namespace xdp

#endif
