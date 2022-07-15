/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <limits>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"
#include "xdp/profile/writer/aie_trace/aie_trace_config_writer.h"

// #ifdef EDGE_BUILD
// #include "edge/aie_trace.h"
// #include "core/edge/user/shim.h"
// #else
#include "x86/aie_trace.h"
// #endif

#include "aie_trace_impl.h"
#include "aie_trace_plugin.h"
#include "aie_trace_metadata.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  bool AieTracePlugin::live = false;

  AieTracePlugin::AieTracePlugin() : XDPPlugin()
  {
    AieTracePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::aie_trace);

    // #ifdef EDGE_BUILD
    // implementation = std::make_unique<AieTrace_EdgeImpl>(db, nullptr);
    // #else
    implementation = std::make_unique<AieTrace_x86Impl>(db, nullptr);
    // #endif
  }

  AieTracePlugin::~AieTracePlugin()
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

    // for(auto h : implementation->deviceHandles) {
    //   xclClose(h);
    // }

    AieTracePlugin::live = false;
  }

  void AieTracePlugin::updateAIEDevice(void* handle)
  {
    if (!handle || !implementation)
      return;

    //Sets up and calls the PS kernel on x86 implementation
    //Sets up and the hardware on the edge implementation
    implementation->updateDevice(handle); 

    // Add all the writers
    DeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(implementation->getDeviceId());
    if (deviceIntf == nullptr) {
    // If DeviceIntf is not already created, create a new one to communicate with physical device
    deviceIntf = new DeviceIntf();
    try {
      deviceIntf->setDevice(new HalDevice(handle));
      deviceIntf->readDebugIPlayout();
    } catch(std::exception& e) {
      // Read debug IP layout could throw an exception
      std::stringstream msg;
      msg << "Unable to read debug IP layout for device " << implementation->getDeviceId() << ": " << e.what();
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      delete deviceIntf;
      return;
    }
    (db->getStaticInfo()).setDeviceIntf(implementation->getDeviceId(), deviceIntf);
    // configure dataflow etc. may not be required here as those are for PL side
    }

    if (implementation->isRuntimeMetrics()) {
      // This code needs to get the actual metric set so we can
      //  configure it.  I'm not sure yet what is needed for this

      uint64_t deviceId = implementation->getDeviceId();

      std::string configFile = "aie_event_runtime_config.json";
      VPWriter* writer = new AieTraceConfigWriter(configFile.c_str(),
                                                  deviceId, implementation->getMetricSet()) ;
      writers.push_back(writer);
      (db->getStaticInfo()).addOpenedFile(writer->getcurrentFileName(), "AIE_EVENT_RUNTIME_CONFIG");

    }

    for (uint64_t n = 0; n < implementation->getNumStreams(); ++n) {
      auto deviceId = implementation->getDeviceId();
	    std::string fileName = "aie_trace_" + std::to_string(deviceId) + "_" + std::to_string(n) + ".txt";
      VPWriter* writer = new AIETraceWriter(fileName.c_str(), deviceId, n,
                                            "", // version
                                            "", // creation time
                                            "", // xrt version
                                            ""); // tool version
      writers.push_back(writer);
      db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(),
                                        "AIE_EVENT_TRACE");

      std::stringstream msg;
      msg << "Creating AIE trace file " << fileName << " for device " << deviceId;
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
    }

    // Ensure trace buffer size is appropriate
    uint64_t aieTraceBufSize = GetTS2MMBufSize(true /*isAIETrace*/);
    bool isPLIO = (db->getStaticInfo()).getNumTracePLIO(implementation->getDeviceId()) ? true : false;

    if (implementation->getContinuousTrace()) {
      // Continuous Trace Offload is supported only for PLIO flow
      if (isPLIO) {
        XDPPlugin::startWriteThread(implementation->getFileDumpIntS(), "AIE_EVENT_TRACE", false);
      } else {
        std::string msg("Continuous offload of AIE Trace is not supported for GMIO mode. So, AIE Trace for GMIO mode will be offloaded only at the end of application.");
        xrt_core::message::send(severity_level::warning, "XRT", msg);
      }
    }

    // First, check against memory bank size
    // NOTE: Check first buffer for PLIO; assume bank 0 for GMIO
    uint8_t memIndex = isPLIO ? deviceIntf->getAIETs2mmMemIndex(0) : 0;
    Memory* memory = (db->getStaticInfo()).getMemory(implementation->getDeviceId(), memIndex);
    if (memory != nullptr) {
      uint64_t fullBankSize = memory->size * 1024;

      if ((fullBankSize > 0) && (aieTraceBufSize > fullBankSize)) {
        aieTraceBufSize = fullBankSize;
        std::string msg = "Requested AIE trace buffer is too big for memory resource. Limiting to " + std::to_string(fullBankSize) + "." ;
        xrt_core::message::send(severity_level::warning, "XRT", msg);
      }
    } 

// Check against amount dedicated as device memory (Edge Linux only)
#if defined (EDGE_BUILD) && ! defined (_WIN32)
    try {
      std::string line;
      std::ifstream ifs;
      ifs.open("/proc/meminfo");
      while (getline(ifs, line)) {
        if (line.find("CmaTotal") == std::string::npos)
          continue;
          
        // Memory sizes are always expressed in kB
        std::vector<std::string> cmaVector;
        boost::split(cmaVector, line, boost::is_any_of(":"));
        auto deviceMemorySize = std::stoull(cmaVector.at(1)) * 1024;
        if (deviceMemorySize == 0)
          break;

        double percentSize = (100.0 * aieTraceBufSize) / deviceMemorySize;
        std::stringstream percentSizeStr;
        percentSizeStr << std::fixed << std::setprecision(3) << percentSize;

        // Limit size of trace buffer if requested amount is too high
        if (percentSize >= 80.0) {
          uint64_t newAieTraceBufSize = (uint64_t)std::ceil(0.8 * deviceMemorySize);
          aieTraceBufSize = newAieTraceBufSize;

          std::stringstream newBufSizeStr;
          newBufSizeStr << std::fixed << std::setprecision(3) << (newAieTraceBufSize / (1024.0 * 1024.0));
          
          std::string msg = "Requested AIE trace buffer is " + percentSizeStr.str() + "% of device memory."
              + " You may run into errors depending upon memory usage of your application."
              + " Limiting to " + newBufSizeStr.str() + " MB.";
          xrt_core::message::send(severity_level::warning, "XRT", msg);
        }
        else {
          std::string msg = "Requested AIE trace buffer is " + percentSizeStr.str() + "% of device memory.";
          xrt_core::message::send(severity_level::info, "XRT", msg);
        }
        
        break;
      }
      ifs.close();
    }
    catch (...) {
        // Do nothing
    }
#endif

    // Create AIE Trace Offloader
    AIETraceDataLogger* aieTraceLogger = new AIETraceDataLogger(implementation->getDeviceId());

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(severity_level::debug)) {
      std::string flowType = (isPLIO) ? "PLIO" : "GMIO";
      std::stringstream msg;
      msg << "Total size of " << std::fixed << std::setprecision(3) << (aieTraceBufSize / (1024.0 * 1024.0))
          << " MB is used for AIE trace buffer for " << std::to_string(implementation->getNumStreams()) << " " << flowType 
          << " streams.";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    }

    AIETraceOffload* aieTraceOffloader = new AIETraceOffload(handle, implementation->getDeviceId(),
                                              deviceIntf, aieTraceLogger,
                                              isPLIO,              // isPLIO?
                                              aieTraceBufSize,     // total trace buffer size
                                              implementation->getNumStreams(),
                                              implementation->isEdge());  // numStream


    // Can't call init without setting important details in offloader
    if (implementation->getContinuousTrace() && isPLIO) {
      aieTraceOffloader->setContinuousTrace();
      aieTraceOffloader->setOffloadIntervalUs(implementation->getOffloadIntervalUs());
    }

    if (!aieTraceOffloader->initReadTrace()) {
      std::string msg = "Allocation of buffer for AIE trace failed. AIE trace will not be available.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      delete aieTraceOffloader;
      delete aieTraceLogger;
      return;
    }
    implementation->aieOffloaders[implementation->getDeviceId()] = std::make_tuple(aieTraceOffloader, aieTraceLogger, deviceIntf);

    // Continuous Trace Offload is supported only for PLIO flow
    if (implementation->getContinuousTrace() && isPLIO)
      aieTraceOffloader->startOffload();
  }

  void AieTracePlugin::flushAIEDevice(void* handle)
  {
    if (implementation)
      implementation->flushDevice(handle);
  }

  void AieTracePlugin::finishFlushAIEDevice(void* handle)
  {
    if (implementation)
      implementation->finishFlushDevice(handle);
  }

  void AieTracePlugin::writeAll(bool openNewFiles)
  {
    for(auto o : implementation->aieOffloaders) {
      auto offloader = std::get<0>(o.second);
      auto logger    = std::get<1>(o.second);

      if (offloader->continuousTrace()) {
        offloader->stopOffload() ;
        while(offloader->getOffloadStatus() != AIEOffloadThreadStatus::STOPPED) ;
      }

      offloader->readTrace(true);
      if (offloader->isTraceBufferFull())
        xrt_core::message::send(severity_level::warning, "XRT", AIE_TS2MM_WARN_MSG_BUF_FULL);
      offloader->endReadTrace();

      delete offloader;
      delete logger;
      // don't delete DeviceIntf
    }
    implementation->aieOffloaders.clear();

    XDPPlugin::endWrite();
  }

  bool AieTracePlugin::alive()
  {
    return AieTracePlugin::live;
  }

} // end namespace xdp
