/**
 * Copyright (C) 2024-2025 Advanced Micro Devices, Inc. - All rights reserved
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
#include<cassert>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/api/hw_context_int.h"

#include "xdp/profile/plugin/aie_halt/aie_halt_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#ifdef XDP_CLIENT_BUILD
#include "xdp/profile/plugin/aie_halt/clientDev/aie_halt.h"
#elif defined (XDP_VE2_BUILD)
#include "xdp/profile/plugin/aie_halt/ve2/aie_halt.h"
#include "xdp/profile/device/xdp_base_device.h"
#endif

namespace xdp {

  bool AIEHaltPlugin::live = false;

  AIEHaltPlugin::AIEHaltPlugin()
    : XDPPlugin()
  {
    AIEHaltPlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_halt);
  }

  AIEHaltPlugin::~AIEHaltPlugin()
  {
    if (VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch (...) {
      }
      db->unregisterPlugin(this);
    }

    AIEHaltPlugin::live = false;
  }

  bool AIEHaltPlugin::alive()
  {
    return AIEHaltPlugin::live;
  }

  void AIEHaltPlugin::updateDevice(void* hwCtxImpl)
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
    (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceId, coreDevice, false);
    (db->getStaticInfo()).setDeviceName(deviceId, "win_device");

    DeviceDataEntry.valid = true;
    DeviceDataEntry.implementation = std::make_unique<AIEHaltClientDevImpl>(db);
    DeviceDataEntry.implementation->setHwContext(hwContext);
    DeviceDataEntry.implementation->updateDevice(mHwCtxImpl);

#elif defined (XDP_VE2_BUILD)
    if (mHwCtxImpl) {
      // For VE2 device flow, only 1 device and xclbin is supported now.
      return;
    }
    mHwCtxImpl = hwCtxImpl;

    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(mHwCtxImpl);
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::hw_context_int::get_core_device(hwContext);
    
    // Only one device for VE2 Device flow
    uint64_t deviceId = db->addDevice("ve2_device");
    (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceId, coreDevice, false);
    (db->getStaticInfo()).setDeviceName(deviceId, "ve2_device");

    DeviceDataEntry.valid = true;
    DeviceDataEntry.implementation = std::make_unique<AIEHaltVE2Impl>(db);
    DeviceDataEntry.implementation->setHwContext(hwContext);
    DeviceDataEntry.implementation->updateDevice(mHwCtxImpl);
#endif
  }

  void AIEHaltPlugin::finishflushDevice(void* hwCtxImpl)
  {
#if defined(XDP_CLIENT_BUILD) || defined(XDP_VE2_BUILD)
    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }

    if (hwCtxImpl != mHwCtxImpl) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
          "New Hw Context Impl passed in AIE Halt Plugin.");
      return;
    }

    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

  void AIEHaltPlugin::writeAll(bool /*openNewFiles*/)
  {
#if defined(XDP_CLIENT_BUILD) || defined(XDP_VE2_BUILD)

    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }

    // For client device flow, only 1 device and xclbin is supported now.
    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

}
