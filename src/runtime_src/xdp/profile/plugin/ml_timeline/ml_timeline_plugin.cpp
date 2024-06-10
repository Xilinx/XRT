/**
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

#define XDP_PLUGIN_SOURCE

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/api/hw_context_int.h"

#include "xdp/profile/plugin/ml_timeline/ml_timeline_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#ifdef XDP_CLIENT_BUILD
#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#endif

namespace xdp {

  bool MLTimelinePlugin::live = false;

  uint32_t ParseMLTimelineBufferSizeConfig()
  {
    uint32_t bufSz = 0;
    std::string szCfgStr = xrt_core::config::get_ml_timeline_buffer_size();
    std::smatch subStr;

    const std::regex validSzRegEx("\\s*([0-9]+)\\s*(K|k|M|m|)\\s*");
    if (std::regex_match(szCfgStr, subStr, validSzRegEx)) {
      try {
        if ("K" == subStr[2] || "k" == subStr[2]) {
          bufSz = (uint32_t)std::stoull(subStr[1]) * uint_constants::one_kb;
        } else if ("M" == subStr[2] || "m" == subStr[2]) {
          bufSz = (uint32_t)std::stoull(subStr[1]) * uint_constants::one_mb;
        }

      } catch (const std::exception &e) {
        std::stringstream msg;
        msg << "Invalid string specified for ML Timeline Buffer Size. "
            << e.what() << std::endl;
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      }

    } else {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                "Invalid string specified for ML Timeline Buffer Size");
    }
  }

  MLTimelinePlugin::MLTimelinePlugin()
    : XDPPlugin()
  {
    MLTimelinePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::ml_timeline);

    mBufSz = ParseMLTimelineBufferSizeConfig();
  }

  MLTimelinePlugin::~MLTimelinePlugin()
  {
    if (VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch (...) {
      }
      db->unregisterPlugin(this);
    }

    MLTimelinePlugin::live = false;
  }

  bool MLTimelinePlugin::alive()
  {
    return MLTimelinePlugin::live;
  }

  void MLTimelinePlugin::updateDevice(void* hwCtxImpl)
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
    (db->getStaticInfo()).updateDeviceClient(deviceId, coreDevice);
    (db->getStaticInfo()).setDeviceName(deviceId, "win_device");

    DeviceDataEntry.valid = true;
    DeviceDataEntry.implementation = std::make_unique<MLTimelineClientDevImpl>(db);
    DeviceDataEntry.implementation->setHwContext(hwContext);
    DeviceDataEntry.implementation->setBufSize(mBufSz);
    DeviceDataEntry.implementation->updateDevice(mHwCtxImpl);
#endif
  }

  void MLTimelinePlugin::finishflushDevice(void* hwCtxImpl)
  {
#ifdef XDP_CLIENT_BUILD
    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }

    if (hwCtxImpl != mHwCtxImpl) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
          "Cannot retrieve ML Timeline data as a new HW Context Implementation is passed.");
      return;
    } 
    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

  void MLTimelinePlugin::writeAll(bool /*openNewFiles*/)
  {
#ifdef XDP_CLIENT_BUILD
    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }
    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

  void MLTimelinePlugin::broadcast(VPDatabase::MessageType msgType, void* /*blob*/)
  {
    switch(msgType)
    {
      case VPDatabase::READ_RECORD_TIMESTAMPS:
      {
        writeAll(false);
        break;
      }
      default:
        break;
    }
  }
}
