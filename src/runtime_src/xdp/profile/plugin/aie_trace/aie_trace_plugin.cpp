/**
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "core/common/api/device_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/message.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_trace/aie_trace_config_writer.h"
#include "xdp/profile/writer/aie_trace/aie_trace_timestamps_writer.h"
// #include "xdp/profile/writer/aie_trace/aie_trace_writer.h"
#include "aie_trace_offload_manager.h"

#ifdef XDP_CLIENT_BUILD
#include "client/aie_trace.h"
#elif defined(XRT_X86_BUILD)
#include "x86/aie_trace.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#elif XDP_VE2_BUILD
#include "ve2/aie_trace.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#else
#include "edge/aie_trace.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#endif

#include "aie_trace_impl.h"
#include "aie_trace_plugin.h"

#ifdef _WIN32
#pragma warning(disable : 4702) //TODO remove when test is implemented properly
#endif

namespace xdp {
using severity_level = xrt_core::message::severity_level;
bool AieTracePluginUnified::live = false;
bool AieTracePluginUnified::configuredOnePartition = false;
bool AieTracePluginUnified::configuredOnePlioPartition = false; // For register xclbin flow 

AieTracePluginUnified::AieTracePluginUnified() : XDPPlugin() {
  AieTracePluginUnified::live = true;

  db->registerPlugin(this);
  db->registerInfo(info::aie_trace);
  db->getStaticInfo().setAieApplication();
}

AieTracePluginUnified::~AieTracePluginUnified() {
  xrt_core::message::send(severity_level::info, "XRT",
                          "Destroying AIE Trace Plugin");

  // Stop thread to write timestamps
  endPoll();

  if (VPDatabase::alive()) {
    try {
      writeAll(false);
    } catch (...) {
    }

    db->unregisterPlugin(this);
  }

  // If the database is dead, then we must have already forced a
  // write at the database destructor so we can just move on
  AieTracePluginUnified::live = false;
}

uint64_t AieTracePluginUnified::getDeviceIDFromHandle(void *handle) {
  auto itr = handleToAIEData.find(handle);

  if (itr != handleToAIEData.end())
    return itr->second.deviceID;

  return (db->getStaticInfo()).getDeviceContextUniqueId(handle);
}

void AieTracePluginUnified::updateAIEDevice(void *handle, bool hw_context_flow) {
  xrt_core::message::send(severity_level::info, "XRT",
                          "Calling AIE Trace updateAIEDevice.");

  if (!handle)
    return;
  
  if (!((db->getStaticInfo()).continueXDPConfig(hw_context_flow)))
    return;

  // In a multipartition scenario, if the user wants to trace one specific partition
  // and we have already configured one partition, we can skip the rest of them
  if ((xrt_core::config::get_aie_trace_settings_config_one_partition()) && (configuredOnePartition)) {
    xrt_core::message::send(severity_level::warning, "XRT",
      "AIE Trace: A previous partition has already been configured. Skipping current partition due to 'config_one_partition=true' setting.");
    return;
  }
  
  auto device = util::convertToCoreDevice(handle, hw_context_flow);
#if ! defined (XRT_X86_BUILD) && ! defined (XDP_CLIENT_BUILD)
  if (1 == device->get_device_id() && xrt_core::config::get_xdp_mode() == "xdna") {  // Device 0 for xdna(ML) and device 1 for zocl(PL)
    xrt_core::message::send(severity_level::warning, "XRT", "Got ZOCL device when xdp_mode is set to XDNA. AIE Event Trace is not yet supported for this combination.");
    return;
  }
  else if(0 == device->get_device_id() && xrt_core::config::get_xdp_mode() == "zocl") {
  #ifdef XDP_VE2_ZOCL_BUILD
    xrt_core::message::send(severity_level::warning, "XRT", "Got XDNA device when xdp_mode is set to ZOCL. AIE Event Trace is not yet supported for this combination.");
    return;
  #else
    xrt_core::message::send(severity_level::debug, "XRT", "Got EDGE device when xdp_mode is set to ZOCL. AIE Event Trace should be available.");
  #endif
    }
#endif
  // Clean out old data every time xclbin gets updated
  if (handleToAIEData.find(handle) != handleToAIEData.end())
    handleToAIEData.erase(handle);

  auto deviceID = getDeviceIDFromHandle(handle);

  // Setting up struct
  auto &AIEData = handleToAIEData[handle];
  AIEData.deviceID = deviceID;
  AIEData.valid = true; // initialize struct

  // Update the static database with information from xclbin
#ifdef XDP_CLIENT_BUILD
  (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceID, device);
  (db->getStaticInfo()).setDeviceName(deviceID, "win_device");  
#else
    if((db->getStaticInfo()).getAppStyle() == xdp::AppStyle::REGISTER_XCLBIN_STYLE)
      (db->getStaticInfo()).updateDeviceFromCoreDeviceHwCtxFlow(deviceID, device, handle, hw_context_flow, true, std::move(std::make_unique<HalDevice>(device->get_device_handle())));
    else
      (db->getStaticInfo()).updateDeviceFromHandle(deviceID, std::move(std::make_unique<HalDevice>(handle)), handle);
#endif

  // Metadata depends on static information from the database
  AIEData.metadata = std::make_shared<AieTraceMetadata>(deviceID, handle);
  if(AIEData.metadata->aieMetadataEmpty())
  {
    AIEData.valid = false;
    xrt_core::message::send(severity_level::warning, "XRT", "AIE Metadata is empty for AIE Trace");
    return;
  }
  if (AIEData.metadata->configMetricsEmpty()) {
    AIEData.valid = false;
    xrt_core::message::send(severity_level::warning, "XRT",
                            AIE_TRACE_TILES_UNAVAILABLE);
    return;
  }
  AIEData.valid = true; // initialize struct

  // If there are tiles configured for this xclbin, then we have configured the first matching xclbin and will not configure any upcoming ones
  if ((xrt_core::config::get_aie_trace_settings_config_one_partition()) && !(AIEData.metadata->configMetricsEmpty()))
    configuredOnePartition = true;

#ifdef XDP_CLIENT_BUILD
  xrt::hw_context context = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
  AIEData.metadata->setHwContext(context);
  AIEData.implementation = std::make_unique<AieTrace_WinImpl>(db, AIEData.metadata);
#elif defined(XRT_X86_BUILD)
  AIEData.implementation = std::make_unique<AieTrace_x86Impl>(db, AIEData.metadata);
#elif XDP_VE2_BUILD
  AIEData.implementation = std::make_unique<AieTrace_VE2Impl>(db, AIEData.metadata);
#else
  AIEData.implementation = std::make_unique<AieTrace_EdgeImpl>(db, AIEData.metadata);
#endif

  // Check for device interface
  PLDeviceIntf *deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceID);

  // Create gmio metadata
  if (!(db->getStaticInfo()).isGMIORead(deviceID)) {
    // Update the AIE specific portion of the device
    // When new xclbin is loaded, the xclbin specific datastructure is already
    // recreated
    if (device != nullptr) {
      for (auto &gmioEntry : AIEData.metadata->get_trace_gmios()) {
        auto gmio = gmioEntry.second;
        // Get the column shift for partition
        // NOTE: If partition is not used, this value is zero.
        // This is later required for GMIO trace offload.
        uint8_t startColShift = AIEData.metadata->getPartitionOverlayStartCols().front();
        // Get the column relative to partition.
        // For loadxclbin flow currently XRT creates partition of whole device from 0th column.
        // Hence absolute and relative columns are same.
        // TODO: For loadxclbin flow XRT will start creating partition of the specified columns,
        //       hence we should stop adding partition shift to col for passing to XAIE Apis.
        uint8_t relCol = ((db->getStaticInfo()).getAppStyle() == xdp::AppStyle::LOAD_XCLBIN_STYLE) ? gmio.shimColumn + startColShift : gmio.shimColumn;
        (db->getStaticInfo()).addTraceGMIO(deviceID, gmio.id, relCol, gmio.channelNum,
                                            gmio.streamId, gmio.burstLength);
      }
    }

    (db->getStaticInfo()).setIsGMIORead(deviceID, true);
  }

  // Check if trace streams are available TODO
  AIEData.metadata->setNumStreamsPLIO(
      (db->getStaticInfo()).getNumAIETraceStream(deviceID, io_type::PLIO));
  AIEData.metadata->setNumStreamsGMIO(
      (db->getStaticInfo()).getNumAIETraceStream(deviceID, io_type::GMIO));

  uint64_t numStreamsPLIO = AIEData.metadata->getNumStreamsPLIO();
  uint64_t numStreamsGMIO = AIEData.metadata->getNumStreamsGMIO();
  bool isPLIO = (numStreamsPLIO > 0) ? true : false;
  bool isGMIO = (numStreamsGMIO > 0) ? true : false;

  // Check if we've already configured a PLIO partition and current partition also has PLIO
  // If so, skip this entire partition. GMIO-only partitions are still allowed.
  // This is applicable only for register xclbin flow.
  if ((db->getStaticInfo()).getAppStyle() == xdp::AppStyle::REGISTER_XCLBIN_STYLE &&
          isPLIO && !isGMIO && configuredOnePlioPartition) {
    xrt_core::message::send(severity_level::critical, "XRT",
      "AIE Trace: PLIO offload is not supported on multiple partitions at once. "
      "A previous PLIO partition has already been configured. "
      "Skipping current PLIO partition.");
    AIEData.valid = false;
    return;
  }

  if ((AIEData.metadata->getNumStreamsPLIO() == 0) && 
      (AIEData.metadata->getNumStreamsGMIO() == 0)) {
    AIEData.valid = false;
    xrt_core::message::send(severity_level::warning, "XRT",
                            AIE_TRACE_UNAVAILABLE);
    return;
  }

  if (AIEData.metadata->getRuntimeMetrics()) {
    std::string configFile = "aie_event_runtime_config_" + std::to_string(deviceID) + ".json";
    VPWriter *writer = new AieTraceConfigWriter(configFile.c_str(), deviceID);
    writers.push_back(writer);
    db->addOpenedFile(writer->getcurrentFileName(),
                      "AIE_EVENT_RUNTIME_CONFIG",
		      deviceID);
  }

  if (!AIEData.offloadManager)
    AIEData.offloadManager = std::make_unique<AIETraceOffloadManager>(deviceID, db, AIEData.implementation.get());
  
  AIEData.offloadManager->createTraceWriters(numStreamsPLIO, numStreamsGMIO, writers);

  // Ensure trace buffer size is appropriate
  uint64_t aieTraceBufSize = GetTS2MMBufSize(true /*isAIETrace*/);
  // uint64_t aieTraceBufSizePLIO = aieTraceBufSize;
  // uint64_t aieTraceBufSizeGMIO = aieTraceBufSize;
  if (isPLIO && !configuredOnePlioPartition) {

    XAie_DevInst* devInst = static_cast<XAie_DevInst*>(AIEData.implementation->setAieDeviceInst(handle, deviceID));
    if(!devInst) {
      xrt_core::message::send(severity_level::warning, "XRT",
        "Unable to get AIE device instance. AIE event trace will not be available.");
      return;
    }
    AIEData.offloadManager->configureAndInitPLIO(handle, deviceIntf, aieTraceBufSize,
                                      AIEData.metadata->getNumStreamsPLIO(), devInst);
    // Mark that we've successfully configured the first PLIO partition
    configuredOnePlioPartition = true;
  }
  if (isGMIO) {
#ifdef XDP_CLIENT_BUILD
  if (!AIEData.offloadManager->configureAndInitGMIO(
        handle, deviceIntf, aieTraceBufSize,
        AIEData.metadata->getNumStreamsGMIO(),
        AIEData.metadata->getHwContext(), AIEData.metadata))
    return;
#else
  XAie_DevInst* devInst =
    static_cast<XAie_DevInst*>(AIEData.implementation->setAieDeviceInst(handle, deviceID));
  if (!AIEData.offloadManager->configureAndInitGMIO(
        handle, deviceIntf, aieTraceBufSize,
        AIEData.metadata->getNumStreamsGMIO(), devInst))
    return;
#endif
  }

  auto &offloaderManager = AIEData.offloadManager;
  try {
    if (!offloaderManager->initReadTraces()) {
      xrt_core::message::send(severity_level::warning, "XRT",
                              AIE_TRACE_BUF_ALLOC_FAIL);
      AIEData.valid = false;
      return;
    }
  } catch (...) {
    std::string msg = "AIE trace is currently not supported on this platform.";
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                            msg);
    AIEData.valid = false;
  }

  // Support system timeline
  if (xrt_core::config::get_aie_trace_settings_enable_system_timeline()) {
#ifdef _WIN32
    std::string deviceName = "win_device";
#else
    std::string deviceName = util::getDeviceName(handle, hw_context_flow);
#endif

    // Writer for timestamp file
    std::string outputFile = "aie_event_timestamps.bin";
    auto tsWriter = new AIETraceTimestampsWriter(outputFile.c_str(),
                                                 deviceName.c_str(), deviceID);
    writers.push_back(tsWriter);
    db->addOpenedFile(tsWriter->getcurrentFileName(),
                      "AIE_EVENT_TRACE_TIMESTAMPS",
		      deviceID);

    // Start the AIE trace timestamps thread
    // NOTE: we purposely start polling before configuring trace events
    AIEData.pollAIETimerThreadCtrlBool = true;
    auto device_thread = std::thread(&AieTracePluginUnified::pollAIETimers,
                                     this, deviceID, handle);
    AIEData.pollAIETimerThread = std::move(device_thread);
  } else {
    AIEData.pollAIETimerThreadCtrlBool = false;
  }

  // Sets up and calls the PS kernel on x86 implementation
  // Sets up and the hardware on the edge implementation
  xrt_core::message::send(severity_level::info, "XRT",
                          "Calling AIE Trace updateDevice.");

  AIEData.implementation->updateDevice();

  // Continuous Trace Offload is supported only for PLIO flow
  if (AIEData.metadata->getContinuousTrace())
    offloaderManager->startOffload(AIEData.metadata->getContinuousTrace(),
                                  AIEData.metadata->getOffloadIntervalUs());
  xrt_core::message::send(severity_level::info, "XRT",
                          "Finished AIE Trace updateAIEDevice.");
}

void AieTracePluginUnified::pollAIETimers(uint64_t index, void *handle) {
  auto it = handleToAIEData.find(handle);

  if (it == handleToAIEData.end())
    return;

  auto &should_continue = it->second.pollAIETimerThreadCtrlBool;

  while (should_continue) {
    handleToAIEData[handle].implementation->pollTimers(index, handle);
    std::this_thread::sleep_for(std::chrono::microseconds(
        handleToAIEData[handle].metadata->getPollingIntervalVal()));
  }
}

void AieTracePluginUnified::flushAIEDevice(void *handle) {
  if (!handle)
    return;

  auto itr = handleToAIEData.find(handle);

  if (itr == handleToAIEData.end())
    return;

  auto &AIEData = itr->second;

  if (!AIEData.valid)
    return;

  // Flush AIE then datamovers
  AIEData.implementation->flushTraceModules();
  if (AIEData.offloadManager)
    AIEData.offloadManager->flushAll(false);
}

void AieTracePluginUnified::finishFlushAIEDevice(void *handle) {
  xrt_core::message::send(severity_level::info, "XRT",
                          "Beginning AIE Trace finishFlushAIEDevice.");
  #ifdef XDP_CLIENT_BUILD
    // For now, just return please
    return;
  #endif

  if (!handle)
    return;

  // mark the hw_ctx handle as invalid for current plugin  
  (db->getStaticInfo()).unregisterPluginFromHwContext(handle);

  auto itr = handleToAIEData.find(handle);
  if (itr == handleToAIEData.end())
    return;

  auto &AIEData = itr->second;
  if (!AIEData.valid)
    return;

  // End polling thread
  endPollforDevice(handle);

  // Flush AIE then datamovers
  AIEData.implementation->flushTraceModules();
  if (AIEData.offloadManager)
    AIEData.offloadManager->flushAll(true);

  XDPPlugin::endWrite();

  handleToAIEData.erase(itr);
}

void AieTracePluginUnified::writeAll(bool openNewFiles) {
  xrt_core::message::send(severity_level::info, "XRT",
                          "Beginning AIE Trace WriteAll.");
  (void)openNewFiles;

  for (const auto &kv : handleToAIEData) {
    // End polling thread
    endPollforDevice(kv.first);

    auto &AIEData = kv.second;

    if (AIEData.valid) {
      AIEData.implementation->flushTraceModules();
      if (AIEData.offloadManager)
        AIEData.offloadManager->flushAll(true);
    }
  }

  XDPPlugin::endWrite();
  handleToAIEData.clear();
}

bool AieTracePluginUnified::alive() { return AieTracePluginUnified::live; }

void AieTracePluginUnified::endPollforDevice(void *handle) {
  auto itr = handleToAIEData.find(handle);

  if (itr == handleToAIEData.end())
    return;

  auto &AIEData = itr->second;

  if (!AIEData.valid || !AIEData.pollAIETimerThreadCtrlBool)
    return;

  AIEData.pollAIETimerThreadCtrlBool = false;

  if (AIEData.pollAIETimerThread.joinable())
    AIEData.pollAIETimerThread.join();

  if (AIEData.implementation)
    AIEData.implementation->freeResources();
}

void AieTracePluginUnified::endPoll() {
  // Ask all threads to end
  for (auto &p : handleToAIEData) {
    auto& data = p.second;
    if (data.pollAIETimerThreadCtrlBool) {
      data.pollAIETimerThreadCtrlBool = false;

      if (data.pollAIETimerThread.joinable())
        data.pollAIETimerThread.join();
      if (data.implementation)
        data.implementation->freeResources();
    }
  }
}
} // end namespace xdp
