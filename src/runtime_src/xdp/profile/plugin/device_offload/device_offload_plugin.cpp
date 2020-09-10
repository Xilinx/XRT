/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include <string>
#include <sstream>

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/device_trace/device_trace_writer.h"
#include "xdp/profile/database/events/creator/device_event_trace_logger.h"
#if 0
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#endif
#include "core/common/config_reader.h"
#include "core/common/message.h"

namespace xdp {

  DeviceOffloadPlugin::DeviceOffloadPlugin() : XDPPlugin()
  {
    active = db->claimDeviceOffloadOwnership() ;
    if (!active) return ; 

    db->registerPlugin(this) ;

    // Get the profiling continuous offload options from xrt.ini    
    continuous_trace = xrt_core::config::get_continuous_trace() ;
    continuous_trace_interval_ms = 
      xrt_core::config::get_continuous_trace_interval_ms() ;
  }

  DeviceOffloadPlugin::~DeviceOffloadPlugin()
  {
  }

  void DeviceOffloadPlugin::addDevice(const std::string& sysfsPath)
  {
    if (!active) return ;

    uint64_t deviceId = db->addDevice(sysfsPath) ;

    // When adding a device, also add a writer to dump the information
    std::string version = "1.0" ;
    std::string creationTime = xdp::getCurrentDateTime() ;
    std::string xrtVersion   = xdp::getXRTVersion() ;
    std::string toolVersion  = xdp::getToolVersion() ;

    std::string filename = 
      "device_trace_" + std::to_string(deviceId) + ".csv" ;
      
    writers.push_back(new DeviceTraceWriter(filename.c_str(),
					    deviceId,
					    version,
					    creationTime,
					    xrtVersion,
					    toolVersion)) ;

    (db->getStaticInfo()).addOpenedFile(filename.c_str(), "VP_TRACE") ;
  }

  void DeviceOffloadPlugin::configureDataflow(uint64_t deviceId,
					      DeviceIntf* devInterface)
  {
    uint32_t numAM = devInterface->getNumMonitors(XCL_PERF_MON_ACCEL) ;
    bool* dataflowConfig = new bool[numAM] ;
    (db->getStaticInfo()).getDataflowConfiguration(deviceId, dataflowConfig, numAM) ;
    devInterface->configureDataflow(dataflowConfig) ;

    delete [] dataflowConfig ;
  }

  void DeviceOffloadPlugin::configureFa(uint64_t deviceId,
					      DeviceIntf* devInterface)
  {
    uint32_t numAM = devInterface->getNumMonitors(XCL_PERF_MON_ACCEL) ;
    bool* FaConfig = new bool[numAM] ;
    (db->getStaticInfo()).getFaConfiguration(deviceId, FaConfig, numAM) ;
    devInterface->configureFa(FaConfig) ;

    delete [] FaConfig ;
  }

  void DeviceOffloadPlugin::configureCtx(uint64_t deviceId,
					      DeviceIntf* devInterface)
  {
    auto ctxInfo = (db->getStaticInfo()).getCtxInfo(deviceId) ;
    devInterface->configAmContext(ctxInfo);
  }

  // It is the responsibility of the child class to instantiate the appropriate
  //  device interface based on the level (OpenCL or HAL)
  void DeviceOffloadPlugin::addOffloader(uint64_t deviceId,
					 DeviceIntf* devInterface)
  {
    if (!active) return ;

    // If offload via memory is requested, make sure the size requested
    //  fits inside the chosen memory resource.
    uint64_t trace_buffer_size = GetTS2MMBufSize() ;
    if (devInterface->hasTs2mm()) {
      uint64_t memorySz = ((db->getStaticInfo()).getMemory(deviceId, devInterface->getTS2MmMemIndex())->size) * 1024 ;
      if (memorySz > 0 && trace_buffer_size > memorySz) {
        trace_buffer_size = memorySz ;
        std::string msg = "Trace buffer size is too big for memory resource.  Using " + std::to_string(memorySz) + " instead." ;
        xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", msg) ;
      }
    }

    TraceLoggerCreatingDeviceEvents* logger = 
      new TraceLoggerCreatingDeviceEvents(deviceId) ;

    DeviceTraceOffload* offloader = 
      new DeviceTraceOffload(devInterface, logger,
                         continuous_trace_interval_ms, // offload_sleep_ms,
                         trace_buffer_size,            // trbuf_size,
                         continuous_trace);            // start_thread

    bool init_successful = offloader->read_trace_init() ;
    if (!init_successful) {
      if (devInterface->hasTs2mm()) {
        xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_ALLOC_FAIL) ;
      }
      delete offloader ;
      delete logger ;
      return ;
    }
    offloaders[deviceId] = std::make_tuple(offloader, logger, devInterface) ;
#if 0
    if((db->getStaticInfo()).getNumAIETraceStream(deviceId)) {

      uint64_t aieTraceBufSz = GetTS2MMBufSize(true /*isAIETrace*/);
      // check only one memory ? : GMIO ?
      uint64_t aieMemorySz = ((db->getStaticInfo()).getMemory(deviceId, devInterface->getAIETs2mmMemIndex(0))->size) * 1024 ;
      if(aieMemorySz > 0 && aieTraceBufSz > aieMemorySz) {
        aieTraceBufSz = aieMemorySz;
        std::string msg = "Trace buffer size is too big for memory resource.  Using " + std::to_string(aieMemorySz) + " instead." ;
        xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", msg);
      }
      bool isPLIO = ((db->getStaticInfo()).getNumTracePLIO(deviceId)) ? true : false;
      AIETraceDataLogger* aieTraceLogger = new AIETraceDataLogger(deviceId);

      AIETraceOffload* aieTraceOffloader = new AIETraceOffload(devInterface, aieTraceLogger,
                                                isPLIO,          // isPLIO 
                                                aieTraceBufSz,   // total trace buffer size
                                                (db->getStaticInfo()).getNumAIETraceStream(deviceId));  // numStream

      if(!aieTraceOffloader->initReadTrace()) {
        xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_ALLOC_FAIL) ; // FOR AIE ?
        delete aieTraceOffloader;
        delete aieTraceLogger;
        return;
      }
      aieOffloaders[deviceId] = std::make_tuple(aieTraceOffloader, aieTraceLogger, devInterface) ;
    }
#endif
  }
  
  void DeviceOffloadPlugin::configureTraceIP(DeviceIntf* devInterface)
  {
    // Collect all the profiling options from xrt.ini
    std::string data_transfer_trace = 
      xrt_core::config::get_data_transfer_trace() ;
    std::string stall_trace = xrt_core::config::get_stall_trace() ;

    // Set up the hardware trace option
    uint32_t traceOption = 0 ;
    
    // Bit 1: 1 = Coarse mode, 0 = Fine mode 
    if (data_transfer_trace == "coarse") traceOption |= 0x1 ;
    
    // Bit 2: 1 = Device trace enabled, 0 = Device trace disabled
    if (data_transfer_trace != "off")    traceOption |= 0x2 ;
    
    // Bit 3: 1 = Pipe stalls enabled, 0 = Pipe stalls disabled
    if (stall_trace == "pipe" || stall_trace == "all") traceOption |= 0x4 ;
    
    // Bit 4: 1 = Dataflow stalls enabled, 0 = Dataflow stalls disabled
    if (stall_trace == "dataflow" || stall_trace == "all") traceOption |= 0x8;
    
    // Bit 5: 1 = Memory stalls enabled, 0 = Memory stalls disabled
    if (stall_trace == "memory" || stall_trace == "all") traceOption |= 0x10 ;

    devInterface->startTrace(traceOption) ;
  }

  void DeviceOffloadPlugin::writeAll(bool openNewFiles)
  {
    if (!active) return ;

    // This function gets called if the database is destroyed before
    //  the plugin object.  At this time, the information in the database
    //  still exists and is viable, so we should flush our devices
    //  and write our writers.
    for (auto o : offloaders)
    {
      (std::get<0>(o.second))->read_trace() ;
    }    
#if 0
    for (auto o : aieOffloaders)
    {
      (std::get<0>(o.second))->readTrace() ;
    }
#endif    

    for (auto w : writers)
    {
      w->write(openNewFiles) ;
    }
  }

  void DeviceOffloadPlugin::clearOffloader(uint32_t deviceId)
  {
#if 0
    if(aieOffloaders.find(deviceId) != aieOffloaders.end()) {
      auto entry = aieOffloaders[deviceId];
      auto offloader = std::get<0>(entry);
      auto logger    = std::get<1>(entry);

      delete offloader;
      delete logger;
    }
#endif

    if(offloaders.find(deviceId) == offloaders.end()) {
      return;
    }
    auto entry = offloaders[deviceId];
    auto offloader = std::get<0>(entry);
    auto logger    = std::get<1>(entry);
    auto intf      = std::get<2>(entry);

    delete offloader;
    delete logger;
    delete intf;
  }

  void DeviceOffloadPlugin::clearOffloaders()
  {
#if 0
    for(auto entry : aieOffloaders) {
      auto offloader = std::get<0>(entry.second);
      auto logger    = std::get<1>(entry.second);

      delete offloader;
      delete logger;
    }
#endif
    for(auto entry : offloaders) {
      auto offloader = std::get<0>(entry.second);
      auto logger    = std::get<1>(entry.second);
      auto intf      = std::get<2>(entry.second);

      delete offloader;
      delete logger;
      delete intf;
    }
  }
  
} // end namespace xdp
