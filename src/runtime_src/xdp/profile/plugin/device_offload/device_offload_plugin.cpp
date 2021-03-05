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
#include <cstring>

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/device_trace/device_trace_writer.h"
#include "xdp/profile/database/events/creator/device_event_trace_logger.h"

#include "core/common/config_reader.h"
#include "core/common/message.h"

// Anonymous namespace for helper functions
namespace {

  static bool nonZero(xclCounterResults& values)
  {
    // Check AIM stats
    for (uint64_t i = 0 ; i < XAIM_MAX_NUMBER_SLOTS ; ++i)
    {
      if (values.WriteBytes[i]      != 0) return true ;
      if (values.WriteTranx[i]      != 0) return true ;
      if (values.WriteLatency[i]    != 0) return true ;
      if (values.WriteMinLatency[i] != 0) return true ;
      if (values.WriteMaxLatency[i] != 0) return true ;
      if (values.ReadBytes[i]       != 0) return true ;
      if (values.ReadTranx[i]       != 0) return true ;
      if (values.ReadLatency[i]     != 0) return true ;
      if (values.ReadMinLatency[i]  != 0) return true ;
      if (values.ReadMaxLatency[i]  != 0) return true ;
      if (values.ReadBusyCycles[i]  != 0) return true ;
      if (values.WriteBusyCycles[i] != 0) return true ;
    }

    // Check AM stats
    for (uint64_t i = 0 ; i < XAM_MAX_NUMBER_SLOTS ; ++i)
    {
      if (values.CuExecCount[i]       != 0) return true ;
      if (values.CuExecCycles[i]      != 0) return true ;
      if (values.CuBusyCycles[i]      != 0) return true ;
      if (values.CuMaxParallelIter[i] != 0) return true ;
      if (values.CuStallExtCycles[i]  != 0) return true ;
      if (values.CuStallStrCycles[i]  != 0) return true ;
      if (values.CuMinExecCycles[i]   != 0) return true ;
      if (values.CuMaxExecCycles[i]   != 0) return true ;
    }

    // Check ASM stats
    for (uint64_t i = 0 ; i < XASM_MAX_NUMBER_SLOTS ; ++i)
    {
      if (values.StrNumTranx[i]     != 0) return true ;
      if (values.StrDataBytes[i]    != 0) return true ;
      if (values.StrBusyCycles[i]   != 0) return true ;
      if (values.StrStallCycles[i]  != 0) return true ;
      if (values.StrStarveCycles[i] != 0) return true ;
    }

    return false ;
  }

} // end anonymous namespace

namespace xdp {

  DeviceOffloadPlugin::DeviceOffloadPlugin() :
    XDPPlugin(), continuous_trace(false), continuous_trace_interval_ms(10), trace_dump_int_s(5)
  {
    active = db->claimDeviceOffloadOwnership() ;
    if (!active) return ; 

    db->registerPlugin(this) ;

    // Get the profiling continuous offload options from xrt.ini    
    continuous_trace = xrt_core::config::get_continuous_trace() ;
    continuous_trace_interval_ms = 
      xrt_core::config::get_continuous_trace_interval_ms() ;
    trace_dump_int_s =
      xrt_core::config::get_trace_dump_interval_s();
  }

  DeviceOffloadPlugin::~DeviceOffloadPlugin()
  {
  }

  void DeviceOffloadPlugin::addDevice(const std::string& sysfsPath)
  {
    if (!active) return ;

    uint64_t deviceId = db->addDevice(sysfsPath) ;

    // When adding a device, also add a writer to dump the information
    std::string version = "1.1" ;
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

    if (continuous_trace) {
      XDPPlugin::startWriteThread(trace_dump_int_s, "VP_TRACE");
    }
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
      Memory* memory = (db->getStaticInfo()).getMemory(deviceId, devInterface->getTS2MmMemIndex());
      if(nullptr == memory) {
        std::string msg = "Information about memory index " + std::to_string(devInterface->getTS2MmMemIndex()) 
                           + " not found in given xclbin. So, cannot check availability of memory resource for device trace offload.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        return;
      } else {
        uint64_t memorySz = (memory->size) * 1024 ;
        if (memorySz > 0 && trace_buffer_size > memorySz) {
          trace_buffer_size = memorySz ;
          std::string msg = "Trace buffer size is too big for memory resource.  Using " + std::to_string(memorySz) + " instead." ;
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg) ;
        }
      }
    }

    TraceLoggerCreatingDeviceEvents* logger = 
      new TraceLoggerCreatingDeviceEvents(deviceId) ;

    bool enable_device_trace = xrt_core::config::get_timeline_trace() ||
      xrt_core::config::get_data_transfer_trace() != "off" ;

    DeviceTraceOffload* offloader = 
      new DeviceTraceOffload(devInterface, logger,
                             continuous_trace_interval_ms, // offload_sleep_ms,
                             trace_buffer_size,            // trbuf_size,
                             continuous_trace,             // start_thread
                             enable_device_trace);

    bool init_successful = offloader->read_trace_init() ;
    if (!init_successful) {
      if (devInterface->hasTs2mm()) {
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_ALLOC_FAIL) ;
      }
      delete offloader ;
      delete logger ;
      return ;
    }
    offloaders[deviceId] = std::make_tuple(offloader, logger, devInterface) ;
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
    if (data_transfer_trace != "off" && data_transfer_trace != "accel")    traceOption |= 0x2 ;
    
    // Bit 3: 1 = Pipe stalls enabled, 0 = Pipe stalls disabled
    if (stall_trace == "pipe" || stall_trace == "all") traceOption |= 0x4 ;
    
    // Bit 4: 1 = Dataflow stalls enabled, 0 = Dataflow stalls disabled
    if (stall_trace == "dataflow" || stall_trace == "all") traceOption |= 0x8;
    
    // Bit 5: 1 = Memory stalls enabled, 0 = Memory stalls disabled
    if (stall_trace == "memory" || stall_trace == "all") traceOption |= 0x10 ;

    devInterface->startTrace(traceOption) ;
  }

  void DeviceOffloadPlugin::readCounters()
  {
    for (auto o : offloaders)
    {
      uint64_t deviceId = o.first ;
      xclCounterResults results ;
      std::get<2>(o.second)->readCounters(results) ;

      // Only store this in the dynamic database if there is valid data.
      //  In the case of hardware emulation the simulation could have exited
      //  and we are reading nothing but 0's
      if (nonZero(results))
      {
        DeviceInfo* deviceInfo = (db->getStaticInfo()).getDeviceInfo(deviceId);
        if (deviceInfo != nullptr) {
          (db->getDynamicInfo()).setCounterResults(deviceId, deviceInfo->currentXclbinUUID(), results) ;
        }
      }
    }
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
      auto offloader = std::get<0>(o.second) ;
      if (offloader->continuous_offload())
      {
        offloader->stop_offload() ;
      }
      else
      {
        offloader->read_trace() ;
      }
    }

    // Also, store away the counter results
    readCounters() ;

    XDPPlugin::endWrite(openNewFiles);
  }

  void DeviceOffloadPlugin::broadcast(VPDatabase::MessageType msg, void* /*blob*/)
  {
    if (!active) return ;

    switch(msg)
    {
    case VPDatabase::READ_COUNTERS:
      {
        readCounters() ;
      }
      break ;
    default:
      break ;
    }
  }

  void DeviceOffloadPlugin::clearOffloader(uint64_t deviceId)
  {
    if(offloaders.find(deviceId) == offloaders.end()) {
      return;
    }
    auto entry = offloaders[deviceId];
    auto offloader = std::get<0>(entry);
    auto logger    = std::get<1>(entry);

    delete offloader;
    delete logger;

    offloaders.erase(deviceId);
  }

  void DeviceOffloadPlugin::clearOffloaders()
  {
    for(auto entry : offloaders) {
      auto offloader = std::get<0>(entry.second);
      auto logger    = std::get<1>(entry.second);

      delete offloader;
      delete logger;
    }
    offloaders.clear();
  }

} // end namespace xdp
