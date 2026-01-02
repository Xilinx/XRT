// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_profile/aie_profile_plugin.h"

#include <boost/algorithm/string.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "core/common/api/hw_context_int.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/include/xrt/experimental/xrt-next.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/device/xdp_base_device.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/aie_base/aie_base_util.h"
#include "xdp/profile/writer/aie_profile/aie_writer.h"

#ifdef XDP_NPU3_BUILD
#include "client/aie_profile.h"
#include "client/aie_profile_npu3.h"
#elif XDP_CLIENT_BUILD
#include "client/aie_profile.h"
#elif XRT_X86_BUILD
#include "x86/aie_profile.h"
#elif XDP_VE2_BUILD
#include "ve2/aie_profile.h"
#else
#include "core/edge/user/shim.h"
#include "edge/aie_profile.h"
#endif

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  bool AieProfilePlugin::live = false;
  bool AieProfilePlugin::configuredOnePartition = false;

  AieProfilePlugin::AieProfilePlugin() : XDPPlugin()
  {
    AieProfilePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_profile);
    db->getStaticInfo().setAieApplication();
  }

  AieProfilePlugin::~AieProfilePlugin()
  {
    xrt_core::message::send(severity_level::info, "XRT", "Destroying AIE Profiling Plugin.");
    // Stop the polling thread

    AieProfilePlugin::live = false;
    endPoll();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }

      db->unregisterPlugin(this);
    }

  }

  bool AieProfilePlugin::alive()
  {
    return AieProfilePlugin::live;
  }

  uint64_t AieProfilePlugin::getDeviceIDFromHandle(void* handle)
  {
    auto itr = handleToAIEProfileImpl.find(handle);
    if (itr != handleToAIEProfileImpl.end())
      return itr->second->getDeviceID();

    return (db->getStaticInfo()).getDeviceContextUniqueId(handle);
  }

  void AieProfilePlugin::updateAIEDevice(void* handle, bool hw_context_flow)
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE Profile update AIE device.");

    // Don't update if no profiling is requested
    if (!xrt_core::config::get_aie_profile())
      return;

    if (!handle)
      return;

    if (!((db->getStaticInfo()).continueXDPConfig(hw_context_flow))) 
      return;

    // In a multipartition scenario, if the user wants to profile one specific partition
    // and we have configured one partition, we can skip the rest of them
    if ((xrt_core::config::get_aie_profile_settings_config_one_partition()) && (configuredOnePartition)) {
      xrt_core::message::send(severity_level::warning, "XRT", 
        "AIE Profile: A previous partition has already been configured. Skipping current partition due to 'config_one_partition=true' setting.");
      return;
    }

    if (hw_context_flow) {
      xrt::hw_context ctx = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
      if (xrt_core::hw_context_int::get_elf_flow(ctx)) {
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
            "AIE Profile is not yet supported for Full ELF flow.");
        return;
      }
    }

    auto device = util::convertToCoreDevice(handle, hw_context_flow);
#if ! defined (XRT_X86_BUILD) && ! defined (XDP_CLIENT_BUILD)
    if (1 == device->get_device_id() && xrt_core::config::get_xdp_mode() == "xdna") {  // Device 0 for xdna(ML) and device 1 for zocl(PL)
      xrt_core::message::send(severity_level::warning, "XRT", "Got ZOCL device when xdp_mode is set to XDNA. AIE Profiling is not yet supported for this combination.");
      return;
    }
    else if(0 == device->get_device_id() && xrt_core::config::get_xdp_mode() == "zocl") {
  #ifdef XDP_VE2_ZOCL_BUILD
      xrt_core::message::send(severity_level::warning, "XRT", "Got XDNA device when xdp_mode is set to ZOCL. AIE Profiling is not yet supported for this combination.");
      return;
  #else
      xrt_core::message::send(severity_level::debug, "XRT", "Got EDGE device when xdp_mode is set to ZOCL. AIE Profiling should be available.");
  #endif
    }
#endif

    auto deviceID = getDeviceIDFromHandle(handle);
    // Update the static database with information from xclbin
    {
#ifdef XDP_CLIENT_BUILD
      (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceID, device);
      (db->getStaticInfo()).setDeviceName(deviceID, "win_device");
#else
      if ((db->getStaticInfo()).getAppStyle() == xdp::AppStyle::REGISTER_XCLBIN_STYLE)
        (db->getStaticInfo()).updateDeviceFromCoreDeviceHwCtxFlow(deviceID, device, handle, hw_context_flow);
      else
        (db->getStaticInfo()).updateDeviceFromHandle(deviceID, nullptr, handle);
#endif
    }

    // Delete old data
    if (handleToAIEProfileImpl.find(handle) != handleToAIEProfileImpl.end())
#ifdef XDP_CLIENT_BUILD
      return;
#else
      handleToAIEProfileImpl.erase(handle);
#endif

    std::shared_ptr<AieProfileMetadata> metadata = std::make_shared<AieProfileMetadata>(deviceID, handle);
    if (metadata->aieMetadataEmpty()) {
      xrt_core::message::send(severity_level::debug, "XRT", "AIE Profile : no AIE metadata available for this xclbin update, skipping updateAIEDevice()");
      return;
    }
    
    // If there are tiles configured for this xclbin, then we have configured the first matching xclbin and will not configure any upcoming ones
    if ((xrt_core::config::get_aie_profile_settings_config_one_partition()) && (metadata->isConfigured()))
      configuredOnePartition = true;

    std::unique_ptr<AieProfileImpl> implementation;
#ifdef XDP_CLIENT_BUILD
    xrt::hw_context context = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
    metadata->setHwContext(context);
  #ifdef XDP_NPU3_BUILD
    if (aie::isNPU3(metadata->getHardwareGen()))
      implementation = std::make_unique<AieProfile_NPU3Impl>(db, metadata, deviceID);
    else
  #endif
      implementation = std::make_unique<AieProfile_WinImpl>(db, metadata, deviceID);
#elif XRT_X86_BUILD
    implementation = std::make_unique<AieProfile_x86Impl>(db, metadata, deviceID);
#elif XDP_VE2_BUILD
    implementation = std::make_unique<AieProfile_VE2Impl>(db, metadata, deviceID);
#else
    implementation = std::make_unique<AieProfile_EdgeImpl>(db, metadata, deviceID);
#endif
    
    // Ensure we only read/configure once per xclbin
    if (!(db->getStaticInfo()).isAIECounterRead(deviceID)) {
      // Sets up and calls the PS kernel on x86 implementation
      // Sets up and the hardware on the edge implementation
      implementation->updateDevice();

      (db->getStaticInfo()).setIsAIECounterRead(deviceID, true);
    }

    (db->getStaticInfo()).saveProfileConfig(metadata->createAIEProfileConfig(), deviceID);

// Open the writer for this device
auto time = std::time(nullptr);
#ifdef _WIN32
    std::tm tm{};
    localtime_s(&tm, &time);
    std::string deviceName = "win_device";
#else
    auto tm = *std::localtime(&time);
    std::string deviceName = util::getDeviceName(handle, hw_context_flow);
#endif

    std::ostringstream timeOss;
    timeOss << std::put_time(&tm, "_%Y_%m_%d_%H%M%S");
    std::string timestamp = timeOss.str();

    std::string outputFile = "aie_profile_" + deviceName + "_" + std::to_string(deviceID) + timestamp + ".csv";

    VPWriter* writer = new AIEProfilingWriter(outputFile.c_str(), deviceName.c_str(), deviceID);
    writers.push_back(writer);
    db->addOpenedFile(writer->getcurrentFileName(), "AIE_PROFILE", deviceID);

    handleToAIEProfileImpl[handle] = std::move(implementation);
    // Start the AIE profiling thread
    handleToAIEProfileImpl[handle]->startPoll(deviceID);
  }

  void AieProfilePlugin::writeAll(bool /*openNewFiles*/)
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE Profile writeall.");

    for (const auto& kv : handleToAIEProfileImpl) {
      // End polling thread
      endPollforDevice(kv.first);
    }

    XDPPlugin::endWrite();
    handleToAIEProfileImpl.clear();
  }

  void AieProfilePlugin::endPollforDevice(void* handle)
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE Profile endPollForDevice.");

    if (!handle)
      return;
    
    // mark the hw_ctx handle as invalid for current plugin
    (db->getStaticInfo()).unregisterPluginFromHwContext(handle);

    if (handleToAIEProfileImpl.empty())
      return;

    auto& implementation = handleToAIEProfileImpl[handle];
    if (!implementation) {
      handleToAIEProfileImpl.erase(handle);
      return;
    }
      
    #ifdef XDP_CLIENT_BUILD
      implementation->poll(0);
    #endif

    implementation->endPoll();
    handleToAIEProfileImpl.erase(handle);
  }

  void AieProfilePlugin::endPoll()
  {
    xrt_core::message::send(severity_level::info, "XRT", "Calling AIE Profile endPoll.");

    #ifdef XDP_CLIENT_BUILD
      auto& implementation = handleToAIEProfileImpl.begin()->second;
      implementation->poll(0);
    #endif
    // Ask all threads to end
    for (auto& p : handleToAIEProfileImpl) {
      if (p.second)
        p.second->endPoll();
    }
    handleToAIEProfileImpl.clear();
  }

  void AieProfilePlugin::broadcast(VPDatabase::MessageType msg, void* /*blob*/)
  {
     switch(msg) {
      case VPDatabase::MessageType::DUMP_AIE_PROFILE:
        {
          XDPPlugin::trySafeWrite("AIE_PROFILE", false);
        }
        break;

      default:
        break;
     }
  }
}  // end namespace xdp
