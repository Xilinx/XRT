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

#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/include/experimental/xrt-next.h"

#include "xdp/profile/device/utility.h"
#include "xdp/profile/device/xdp_base_device.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_debug/aie_debug_writer.h"

#ifdef XDP_CLIENT_BUILD
#include "client/aie_debug.h"
#elif XDP_VE2_BUILD
#include "ve2/aie_debug.h"
#else
#include "core/edge/user/shim.h"
#include "edge/aie_debug.h"
#endif

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;

  bool AieDebugPlugin::live = false;

  /****************************************************************************
   * Constructor
   ***************************************************************************/
  AieDebugPlugin::AieDebugPlugin() : XDPPlugin()
  {
    AieDebugPlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_debug);
    db->getStaticInfo().setAieApplication();
  }

  /****************************************************************************
   * Destructor
   ***************************************************************************/
  AieDebugPlugin::~AieDebugPlugin()
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling ~AieDebugPlugin destructor.");

    AieDebugPlugin::live = false;
    for (const auto& kv : handleToAIEData)
      endPollforDevice(kv.first);

    XDPPlugin::endWrite();
    handleToAIEData.clear();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }
      db->unregisterPlugin(this);
    }
  }

  /****************************************************************************
   * Check if plugin is alive
   ***************************************************************************/
  bool AieDebugPlugin::alive()
  {
    return AieDebugPlugin::live;
  }

  /****************************************************************************
   * Get device ID from handle
   ***************************************************************************/
  uint64_t AieDebugPlugin::getDeviceIDFromHandle(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE DEBUG getDeviceIDFromHandle.");
    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

#ifdef XDP_CLIENT_BUILD
    return db->addDevice("win_device");
#elif XDP_VE2_BUILD
    return db->addDevice("ve2_device");
#else
    return db->addDevice(util::getDebugIpLayoutPath(handle)); // Get the unique device Id
#endif
  }

  /****************************************************************************
   * Update AIE device
   ***************************************************************************/
  void AieDebugPlugin::updateAIEDevice(void* handle)
  {
    if (!xrt_core::config::get_aie_debug() || !handle)
      return;
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE DEBUG update AIE device.");

    // Handle relates to HW context in case of Client XRT
#if defined(XDP_CLIENT_BUILD) || defined(XDP_VE2_BUILD)
    xrt::hw_context context = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
    auto device = xrt_core::hw_context_int::get_core_device(context);
#else

#endif
    auto deviceID = getDeviceIDFromHandle(handle);
    std::stringstream msg;
    msg<<"AieDebugPlugin::updateAIEDevice. Device Id =. "<<deviceID;
    xrt_core::message::send(severity_level::info, "XRT", msg.str());
    // Update the static database with information from xclbin
    {
#if defined(XDP_CLIENT_BUILD)
      (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceID, device);
      (db->getStaticInfo()).setDeviceName(deviceID, "win_device");
#elif defined(XDP_VE2_BUILD)
      (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceID, device);
      std::string deviceName = util::getDeviceName(handle,true);
      if (deviceName != "")
        (db->getStaticInfo()).setDeviceName(deviceID, deviceName);
#else
      (db->getStaticInfo()).updateDeviceFromHandle(deviceID, nullptr, handle);
      std::string deviceName = util::getDeviceName(handle);
      if (deviceName != "")
        (db->getStaticInfo()).setDeviceName(deviceID, deviceName);
#endif
    }

    // Delete old data
    if (handleToAIEData.find(handle) != handleToAIEData.end())
#ifdef XDP_CLIENT_BUILD
      return;
#else
      handleToAIEData.erase(handle);
#endif
    auto& AIEData = handleToAIEData[handle];

    AIEData.deviceID = deviceID;
    AIEData.metadata = std::make_unique<AieDebugMetadata>(deviceID, handle);
    if (AIEData.metadata->aieMetadataEmpty())
    {
      AIEData.valid = false;
      xrt_core::message::send(severity_level::debug, "XRT",
        "AIE DEBUG : no AIE metadata available for this xclbin update, skipping updateAIEDevice()");
      return;
    }
    AIEData.valid = true;

    // Parse user settings
    AIEData.metadata->parseMetrics();

#ifdef XDP_CLIENT_BUILD
    AIEData.metadata->setHwContext(context);
    AIEData.implementation = std::make_unique<AieDebug_WinImpl>(db, AIEData.metadata);
#elif XDP_VE2_BUILD
    AIEData.implementation = std::make_unique<AieDebug_VE2Impl>(db, AIEData.metadata);
#else
    AIEData.implementation = std::make_unique<AieDebug_EdgeImpl>(db, AIEData.metadata);
#endif

    auto& implementation = AIEData.implementation;
    implementation->updateAIEDevice(handle);

    // Open writer for this device
    auto time = std::time(nullptr);
#ifdef _WIN32
    std::tm tm{};
    localtime_s(&tm, &time);
    std::string deviceName = "aie_debug_win_device";
#else
    auto tm = *std::localtime(&time);
    #ifdef XDP_VE2_BUILD
      std::string deviceName = util::getDeviceName(handle,true);
    #else
      std::string deviceName = util::getDeviceName(handle);
    #endif
#endif

    std::ostringstream timeOss;
    timeOss << std::put_time(&tm, "_%Y_%m_%d_%H%M%S");
    std::string timestamp = timeOss.str();

    std::string outputFile = "aie_debug_" + deviceName + timestamp + ".csv";
    VPWriter* writer = new AIEDebugWriter(outputFile.c_str(), deviceName.c_str(), deviceID);
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_DEBUG");
  }

  /****************************************************************************
   * Finish debugging
   ***************************************************************************/
  void AieDebugPlugin::endAIEDebugRead(void* handle)
  {
    if (!mPollRegisters)
      return;

    auto deviceID = getDeviceIDFromHandle(handle);
    xrt_core::message::send(severity_level::debug, "XRT",
      "AieDebugPlugin::endAIEDebugRead - polling registers for device " + std::to_string(deviceID));

    // Poll all requested AIE registers
    handleToAIEData[handle].implementation->poll(deviceID, handle);
    mPollRegisters = false;
  }

  /****************************************************************************
   * End polling
   ***************************************************************************/
  void AieDebugPlugin::endPollforDevice(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "AIE Debug endPollforDevice");
    if (handleToAIEData.find(handle) == handleToAIEData.end())
      return;

    auto& AIEData = handleToAIEData[handle];
    if (!AIEData.valid)
      return;

    // Poll all requested AIE registers (if not done already)
    if (mPollRegisters) {
      auto deviceID = getDeviceIDFromHandle(handle);
      xrt_core::message::send(severity_level::debug, "XRT",
        "AieDebugPlugin::endPollforDevice - polling registers for device " + std::to_string(deviceID));

      AIEData.implementation->poll(deviceID, handle);
      mPollRegisters = false;
    }

    handleToAIEData.erase(handle);
  }

}  // end namespace xdp

