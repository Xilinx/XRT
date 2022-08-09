/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <boost/algorithm/string.hpp>

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/edge/user/shim.h"
#include "core/include/experimental/xrt-next.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_profile/aie_writer.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  bool AIEProfilePluginUnified::live = false;

  AIEProfilePluginUnified::AIEProfilePluginUnified() 
      : XDPPlugin()
  {
    AIEProfilePluginUnified::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_profile);
    db->getStaticInfo().setAieApplication();
    // getPollingInterval(); //moved later

  }

  AIEProfilePluginUnified::~AIEProfilePluginUnified()
  {
    // Stop the polling thread
    endPoll();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }

      db->unregisterPlugin(this);
    }
    AIEProfilePluginUnified::live = false;
  }

  bool AIEProfilePluginUnified::alive()
  {
    return AIEProfilePluginUnified::live;
  }


  bool AIEProfilePluginUnified::checkAieDevice(uint64_t deviceId, void* handle)
  {
    aieDevInst = static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    aieDevice  = static_cast<xaiefal::XAieDev*>(db->getStaticInfo().getAieDevice(allocateAieDevice, deallocateAieDevice, handle)) ;
    if (!aieDevInst || !aieDevice) {
      xrt_core::message::send(severity_level::warning, "XRT", 
          "Unable to get AIE device. There will be no AIE profiling.");
      return false;
    }
    return true;
  }

  void AIEProfilePluginUnified::updateAIEDevice(void* handle)
  {

    // Don't update if no profiling is requested
    if (!xrt_core::config::get_aie_profile())
      return;

    if (!handle)
      return;

    // delete old data
    if (handleToAIEData.find(handle) != handleToAIEData.end())
        handleToAIEData.erase(handle);
    auto& AIEData = handleToAIEData[handle];

    auto deviceID = getDeviceIDFromHandle(handle);
    AIEData.deviceID = deviceID;
    AIEData.metadata = std::make_shared<AieTraceMetadata>(deviceID, handle);
    auto& metadata = AIEData.metadata;
    AIEData.supported = true; // initialize struct
    AIEData.devIntf = nullptr;

    //Get the polling interval after the metadata has been defined.
    metadata->getPollingInterval(); //moved later


    AIEData.implementation = std::make_unique<AieTrace_x86Impl>(db, metadata);
    auto& implementation = AIEData.implementation;

    if (!(db->getStaticInfo()).isDeviceReady(deviceId)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceId, handle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(handle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
        }
      }
    }

    // Ensure we only read/configure once per xclbin
    if (!(db->getStaticInfo()).isAIECounterRead(deviceId)) {
      // Update the AIE specific portion of the device
      // When new xclbin is loaded, the xclbin specific datastructure is already recreated

      // 1. Runtime-defined counters
      // NOTE: these take precedence

      if(!checkAieDevice(deviceId, handle))
        return;

      // bool runtimeCounters = setMetrics(deviceId,handle);
      // // bool runtimeCounters = setMetricsSettings(deviceId, handle);
      // if(!runtimeCounters) 
      //   runtimeCounters = setMetrics(deviceId, handle);

      //Sets up and calls the PS kernel on x86 implementation
      //Sets up and the hardware on the edge implementation
      implementation->updateDevice();

      // 2. Compiler-defined counters
      if (!runtimeCounters) {
        std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
        auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

        if (counters.empty()) {
          xrt_core::message::send(severity_level::warning, "XRT", 
            "AIE Profile Counters were not found for this design. Please specify tile_based_[aie|aie_memory|interface_tile]_metrics under \"AIE_profile_settings\" section in your xrt.ini.");
          (db->getStaticInfo()).setIsAIECounterRead(deviceId,true);
          return;
        }
        else {
          // XAie_DevInst* aieDevInst =
          //   static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));

          // for (auto& counter : counters) {
          //   tile_type tile;
          //   auto payload = getCounterPayload(aieDevInst, tile, counter.column, counter.row, 
          //                                    counter.startEvent);

          //   (db->getStaticInfo()).addAIECounter(deviceId, counter.id, counter.column,
          //       counter.row + 1, counter.counterNumber, counter.startEvent, counter.endEvent,
          //       counter.resetEvent, payload, counter.clockFreqMhz, counter.module, counter.name);
          //}
        }
      }

      (db->getStaticInfo()).setIsAIECounterRead(deviceId, true);
    }

    // Open the writer for this device
    struct xclDeviceInfo2 info;
    xclGetDeviceInfo2(handle, &info);
    std::string deviceName = std::string(info.mName);
    // Create and register writer and file
    std::string core_str = (mCoreMetricSet.empty())   ? "" : "_" + mCoreMetricSet;
    std::string mem_str  = (mMemoryMetricSet.empty()) ? "" : "_" + mMemoryMetricSet;
    std::string shim_str = (mShimMetricSet.empty())   ? "" : "_" + mShimMetricSet;    
    std::string chan_str = (mChannelId < 0)           ? "" : "_chan" + std::to_string(mChannelId);

    std::string outputFile = "aie_profile_" + deviceName + core_str + mem_str 
        + shim_str + chan_str + ".csv";

    VPWriter* writer = new AIEProfilingWriter(outputFile.c_str(),
                                              deviceName.c_str(), mIndex);
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_PROFILE");

    // Start the AIE profiling thread
    mThreadCtrlMap[handle] = true;
    // auto device_thread = std::thread(&AIEProfilePluginUnified::pollAIECounters, this, mIndex, handle);
    // mThreadMap[handle] = std::move(device_thread);

    ++mIndex;
  }

  uint64_t AieProfilePluginUnified::getDeviceIDFromHandle(void* handle)
  {
    constexpr uint32_t PATH_LENGTH = 512;

    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

    char pathBuf[PATH_LENGTH];
    memset(pathBuf, 0, PATH_LENGTH);
    xclGetDebugIPlayoutPath(handle, pathBuf, PATH_LENGTH);
    std::string sysfspath(pathBuf);
    uint64_t deviceID =  db->addDevice(sysfspath); // Get the unique device Id
    return deviceID;
  }


  void AIEProfilePluginUnified::endPollforDevice(void* handle)
  {
    // Ask thread to stop
    mThreadCtrlMap[handle] = false;

    auto it = mThreadMap.find(handle);
    if (it != mThreadMap.end()) {
      it->second.join();
      mThreadMap.erase(it);
      mThreadCtrlMap.erase(handle);
    }
  }

  void AIEProfilePluginUnified::endPoll()
  {
    // Ask all threads to end
    for (auto& p : mThreadCtrlMap)
      p.second = false;

    for (auto& t : mThreadMap)
      t.second.join();

    mThreadCtrlMap.clear();
    mThreadMap.clear();
  }
 
} // end namespace xdp
