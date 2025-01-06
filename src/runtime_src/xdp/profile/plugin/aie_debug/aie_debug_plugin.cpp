// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

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
    db->getStaticInfo().setAieApplication();
  }

  /****************************************************************************
   * Destructor
   ***************************************************************************/
  AieDebugPlugin::~AieDebugPlugin()
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling ~AieDebugPlugin destructor.");

    for (const auto& kv : handleToAIEData)
      endPollforDevice(kv.first);

    XDPPlugin::endWrite();
    handleToAIEData.clear();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }
      db->unregisterPlugin(this);}

    AieDebugPlugin::live = false;
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
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE DEBUG AieDebugPlugin::getDeviceIDFromHandle.");
    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

#ifdef XDP_CLIENT_BUILD
    return db->addDevice("win_device");
#else
    //return db->addDevice(util::getDebugIpLayoutPath(handle));
    return db->addDevice("temp_edge_device");
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
#ifdef XDP_CLIENT_BUILD
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
#ifdef XDP_CLIENT_BUILD
      (db->getStaticInfo()).updateDeviceClient(deviceID, device);
      (db->getStaticInfo()).setDeviceName(deviceID, "win_device");
#else
      (db->getStaticInfo()).updateDevice(deviceID, nullptr, handle);
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
    std::string deviceName = "win_device";
#else
    auto tm = *std::localtime(&time);
    std::string deviceName = util::getDeviceName(handle);
#endif

    auto isDetailedInterpretation = xrt_core::config::get_aie_debug_settings_detailed_interpretation();
    std::ostringstream timeOss;
    timeOss << std::put_time(&tm, "_%Y_%m_%d_%H%M%S");
    std::string timestamp = timeOss.str();

    std::string outputFile = "aie_debug_" + deviceName + timestamp + ".csv";
    VPWriter* writer = new AIEDebugWriter(outputFile.c_str(), deviceName.c_str(), mIndex, isDetailedInterpretation);
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_DEBUG");

    mIndex++;
  }

  /****************************************************************************
   * Finish debugging
   ***************************************************************************/
  void AieDebugPlugin::endAIEDebugRead(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "AIE Debug endAIEDebugRead");
    auto deviceID = getDeviceIDFromHandle(handle);
    xrt_core::message::send(severity_level::debug, "XRT",
      "AieDebugPlugin::endAIEDebugRead deviceID is " + std::to_string(deviceID));
    handleToAIEData[handle].implementation->poll(deviceID, handle);
  }

  /****************************************************************************
   * End polling
   ***************************************************************************/
  void AieDebugPlugin::endPollforDevice(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "AIE Debug endPollforDevice");
    if (handleToAIEData.empty())
      return;

    auto& AIEData = handleToAIEData[handle];
    if (!AIEData.valid)
      return;

    //AIEData.implementation->poll(0, handle);

    handleToAIEData.erase(handle);
  }

}  // end namespace xdp

