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

#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/flexml_timeline/flexml_timeline_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"

#ifdef XDP_MINIMAL_BUILD
  #include "xdp/profile/plugin/flexml_timeline/win/flexml_timeline.h"
#endif

namespace xdp {

  bool FlexMLTimelinePlugin::live = false;

  FlexMLTimelinePlugin::FlexMLTimelinePlugin()
    : XDPPlugin()
  {
    FlexMLTimelinePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::flexml_timeline);
  }

  FlexMLTimelinePlugin::~FlexMLTimelinePlugin()
  {
    if (VPDatabase::alive()) {
      try {
        // write
      }
      catch (...) {
      }
    }

    FlexMLTimelinePlugin::live = false;
  }

  bool FlexMLTimelinePlugin::alive()
  {
    return FlexMLTimelinePlugin::live;
  }

  uint64_t FlexMLTimelinePlugin::getDeviceIDFromHandle(void* handle)
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

  void FlexMLTimelinePlugin::updateAIEDevice(void* handle)
  {
    if (!handle)
      return;

    uint64_t deviceID = getDeviceIDFromHandle(handle);

    (db->getStaticInfo()).updateDevice(deviceID, handle);
    (db->getStaticInfo()).setDeviceName(deviceID, "win_device");

    // Clean out old data every time xclbin gets updated
    if (handleToAIEData.find(handle) != handleToAIEData.end())
      handleToAIEData.erase(handle);

    //Setting up struct 
    auto& AIEDataEntry = handleToAIEData[handle];

    AIEDataEntry.deviceID = deviceID;
    AIEDataEntry.valid = true; // initialize struct

#if 0
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
    AIEDataEntry.aieMetadata = std::make_shared<AieConfigMetadata>();
    AIEDataEntry.implementation = std::make_unique<FlexMLTimelineWinImpl>(db, AIEDataEntry.aieMetadata);
#endif
    AIEDataEntry.implementation->updateAIEDevice(handle);
  }

  void FlexMLTimelinePlugin::flushAIEDevice(void* handle)
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

    AIEDataEntry.implementation->flushAIEDevice(handle);
  }

  void FlexMLTimelinePlugin::finishflushAIEDevice(void* handle)
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

}
