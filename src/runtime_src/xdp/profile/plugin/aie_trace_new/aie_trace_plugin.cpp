/**
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

#include <memory>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"
#include "xdp/profile/writer/aie_trace/aie_trace_config_writer.h"

#ifdef XRT_X86_BUILD
  #include "x86/aie_trace.h"
#else
  #include "edge/aie_trace.h"
  #include "core/edge/user/shim.h"
#endif

#include "aie_trace_impl.h"
#include "aie_trace_plugin.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  bool AieTracePluginUnified::live = false;

  AieTracePluginUnified::AieTracePluginUnified()
  : XDPPlugin()
  {
    AieTracePluginUnified::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_trace);
  }

  AieTracePluginUnified::~AieTracePluginUnified()
  {
    if (VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch(...) {
      }
      db->unregisterPlugin(this);
    }
    // If the database is dead, then we must have already forced a 
    //  write at the database destructor so we can just move on
    AieTracePluginUnified::live = false;
  }

  uint64_t AieTracePluginUnified::getDeviceIDFromHandle(void* handle)
  {
    auto itr = handleToAIEData.find(handle);
    if (itr != handleToAIEData.end())
      return itr->second.deviceID;

    std::array<char, sysfs_max_path_length> pathBuf = {0};
    xclGetDebugIPlayoutPath(handle, pathBuf.data(), (sysfs_max_path_length-1) ) ;
    std::string sysfspath(pathBuf.data());
    uint64_t deviceID =  db->addDevice(sysfspath); // Get the unique device Id
    return deviceID;
  }
  
  

  void AieTracePluginUnified::updateAIEDevice(void* handle)
  {
    if (!handle)
      return;

    // Clean out old data every time xclbin gets updated
    if (handleToAIEData.find(handle) != handleToAIEData.end())
        handleToAIEData.erase(handle);

    //Setting up struct 
    auto& AIEData = handleToAIEData[handle];

    auto deviceID = getDeviceIDFromHandle(handle);
    AIEData.deviceID = deviceID;
    AIEData.metadata = std::make_shared<AieTraceMetadata>(deviceID, handle);
    AIEData.valid = true; // initialize struct
    AIEData.devIntf = nullptr;

#ifdef XRT_X86_BUILD
    AIEData.implementation = std::make_unique<AieTrace_x86Impl>(db, AIEData.metadata);
#else
    AIEData.implementation = std::make_unique<AieTrace_EdgeImpl>(db, AIEData.metadata);
#endif


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
    
    // Check for device interface
    DeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceID);
    if (deviceIntf == nullptr) 
      deviceIntf = db->getStaticInfo().createDeviceIntf(deviceID, new HalDevice(handle));

    // Create gmio metadata
    if (!(db->getStaticInfo()).isGMIORead(deviceID)) {
      // Update the AIE specific portion of the device
      // When new xclbin is loaded, the xclbin specific datastructure is already recreated
      std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle) ;
      if (device != nullptr) {
        for (auto& gmio : AIEData.metadata->get_trace_gmios(device.get()))
          (db->getStaticInfo()).addTraceGMIO(deviceID, gmio.id, gmio.shimColumn, gmio.channelNum, gmio.streamId, gmio.burstLength);
      }
      (db->getStaticInfo()).setIsGMIORead(deviceID, true);
    }

    // Check if trace streams are available
    AIEData.metadata->setNumStreams((db->getStaticInfo()).getNumAIETraceStream(deviceID));
    if (AIEData.metadata->getNumStreams() == 0) {
      AIEData.valid = false;
      xrt_core::message::send(severity_level::warning, "XRT", AIE_TRACE_UNAVAILABLE);
      return;
    }

    if (AIEData.metadata->getRuntimeMetrics()) {
      std::string configFile = "aie_event_runtime_config.json";
      VPWriter* writer = new AieTraceConfigWriter(configFile.c_str(),deviceID);
      writers.push_back(writer);
      (db->getStaticInfo()).addOpenedFile(writer->getcurrentFileName(), "AIE_EVENT_RUNTIME_CONFIG");
    }

    // Add writer for every stream
    for (uint64_t n = 0; n < AIEData.metadata->getNumStreams(); ++n) {
	    std::string fileName = "aie_trace_" + std::to_string(deviceID) + "_" + std::to_string(n) + ".txt";
      VPWriter* writer = new AIETraceWriter
      ( fileName.c_str()
      , deviceID
      , n  // stream id
      , "" // version
      , "" // creation time
      , "" // xrt version
      , "" // tool version
      );
      writers.push_back(writer);
      db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "AIE_EVENT_TRACE");

      std::stringstream msg;
      msg << "Creating AIE trace file " << fileName << " for device " << deviceID;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
    }

    // Ensure trace buffer size is appropriate
    uint64_t aieTraceBufSize = GetTS2MMBufSize(true /*isAIETrace*/);
    bool isPLIO = (db->getStaticInfo()).getNumTracePLIO(deviceID) ? true : false;

    if (AIEData.metadata->getContinuousTrace())
      XDPPlugin::startWriteThread(AIEData.metadata->getFileDumpIntS(), "AIE_EVENT_TRACE", false);

    // First, check against memory bank size
    // NOTE: Check first buffer for PLIO; assume bank 0 for GMIO
    uint8_t memIndex = isPLIO ? deviceIntf->getAIETs2mmMemIndex(0) : 0;
    Memory* memory = (db->getStaticInfo()).getMemory(deviceID, memIndex);
    if (memory != nullptr) {
      uint64_t fullBankSize = memory->size * 1024;

      if ((fullBankSize > 0) && (aieTraceBufSize > fullBankSize)) {
        aieTraceBufSize = fullBankSize;
        std::string msg = "Requested AIE trace buffer is too big for memory resource. Limiting to "
                        + std::to_string(fullBankSize) + "." ;
        xrt_core::message::send(severity_level::warning, "XRT", msg);
      }
    }
    // Ensures Contiguous Memory Allocation specific for linux/edge.
    aieTraceBufSize = AIEData.implementation->checkTraceBufSize(aieTraceBufSize);

    // Create AIE Trace Offloader
    AIEData.logger = std::make_unique<AIETraceDataLogger>(deviceID);

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
      std::string flowType = (isPLIO) ? "PLIO" : "GMIO";
      std::stringstream msg;
      msg << "Total size of " << std::fixed << std::setprecision(3)
          << (aieTraceBufSize / (1024.0 * 1024.0))
          << " MB is used for AIE trace buffer for "
          << std::to_string(AIEData.metadata->getNumStreams()) << " " << flowType
          << " streams.";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    AIEData.offloader = std::make_unique<AIETraceOffload>
    ( handle
    , deviceID
    , deviceIntf
    , AIEData.logger.get()
    , isPLIO              // isPLIO?
    , aieTraceBufSize     // total trace buffer size
    , AIEData.metadata->getNumStreams()
    );
    auto& offloader = AIEData.offloader;

    // Can't call init without setting important details in offloader
    if (AIEData.metadata->getContinuousTrace()) {
      offloader->setContinuousTrace();
      offloader->setOffloadIntervalUs(AIEData.metadata->getOffloadIntervalUs());
    }

    try {
      if (!offloader->initReadTrace()) {
        xrt_core::message::send(severity_level::warning, "XRT", AIE_TRACE_BUF_ALLOC_FAIL);
        AIEData.valid = false;
        return;
      }
    } catch (...) {
        std::string msg = "AIE trace is currently not supported on this platform.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        AIEData.valid = false;
    }

    //Sets up and calls the PS kernel on x86 implementation
    //Sets up and the hardware on the edge implementation
    AIEData.implementation->updateDevice();

    // Continuous Trace Offload is supported only for PLIO flow
    if (AIEData.metadata->getContinuousTrace())
      offloader->startOffload();
  }

  void AieTracePluginUnified::flushOffloader(const std::unique_ptr<AIETraceOffload>& offloader, bool warn)
  {
    if (offloader->continuousTrace()) {
      offloader->stopOffload();
      while(offloader->getOffloadStatus() != AIEOffloadThreadStatus::STOPPED);
    } else {
      offloader->readTrace(true);
      offloader->endReadTrace();
    } 

    if (warn && offloader->isTraceBufferFull())
      xrt_core::message::send(severity_level::warning, "XRT", AIE_TS2MM_WARN_MSG_BUF_FULL);
  }

  void AieTracePluginUnified::flushAIEDevice(void* handle)
  {
    if (!handle)
      return;

    auto itr = handleToAIEData.find(handle);
    if (itr == handleToAIEData.end())
      return;

    auto& AIEData = itr->second;
    if (!AIEData.valid)
      return;

    flushOffloader(AIEData.offloader, false);
  }

  void AieTracePluginUnified::finishFlushAIEDevice(void* handle)
  {
    if (!handle)
      return;

    auto itr = handleToAIEData.find(handle);
    if (itr == handleToAIEData.end())
      return;

    auto& AIEData = itr->second;
    if (!AIEData.valid)
      return;

    flushOffloader(AIEData.offloader, true);
    XDPPlugin::endWrite();
    (db->getStaticInfo()).deleteCurrentlyUsedDeviceInterface(AIEData.deviceID);
    
    handleToAIEData.erase(itr);
  }

  void AieTracePluginUnified::writeAll(bool openNewFiles)
  {
    for (const auto& kv : handleToAIEData) {
      auto& AIEData = kv.second;
      if (AIEData.valid)
        flushOffloader(AIEData.offloader, true);
    }

    XDPPlugin::endWrite();
    handleToAIEData.clear();
	
  }

  bool AieTracePluginUnified::alive()
  {
    return AieTracePluginUnified::live;
  }

} // end namespace xdp
