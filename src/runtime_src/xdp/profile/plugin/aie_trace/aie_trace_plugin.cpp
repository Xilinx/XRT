/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"

#ifdef XDP_CLIENT_BUILD
#include "client/aie_trace.h"
#elif defined(XRT_X86_BUILD)
#include "x86/aie_trace.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#elif XDP_VE2_BUILD
#include "ve2/aie_trace.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "core/common/shim/hwctx_handle.h"
#include "shim/xdna_hwctx.h"
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

#ifdef XDP_CLIENT_BUILD
  return db->addDevice("win_sysfspath");
#elif XDP_VE2_BUILD
  return db->addDevice("ve2_device");
#else
  return db->addDevice(util::getDebugIpLayoutPath(handle)); // Get the unique device Id
#endif
}

void AieTracePluginUnified::updateAIEDevice(void *handle, bool hw_context_flow) {
  xrt_core::message::send(severity_level::info, "XRT",
                          "Calling AIE Trace updateAIEDevice.");

  if (!handle)
    return;
  
  auto device = util::convertToCoreDevice(handle, hw_context_flow);

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
#elif defined(XDP_VE2_BUILD)
  (db->getStaticInfo()).updateDeviceFromCoreDevice(deviceID, device, true, std::move(std::make_unique<HalDevice>(device->get_device_handle())));
#else
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
        (db->getStaticInfo()).addTraceGMIO(deviceID, gmio.id, gmio.shimColumn+startColShift,
                                           gmio.channelNum, gmio.streamId, gmio.burstLength);
      }
    }

    (db->getStaticInfo()).setIsGMIORead(deviceID, true);
  }

  // Check if trace streams are available TODO
  AIEData.metadata->setNumStreams(
      (db->getStaticInfo()).getNumAIETraceStream(deviceID));

  if (AIEData.metadata->getNumStreams() == 0) {
    AIEData.valid = false;
    xrt_core::message::send(severity_level::warning, "XRT",
                            AIE_TRACE_UNAVAILABLE);
    return;
  }

  if (AIEData.metadata->getRuntimeMetrics()) {
    std::string configFile = "aie_event_runtime_config.json";
    VPWriter *writer = new AieTraceConfigWriter(configFile.c_str(), deviceID);
    writers.push_back(writer);
    (db->getStaticInfo())
        .addOpenedFile(writer->getcurrentFileName(),
                       "AIE_EVENT_RUNTIME_CONFIG");
  }

  // Add writer for every stream
  for (uint64_t n = 0; n < AIEData.metadata->getNumStreams(); ++n) {
    std::string fileName = "aie_trace_" + std::to_string(deviceID) + "_" +
                           std::to_string(n) + ".txt";
    VPWriter *writer = new AIETraceWriter(
      fileName.c_str(),
      deviceID,
      n,  // stream id
      "", // version
      "", // creation time
      "", // xrt version
      ""  // tool version
    );
    writers.push_back(writer);
    db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(),
                                      "AIE_EVENT_TRACE");

    std::stringstream msg;
    msg << "Creating AIE trace file " << fileName << " for device " << deviceID;
    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  // Ensure trace buffer size is appropriate
  uint64_t aieTraceBufSize = GetTS2MMBufSize(true /*isAIETrace*/);
  bool isPLIO = (db->getStaticInfo()).getNumTracePLIO(deviceID) ? true : false;

  if (AIEData.metadata->getContinuousTrace())
    XDPPlugin::startWriteThread(AIEData.metadata->getFileDumpIntS(),
                                "AIE_EVENT_TRACE", false);

  // First, check against memory bank size
  // NOTE: Check first buffer for PLIO; assume bank 0 for GMIO
  uint8_t memIndex = 0;
  if (isPLIO && (deviceIntf != nullptr))
    memIndex = deviceIntf->getAIETs2mmMemIndex(0);

  Memory *memory = (db->getStaticInfo()).getMemory(deviceID, memIndex);

  if (memory != nullptr) {
    uint64_t fullBankSize = memory->size * 1024;

    if ((fullBankSize > 0) && (aieTraceBufSize > fullBankSize)) {
      aieTraceBufSize = fullBankSize;
      std::string msg = "Requested AIE trace buffer is too big for memory "
                        "resource. Limiting to " +
                        std::to_string(fullBankSize) + ".";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }
  }

  // Ensures Contiguous Memory Allocation specific for linux/edge.
  aieTraceBufSize = AIEData.implementation->checkTraceBufSize(aieTraceBufSize);

  // Create AIE Trace Offloader
  AIEData.logger = std::make_unique<AIETraceDataLogger>(deviceID);

  if (xrt_core::config::get_verbosity() >=
      static_cast<uint32_t>(severity_level::debug)) {
    std::string flowType = (isPLIO) ? "PLIO" : "GMIO";
    std::stringstream msg;
    msg << "Total size of " << std::fixed << std::setprecision(3)
        << (aieTraceBufSize / (1024.0 * 1024.0))
        << " MB is used for AIE trace buffer for "
        << std::to_string(AIEData.metadata->getNumStreams()) << " " << flowType
        << " streams.";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
  }

#ifdef XDP_CLIENT_BUILD
  AIEData.offloader = std::make_unique<AIETraceOffload>(
      handle, deviceID, deviceIntf, AIEData.logger.get(), isPLIO // isPLIO?
      ,
      aieTraceBufSize // total trace buffer size
      ,
      AIEData.metadata->getNumStreams(), AIEData.metadata->getHwContext(),
      AIEData.metadata);
#elif XDP_VE2_BUILD
  xrt::hw_context context = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
  auto hwctx_hdl = static_cast<xrt_core::hwctx_handle*>(context);
  auto hwctx_obj = dynamic_cast<shim_xdna_edge::xdna_hwctx*>(hwctx_hdl);
  auto aieObj = hwctx_obj->get_aie_array();
  XAie_DevInst* devInst = aieObj->get_dev();

  AIEData.offloader = std::make_unique<AIETraceOffload>(
      handle, deviceID, deviceIntf, AIEData.logger.get(), isPLIO // isPLIO?
      ,
      aieTraceBufSize // total trace buffer size
      ,
      AIEData.metadata->getNumStreams(), devInst);
#else
  AIEData.offloader = std::make_unique<AIETraceOffload>(
      handle, deviceID, deviceIntf, AIEData.logger.get(), isPLIO // isPLIO?
      ,
      aieTraceBufSize // total trace buffer size
      ,
      AIEData.metadata->getNumStreams());
#endif

  auto &offloader = AIEData.offloader;

  // Can't call init without setting important details in offloader
  if (AIEData.metadata->getContinuousTrace()) {
    offloader->setContinuousTrace();
    offloader->setOffloadIntervalUs(AIEData.metadata->getOffloadIntervalUs());
  }

  try {
    if (!offloader->initReadTrace()) {
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
    db->getStaticInfo().addOpenedFile(tsWriter->getcurrentFileName(),
                                      "AIE_EVENT_TRACE_TIMESTAMPS");

    // Start the AIE trace timestamps thread
    // NOTE: we purposely start polling before configuring trace events
    AIEData.threadCtrlBool = true;
    auto device_thread = std::thread(&AieTracePluginUnified::pollAIETimers,
                                     this, deviceID, handle);
    AIEData.thread = std::move(device_thread);
  } else {
    AIEData.threadCtrlBool = false;
  }

  // Sets up and calls the PS kernel on x86 implementation
  // Sets up and the hardware on the edge implementation
  xrt_core::message::send(severity_level::info, "XRT",
                          "Calling AIE Trace updateDevice.");

  AIEData.implementation->updateDevice();

  // Continuous Trace Offload is supported only for PLIO flow
  if (AIEData.metadata->getContinuousTrace())
    offloader->startOffload();
  xrt_core::message::send(severity_level::info, "XRT",
                          "Finished AIE Trace updateAIEDevice.");
}

void AieTracePluginUnified::pollAIETimers(uint64_t index, void *handle) {
  auto it = handleToAIEData.find(handle);

  if (it == handleToAIEData.end())
    return;

  auto &should_continue = it->second.threadCtrlBool;

  while (should_continue) {
    handleToAIEData[handle].implementation->pollTimers(index, handle);
    std::this_thread::sleep_for(std::chrono::microseconds(
        handleToAIEData[handle].metadata->getPollingIntervalVal()));
  }
}

void AieTracePluginUnified::flushOffloader(
    const std::unique_ptr<AIETraceOffload> &offloader, bool warn) {
  if (offloader->continuousTrace()) {
    offloader->stopOffload();

    while (offloader->getOffloadStatus() != AIEOffloadThreadStatus::STOPPED)
      ;
  } else {
    offloader->readTrace(true);
    offloader->endReadTrace();
  }

  if (warn && offloader->isTraceBufferFull())
    xrt_core::message::send(severity_level::warning, "XRT",
                            AIE_TS2MM_WARN_MSG_BUF_FULL);
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
  flushOffloader(AIEData.offloader, false);
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
  flushOffloader(AIEData.offloader, true);
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
      flushOffloader(AIEData.offloader, true);
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

  if (!AIEData.valid || !AIEData.threadCtrlBool)
    return;

  AIEData.threadCtrlBool = false;

  if (AIEData.thread.joinable())
    AIEData.thread.join();

  AIEData.implementation->freeResources();
}

void AieTracePluginUnified::endPoll() {
  // Ask all threads to end
  for (auto &p : handleToAIEData) {
    if (p.second.threadCtrlBool) {
      p.second.threadCtrlBool = false;

      if (p.second.thread.joinable())
        p.second.thread.join();
    }
  }
}
} // end namespace xdp
