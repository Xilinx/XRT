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

#include <array>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/api/hw_context_int.h"

#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"

#ifdef XDP_CLIENT_BUILD
  #include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#endif

namespace xdp {

  bool MLTimelinePlugin::live = false;

  MLTimelinePlugin::MLTimelinePlugin()
    : XDPPlugin()
  {
    MLTimelinePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::ml_timeline);
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

  uint64_t MLTimelinePlugin::getDeviceIDFromHandle(void* handle)
  { 
    auto itr = handleToDeviceData.find(handle);
    if (itr != handleToDeviceData.end())
      return itr->second.deviceID;

#ifdef XDP_CLIENT_BUILD
    return db->addDevice("win_device");
#else
    return db->addDevice(""); // Not supported for non-client device 
#endif
  }

  void MLTimelinePlugin::updateDevice(void* handle)
  {
    if (!handle)
      return;

    uint64_t deviceID = getDeviceIDFromHandle(handle);

    (db->getStaticInfo()).updateDevice(deviceID, handle);
#ifdef XDP_CLIENT_BUILD
    (db->getStaticInfo()).setDeviceName(deviceID, "win_device");
#endif

    // Clean out old data every time xclbin gets updated
    if (handleToDeviceData.find(handle) != handleToDeviceData.end())
      handleToDeviceData.erase(handle);

    //Setting up struct 
    auto& DeviceDataEntry = handleToDeviceData[handle];

    DeviceDataEntry.deviceID = deviceID;
    DeviceDataEntry.valid = true; // initialize struct

#ifndef XDP_CLIENT_BUILD
    // Get Device info // Investigate further (isDeviceReady should be always called??)
    if (!(db->getStaticInfo()).isDeviceReady(deviceID)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceID, handle);
      {
        std::string deviceName = util::getDeviceName(handle);
        if (deviceName != "")
          (db->getStaticInfo()).setDeviceName(deviceID, deviceName);
      }
    }
#endif


#ifdef XDP_CLIENT_BUILD
    DeviceDataEntry.implementation = std::make_unique<MLTimelineClientDevImpl>(db);
    DeviceDataEntry.implementation->setHwContext(xrt_core::hw_context_int::create_hw_context_from_implementation(handle));
#endif
  }

  void MLTimelinePlugin::finishflushDevice(void* handle)
  {
    if (!handle)
      return;

    auto itr = handleToDeviceData.find(handle);
    if (itr == handleToDeviceData.end()) {
      return;
    }
    auto& DeviceDataEntry = itr->second;
    if (!DeviceDataEntry.valid)
      return;

    DeviceDataEntry.implementation->finishflushDevice(handle);
    handleToDeviceData.erase(handle);
  }

  void MLTimelinePlugin::writeAll(bool /*openNewFiles*/)
  {
    for (const auto& entry : handleToDeviceData) {
      auto& DeviceDataEntry = entry.second;

      if (!DeviceDataEntry.valid) {
        continue;
      }
      DeviceDataEntry.implementation->finishflushDevice(entry.first);
      handleToDeviceData.erase(entry.first);
    }
    handleToDeviceData.clear();
  }

}
