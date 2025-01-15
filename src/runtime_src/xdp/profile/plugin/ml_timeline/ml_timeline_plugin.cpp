/**
 * Copyright (C) 2023-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/plugin/ml_timeline/ml_timeline_plugin.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_impl.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#ifdef XDP_CLIENT_BUILD
#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#elif defined (XDP_VE2_BUILD)
#include "xdp/profile/plugin/ml_timeline/ve2/ml_timeline.h"
#endif

namespace xdp {

  bool MLTimelinePlugin::live = false;

  uint32_t ParseMLTimelineBufferSizeConfig()
  {
    uint32_t bufSz = 0;
    std::string szCfgStr = xrt_core::config::get_ml_timeline_buffer_size();
    std::smatch subStr;

    std::stringstream msg;

    const std::regex validSzRegEx("\\s*([0-9]+)\\s*(K|k|M|m|)\\s*");
    if (std::regex_match(szCfgStr, subStr, validSzRegEx)) {
      uint32_t szKB = 0;
      try {
        if ("K" == subStr[2] || "k" == subStr[2]) {
          szKB = (uint32_t)std::stoull(subStr[1]);
        } else if ("M" == subStr[2] || "m" == subStr[2]) {
          // Convert to KB now
          szKB = (uint32_t)std::stoull(subStr[1]) * uint_constants::one_kb;
        }
        if (0 != (szKB % RECORD_TIMER_ENTRY_SZ_IN_BYTES)) {
          /* Adjust given ML Timeline Buffer Size for alignment to avoid incorrect reads when Host Buffer
           * gets overwritten with excess record timer data.
           */
          uint32_t q = szKB / RECORD_TIMER_ENTRY_SZ_IN_BYTES;
          // Round up to next 12KB aligned size 
          szKB = (q + 1) * RECORD_TIMER_ENTRY_SZ_IN_BYTES;
          bufSz = szKB * uint_constants::one_kb;
          std::stringstream amsg;
          amsg << "Adjusting given ML Timeline Buffer Size " << szCfgStr
               << " to 0x" << std::hex << bufSz << std::dec << " (in bytes) for alignment." << std::endl;
          xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", amsg.str());
        } else if (szKB) {
          bufSz = szKB * uint_constants::one_kb;
        }
      } catch (const std::exception &e) {
        msg << "Hit exception " << e.what() << ". ";
      }
    }
    if (0 == bufSz) {
      bufSz = 0x30000;
      msg << "Invalid string " << szCfgStr << " specified for ML Timeline Buffer Size. Using default size of 192KB." << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
    }
    
    return bufSz;
  }

  MLTimelinePlugin::MLTimelinePlugin()
    : XDPPlugin(),
      mBufSz(0)
  {
    MLTimelinePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::ml_timeline);
  }

  MLTimelinePlugin::~MLTimelinePlugin()
  {
    if (VPDatabase::alive()) {
      try {
        MLTimelinePlugin::live = false;
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
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
          "In ML Timeline Plugin : updateDevice.");

#ifdef XDP_CLIENT_BUILD

    if (mMultiImpl.find(hwCtxImpl) != mMultiImpl.end()) {
      // Same Hardware Context Implementation uses the same impl and buffer
      return;
    }

    if (0 == mBufSz)
      mBufSz = ParseMLTimelineBufferSizeConfig();

    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(hwCtxImpl);
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::hw_context_int::get_core_device(hwContext);

    uint64_t implId = mMultiImpl.size();

    std::string winDeviceName = "win_device" + std::to_string(implId);
    uint64_t deviceId = db->addDevice(winDeviceName);
    (db->getStaticInfo()).updateDeviceClient(deviceId, coreDevice, false);
    (db->getStaticInfo()).setDeviceName(deviceId, winDeviceName);

    mMultiImpl[hwCtxImpl] = std::make_pair(implId, std::make_unique<MLTimelineClientDevImpl>(db, mBufSz));
    auto mlImpl = mMultiImpl[hwCtxImpl].second.get();
    mlImpl->updateDevice(hwCtxImpl);

#elif defined (XDP_VE2_BUILD)

    if (mMultiImpl.find(hwCtxImpl) != mMultiImpl.end()) {
      // Same Hardware Context Implementation uses the same impl and buffer
      return;
    }

    if (0 == mBufSz)
      mBufSz = ParseMLTimelineBufferSizeConfig();

    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(hwCtxImpl);
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::hw_context_int::get_core_device(hwContext);

    uint64_t implId = mMultiImpl.size();

    std::string deviceName = "ve2_device" + std::to_string(implId);
    uint64_t deviceId = db->addDevice(deviceName);
    (db->getStaticInfo()).updateDeviceClient(deviceId, coreDevice, false);
    (db->getStaticInfo()).setDeviceName(deviceId, deviceName);

    mMultiImpl[hwCtxImpl] = std::make_pair(implId, std::make_unique<MLTimelineVE2Impl>(db, mBufSz));
    auto mlImpl = mMultiImpl[hwCtxImpl].second.get();
    mlImpl->updateDevice(hwCtxImpl);
    
  #endif

  }

  void MLTimelinePlugin::finishflushDevice(void* hwCtxImpl)
  {
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
          "In ML Timeline Plugin : finish flush Device.");

#if defined(XDP_CLIENT_BUILD) || defined(XDP_VE2_BUILD)
    if (mMultiImpl.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
          "In ML Timeline Plugin : No active HW Context found. So no data flush done.");
      return;   
    }
    std::map<void* /*hwCtxImpl*/,
             std::pair<uint64_t /* deviceId */, std::unique_ptr<MLTimelineImpl>>>::iterator itr;

    itr = mMultiImpl.find(hwCtxImpl);
    if (itr == mMultiImpl.end()) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
          "Cannot retrieve ML Timeline data as a new HW Context Implementation is passed.");
      return;
    }

    itr->second.second->finishflushDevice(hwCtxImpl, itr->second.first);
    itr->second.second.reset(nullptr);
    mMultiImpl.erase(itr);
#endif
  }

  void MLTimelinePlugin::writeAll(bool /*openNewFiles*/)
  {
#if defined(XDP_CLIENT_BUILD) || defined(XDP_VE2_BUILD)
    for (auto &e : mMultiImpl) {
      if (nullptr == e.second.second)
        continue;
      e.second.second->finishflushDevice(e.first, e.second.first);
      e.second.second.reset(nullptr);
    }
    mMultiImpl.clear();
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
        "In ML Timeline Plugin : All data have been dumped.");
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
