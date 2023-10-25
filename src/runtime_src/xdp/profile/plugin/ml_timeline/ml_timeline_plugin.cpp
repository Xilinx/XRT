/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_SOURCE

#include <array>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/xrt_profiling.h"
#include "core/common/api/hw_context_int.h"

#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"

#ifdef XDP_MINIMAL_BUILD
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
    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

#ifdef XDP_MINIMAL_BUILD
    return db->addDevice("win_device");
#else
    std::array<char, sysfs_max_path_length> pathBuf = {0};
    xclGetDebugIPlayoutPath(handle, pathBuf.data(), (sysfs_max_path_length-1) ) ;
    std::string sysfspath(pathBuf.data());
    uint64_t deviceID =  db->addDevice(sysfspath); // Get the unique device Id
    return deviceID;
#endif
  }

  void MLTimelinePlugin::updateAIEDevice(void* handle)
  {
    if (!handle)
      return;

    uint64_t deviceID = getDeviceIDFromHandle(handle);

    (db->getStaticInfo()).updateDevice(deviceID, handle);
#ifdef XDP_MINIMAL_BUILD
    (db->getStaticInfo()).setDeviceName(deviceID, "win_device");
#endif

    // Clean out old data every time xclbin gets updated
    if (handleToAIEData.find(handle) != handleToAIEData.end())
      handleToAIEData.erase(handle);

    //Setting up struct 
    auto& AIEDataEntry = handleToAIEData[handle];

    AIEDataEntry.deviceID = deviceID;
    AIEDataEntry.valid = true; // initialize struct

#ifndef XDP_MINIMAL_BUILD
    // Get Device info // Investigate further (isDeviceReady should be always called??)
    if (!(db->getStaticInfo()).isDeviceReady(deviceID)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceID, handle);
      {
        struct xclDeviceInfo2 info;
        if (xclGetDeviceInfo2(handle, &info) == 0)
          (db->getStaticInfo()).setDeviceName(deviceID, std::string(info.mName));
      }
    }
#endif


#ifdef XDP_MINIMAL_BUILD
    AIEDataEntry.implementation = std::make_unique<MLTimelineClientDevImpl>(db);
    AIEDataEntry.implementation->setHwContext(xrt_core::hw_context_int::create_hw_context_from_implementation(handle));
#endif
    AIEDataEntry.implementation->updateAIEDevice(handle);
  }

  void MLTimelinePlugin::finishflushAIEDevice(void* handle)
  {
    if (!handle)
      return;

    auto itr = handleToAIEData.find(handle);
    if (itr == handleToAIEData.end()) {
      return;
    }
    auto& AIEDataEntry = itr->second;
    if (!AIEDataEntry.valid)
      return;

    AIEDataEntry.implementation->finishflushAIEDevice(handle);
    handleToAIEData.erase(handle);
  }

  void MLTimelinePlugin::writeAll(bool /*openNewFiles*/)
  {
    for (const auto& entry : handleToAIEData) {
      auto& AIEDataEntry = entry.second;

      if (!AIEDataEntry.valid) {
        continue;
      }
      AIEDataEntry.implementation->finishflushAIEDevice(entry.first);
      handleToAIEData.erase(entry.first);
    }
    handleToAIEData.clear();
  }

}
