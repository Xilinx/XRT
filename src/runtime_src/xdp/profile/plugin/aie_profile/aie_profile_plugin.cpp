/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/plugin/aie_profile/aie_profile_plugin.h"

#include <boost/algorithm/string.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/xrt_profiling.h"
#include "core/include/experimental/xrt-next.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_profile/aie_writer.h"

#ifdef XDP_MINIMAL_BUILD
#include "win/aie_profile.h"
// #include "shim.h"
#elif defined(XRT_X86_BUILD)
#include "x86/aie_profile.h"
#else
#include "core/edge/user/shim.h"
#include "edge/aie_profile.h"
#endif

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  bool AieProfilePlugin::live = false;

  AieProfilePlugin::AieProfilePlugin() : XDPPlugin()
  {
    AieProfilePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_profile);
    db->getStaticInfo().setAieApplication();
  }

  AieProfilePlugin::~AieProfilePlugin()
  {
    // Stop the polling thread
    endPoll();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }

      db->unregisterPlugin(this);
    }
    AieProfilePlugin::live = false;
  }

  bool AieProfilePlugin::alive()
  {
    return AieProfilePlugin::live;
  }

  uint64_t AieProfilePlugin::getDeviceIDFromHandle(void* handle)
  {
    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

#ifdef XDP_MINIMAL_BUILD
    return 0;
#else
    constexpr uint32_t PATH_LENGTH = 512;
    
    char pathBuf[PATH_LENGTH];
    memset(pathBuf, 0, PATH_LENGTH);

    xclGetDebugIPlayoutPath(handle, pathBuf, PATH_LENGTH);
    std::string sysfspath(pathBuf);
    uint64_t deviceID = db->addDevice(sysfspath);  // Get the unique device Id
    return deviceID;
#endif
  }

  void AieProfilePlugin::updateAIEDevice(void* handle)
  {
    // Don't update if no profiling is requested
    if (!xrt_core::config::get_aie_profile())
      return;

    if (!handle)
      return;

    auto deviceID = getDeviceIDFromHandle(handle);

    // Update the static database with information from xclbin
    (db->getStaticInfo()).updateDevice(deviceID, handle);
    {
#ifdef XDP_MINIMAL_BUILD
      (db->getStaticInfo()).setDeviceName(deviceID, "win_device");
#else
      struct xclDeviceInfo2 info;
      if (xclGetDeviceInfo2(handle, &info) == 0) {
        (db->getStaticInfo()).setDeviceName(deviceID, std::string(info.mName));
      }
#endif
    }

    // delete old data
    if (handleToAIEData.find(handle) != handleToAIEData.end())
      handleToAIEData.erase(handle);
    auto& AIEData = handleToAIEData[handle];

    AIEData.deviceID = deviceID;
    AIEData.metadata = std::make_shared<AieProfileMetadata>(deviceID, handle);

#ifdef XDP_MINIMAL_BUILD
    AIEData.implementation = std::make_unique<AieProfile_WinImpl>(db, AIEData.metadata);
#elif defined(XRT_X86_BUILD)
    AIEData.implementation = std::make_unique<AieProfile_x86Impl>(db, AIEData.metadata);
#else
    AIEData.implementation = std::make_unique<AieProfile_EdgeImpl>(db, AIEData.metadata);
#endif
    auto& implementation = AIEData.implementation;

    // Ensure we only read/configure once per xclbin
    if (!(db->getStaticInfo()).isAIECounterRead(deviceID)) {
      // Sets up and calls the PS kernel on x86 implementation
      // Sets up and the hardware on the edge implementation
      implementation->updateDevice();

      (db->getStaticInfo()).setIsAIECounterRead(deviceID, true);
    }

    // Open the writer for this device
    auto time = std::time(nullptr);

#ifdef _WIN32
    std::tm tm{};
    localtime_s(&tm, &time);
    std::string deviceName = "win_device";
#else
    auto tm = *std::localtime(&time);
    struct xclDeviceInfo2 info;
    xclGetDeviceInfo2(handle, &info);
    std::string deviceName = std::string(info.mName);
#endif

    std::ostringstream timeOss;
    timeOss << std::put_time(&tm, "_%Y_%m_%d_%H%M%S");
    std::string timestamp = timeOss.str();

    std::string outputFile = "aie_profile_" + deviceName + timestamp + ".csv";

    VPWriter* writer = new AIEProfilingWriter(outputFile.c_str(), deviceName.c_str(), mIndex);
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_PROFILE");

    // Start the AIE profiling thread
    AIEData.threadCtrlBool = true;
    auto device_thread = std::thread(&AieProfilePlugin::pollAIECounters, this, mIndex, handle);
    // auto device_thread = std::thread(&AieProfileImpl::pollAIECounters,
    // implementation.get(), mIndex, handle);
    AIEData.thread = std::move(device_thread);

    ++mIndex;
  }

  void AieProfilePlugin::pollAIECounters(uint32_t index, void* handle)
  {
    auto it = handleToAIEData.find(handle);
    if (it == handleToAIEData.end())
      return;

    auto& should_continue = it->second.threadCtrlBool;

    while (should_continue) {
      handleToAIEData[handle].implementation->poll(index, handle);
      std::this_thread::sleep_for(std::chrono::microseconds(handleToAIEData[handle].metadata->getPollingIntervalVal()));
    }
    // Final Polling Operation
    handleToAIEData[handle].implementation->poll(index, handle);
  }

  void AieProfilePlugin::endPollforDevice(void* handle)
  {
    // Ask thread to stop
    auto& AIEData = handleToAIEData[handle];
    AIEData.threadCtrlBool = false;

    AIEData.thread.join();

    AIEData.implementation->freeResources();
    handleToAIEData.erase(handle);
  }

  void AieProfilePlugin::endPoll()
  {
    // Ask all threads to end
    for (auto& p : handleToAIEData) p.second.threadCtrlBool = false;

    for (auto& p : handleToAIEData) p.second.thread.join();

    handleToAIEData.clear();
  }

}  // end namespace xdp
