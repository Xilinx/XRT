/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include<regex>
#include<string>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/api/hw_context_int.h"

#include "xdp/profile/plugin/aie_pc/aie_pc_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#ifdef XDP_CLIENT_BUILD
#include "xdp/profile/plugin/aie_pc/clientDev/aie_pc.h"
#endif

namespace xdp {

  bool AIEPCPlugin::live = false;

  AIEPCPlugin::AIEPCPlugin()
    : XDPPlugin()
  {
    AIEPCPlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_pc);
  }

  AIEPCPlugin::~AIEPCPlugin()
  {
    if (VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch (...) {
      }
      db->unregisterPlugin(this);
    }

    AIEPCPlugin::live = false;
  }

  bool AIEPCPlugin::alive()
  {
    return AIEPCPlugin::live;
  }

  void AIEPCPlugin::updateDevice(void* hwCtxImpl)
  {
#ifdef XDP_CLIENT_BUILD
    if (mHwCtxImpl) {
      // For client device flow, only 1 device and xclbin is supported now.
      return;
    }
    mHwCtxImpl = hwCtxImpl;

    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(mHwCtxImpl);
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::hw_context_int::get_core_device(hwContext);

    // Only one device for Client Device flow
    uint64_t deviceId = db->addDevice("win_device");
    (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceId, coreDevice);
    (db->getStaticInfo()).setDeviceName(deviceId, "win_device");

    DeviceDataEntry.valid = true;
    DeviceDataEntry.implementation = std::make_unique<AIEPCClientDevImpl>(db);
    DeviceDataEntry.implementation->setHwContext(hwContext);
    DeviceDataEntry.implementation->updateDevice(mHwCtxImpl);
#endif
  }

  void AIEPCPlugin::finishflushDevice(void* hwCtxImpl)
  {
#ifdef XDP_CLIENT_BUILD
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "AIE PC Plugin Finish Flush");
    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }

    if (hwCtxImpl != mHwCtxImpl) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
          "New Hw Context Impl passed in AIE PC Plugin.");
      return;
    } 

    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

  void AIEPCPlugin::writeAll(bool /*openNewFiles*/)
  {
#ifdef XDP_CLIENT_BUILD

    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }

    // For client device flow, only 1 device and xclbin is supported now.
    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

}