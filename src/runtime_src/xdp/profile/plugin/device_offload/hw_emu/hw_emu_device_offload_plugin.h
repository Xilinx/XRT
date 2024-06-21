/**
 * Copyright (C) 2022 Xilinx, Inc
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

#ifndef HW_EMU_DEVICE_OFFLOAD_PLUGIN_DOT_H
#define HW_EMU_DEVICE_OFFLOAD_PLUGIN_DOT_H

#include <set>

#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"

namespace xdp {

  class HWEmuDeviceOffloadPlugin : public PLDeviceOffloadPlugin
  {
  private:
    // In hardware emulation, there should only ever be one device,
    //  so all of the handles passed in by our callbacks (and accessible
    //  to the users) will all be pointing to the same device.  Therefore
    //  we do not need to store a handle locally.

    // We do, however, need to keep track of the device IDs we've seen
    //  to keep track of when we create new writers.
    std::set<uint64_t> devicesSeen ;

    virtual void readTrace() override ;
  public:
    HWEmuDeviceOffloadPlugin() ;
    ~HWEmuDeviceOffloadPlugin() override ;

    virtual void flushDevice(void* device) override ;
    virtual void updateDevice(void* device) override ;
  } ;

} // end namespace xdp

#endif
