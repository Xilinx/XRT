/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#include <string>
#include <sstream>
#include <cstring>

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/device_info.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/writer/device_trace/device_trace_writer.h"
#include "xdp/profile/device/device_trace_logger.h"
#include "xdp/profile/device/tracedefs.h"

#include "core/common/config_reader.h"
#include "core/common/message.h"

// Anonymous namespace for helper functions
namespace {

  static bool nonZero(xdp::CounterResults& values)
  {
    // Check AIM stats
    for (uint64_t i = 0 ; i < xdp::MAX_NUM_AIMS ; ++i)
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
    for (uint64_t i = 0 ; i < xdp::MAX_NUM_AMS ; ++i)
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
    for (uint64_t i = 0 ; i < xdp::MAX_NUM_ASMS ; ++i)
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
    XDPPlugin(),
    device_trace(false), continuous_trace(false), trace_buffer_offload_interval_ms(10)
  {
    db->registerPlugin(this) ;

    // Since OpenCL device offload doesn't actually add device offload info,
    //  setting the available information has to be pushed down to both
    //  the HAL or HWEmu plugin

    if (xrt_core::config::get_device_trace() != "off") {
      device_trace = true;
    }

    // Get the profiling continuous offload options from xrt.ini
    //  Device offload continuous offload and dumping is only supported
    //  for hardware, not emulation
    if (getFlowMode() == HW) {
      continuous_trace = xrt_core::config::get_continuous_trace() ;

      trace_buffer_offload_interval_ms =
        xrt_core::config::get_trace_buffer_offload_interval_ms();

      m_enable_circular_buffer = continuous_trace;
    }
    else {
      if (xrt_core::config::get_continuous_trace()) {
        xrt_core::message::send(xrt_core::message::severity_level::warning,
                                "XRT",
                                "Continuous offload and dumping of device data is not supported in emulation and has been disabled.");
      }
    }
  }

  void DeviceOffloadPlugin::addDevice(const std::string& sysfsPath)
  {
    uint64_t deviceId = db->addDevice(sysfsPath) ;

    if (!device_trace)
        return;
    
    // When adding a device, also add a writer to dump the information
    std::string version = "1.1" ;
    std::string creationTime = xdp::getCurrentDateTime() ;
    std::string xrtVersion   = xdp::getXRTVersion() ;
    std::string toolVersion  = xdp::getToolVersion() ;

    std::string filename = 
      "device_trace_" + std::to_string(deviceId) + ".csv" ;

    VPWriter* writer = new DeviceTraceWriter(filename.c_str(),
                                             deviceId,
                                             version,
                                             creationTime,
                                             xrtVersion,
                                             toolVersion);
    writers.push_back(writer);
    (db->getStaticInfo()).addOpenedFile(writer->getcurrentFileName(), "VP_TRACE") ;

    if (continuous_trace)
      XDPPlugin::startWriteThread(XDPPlugin::get_trace_file_dump_int_s(), "VP_TRACE");
  }

  void DeviceOffloadPlugin::configureDataflow(uint64_t deviceId,
                                              DeviceIntf* devInterface)
  {
    uint32_t numAM = devInterface->getNumMonitors(xdp::MonitorType::accel) ;
    bool* dataflowConfig = new bool[numAM] ;
    (db->getStaticInfo()).getDataflowConfiguration(deviceId, dataflowConfig, numAM) ;
    devInterface->configureDataflow(dataflowConfig) ;

    delete [] dataflowConfig ;
  }

  void DeviceOffloadPlugin::configureFa(uint64_t deviceId,
                                        DeviceIntf* devInterface)
  {
    uint32_t numAM = devInterface->getNumMonitors(xdp::MonitorType::accel) ;
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
    uint64_t trace_buffer_size = 0;
    std::vector<uint64_t> buf_sizes;

    if (devInterface->hasTs2mm()) {
      size_t num_ts2mm = devInterface->getNumberTS2MM();
      trace_buffer_size = GetTS2MMBufSize();
      uint64_t each_buffer_size = devInterface->getAlignedTraceBufferSize(trace_buffer_size, static_cast<unsigned int>(num_ts2mm));

      buf_sizes.resize(num_ts2mm, each_buffer_size);
      for(size_t i = 0; i < num_ts2mm; i++) {
        Memory* memory = (db->getStaticInfo()).getMemory(deviceId, devInterface->getTS2MmMemIndex(i));
        if(nullptr == memory) {
          std::string msg = "Information about memory index " + std::to_string(devInterface->getTS2MmMemIndex(i)) 
                             + " not found in given xclbin. So, cannot check availability of memory resource for "
                             + std::to_string(i) +
                             + "th. TS2MM for device trace offload.";
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
          return;
        } else {
          uint64_t memorySz = (memory->size) * 1024 ;
          if (memorySz > 0 && each_buffer_size > memorySz) {
            buf_sizes[i] = memorySz ;
            std::string msg = "Trace buffer size for " + std::to_string(i)
                              + "th. TS2MM is too big for memory resource.  Using " + std::to_string(memorySz) + " instead." ;
            xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg) ;
          }
        }
      }
    }

    DeviceTraceLogger* logger = new DeviceTraceLogger(deviceId) ;

    // We start the thread manually because of race conditions
    DeviceTraceOffload* offloader = 
      new DeviceTraceOffload(devInterface, logger,
                             trace_buffer_offload_interval_ms, // offload_sleep_ms
                             trace_buffer_size);           // trace buffer size

    // If trace is enabled, set up trace.  Otherwise just keep the offloader
    //  for reading the counters.
    if (device_trace) {
      bool init_successful =
        offloader->read_trace_init(m_enable_circular_buffer, buf_sizes) ;

      if (!init_successful) {
        if (devInterface->hasTs2mm()) {
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", TS2MM_WARN_MSG_ALLOC_FAIL) ;
        }
        if (xrt_core::config::get_device_counters()) {
          /* As device_counters is enabled, the offloader object is required for reading counters.
           * So do not delete offlader and logger.
           * As trace infrastructure could not be initialized, disable device_trace to avoid further issue.
           */
          device_trace = false;
        } else {
          delete offloader ;
          delete logger ;
          return ;
        }
      }
    }

    offloaders[deviceId] = std::make_tuple(offloader, logger, devInterface) ;
  }

  void DeviceOffloadPlugin::startContinuousThreads(uint64_t deviceId)
  {
    if (offloaders.find(deviceId) == offloaders.end())
      return ;

    DeviceData& data = offloaders[deviceId] ;
    auto offloader = std::get<0>(data) ;
    auto devInterface = std::get<2>(data) ;
    if (offloader == nullptr)
      return ;

    offloader->train_clock();
    // Trace FIFO is usually very small (8k,16k etc)
    //  Hence enable Continuous clock training/Trace
    //  ONLY for Offload to DDR Memory
    if (!(devInterface->hasTs2mm())) {
      if (continuous_trace) {
        xrt_core::message::send(xrt_core::message::severity_level::warning,
                                "XRT", CONTINUOUS_OFFLOAD_WARN_MSG_FIFO);
      }
      return ;
    }

    // We have TS2MM
    if (continuous_trace) {
      offloader->start_offload(OffloadThreadType::TRACE);
      offloader->set_continuous();
      if (m_enable_circular_buffer) {
        if (devInterface->supportsCircBufPL()) {
          uint64_t min_offload_rate = 0 ;
          uint64_t requested_offload_rate = 0 ;
          bool use_circ_buf =
            offloader->using_circular_buffer(min_offload_rate,
                                             requested_offload_rate);
          if (!use_circ_buf) {
            std::string msg = std::string(TS2MM_WARN_MSG_CIRC_BUF) +
              " Minimum required offload rate (bytes per second) : " +
              std::to_string(min_offload_rate) +
              " Requested offload rate : " +
              std::to_string(requested_offload_rate);
            xrt_core::message::send(xrt_core::message::severity_level::warning,
                                    "XRT", msg);
          }
        }
      }
    }
    else {
      offloader->start_offload(OffloadThreadType::CLOCK_TRAIN);
    }
  }
  
  void DeviceOffloadPlugin::configureTraceIP(DeviceIntf* devInterface)
  {
    // Collect all the profiling options from xrt.ini
    std::string data_transfer_trace = xrt_core::config::get_device_trace() ;
    std::string stall_trace = xrt_core::config::get_stall_trace() ;

    // Set up the hardware trace option
    uint32_t traceOption = 0 ;
    
    // Bit 1: 1 = Coarse mode, 0 = Fine mode
    if (data_transfer_trace == "coarse") {
      if (!devInterface->supportsCoarseModeAIM())
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", COARSE_MODE_UNSUPPORTED);
      else
        traceOption |= 0x1 ;
    }
    
    // Bit 2: 1 = Device trace enabled, 0 = Device trace disabled
    if (data_transfer_trace != "off" && data_transfer_trace != "accel")
      traceOption |= 0x2 ;
    
    // Bit 3: 1 = Pipe stalls enabled, 0 = Pipe stalls disabled
    if (stall_trace == "pipe" || stall_trace == "all")
      traceOption |= 0x4 ;
    
    // Bit 4: 1 = Dataflow stalls enabled, 0 = Dataflow stalls disabled
    if (stall_trace == "dataflow" || stall_trace == "all")
      traceOption |= 0x8;
    
    // Bit 5: 1 = Memory stalls enabled, 0 = Memory stalls disabled
    if (stall_trace == "memory" || stall_trace == "all")
      traceOption |= 0x10 ;

    devInterface->startTrace(traceOption) ;
  }

  void DeviceOffloadPlugin::readCounters()
  {
    for (auto o : offloaders)
    {
      uint64_t deviceId = o.first ;
      xdp::CounterResults results ;
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

  bool DeviceOffloadPlugin::flushTraceOffloader(DeviceTraceOffload* offloader)
  {
    if (!offloader)
      return false;

    try {
      if (offloader->continuous_offload()) {
        offloader->stop_offload() ;
        // To avoid a race condition, wait until the offloader has stopped
        while(offloader->get_status() != OffloadThreadStatus::STOPPED) ;
      }
      else {
        if (device_trace) {
          offloader->read_trace();
          offloader->process_trace();
          offloader->read_trace_end();
        }
      }
    } catch (std::exception& /*e*/) {
        // Reading the trace could throw an exception if ioctls fail.
        return false;
      }

    return true;
  }

  void DeviceOffloadPlugin::writeAll(bool /*openNewFiles*/)
  {
    // This function gets called if the database is destroyed before
    //  the plugin object.  At this time, the information in the database
    //  still exists and is viable, so we should flush our devices
    //  and write our writers.
    for (auto o : offloaders) {
      auto offloader = std::get<0>(o.second) ;
      flushTraceOffloader(offloader);
      checkTraceBufferFullness(offloader, o.first);
    }

    // Also, store away the counter results
    readCounters() ;

    XDPPlugin::endWrite();
  }

  void DeviceOffloadPlugin::checkTraceBufferFullness(DeviceTraceOffload* offloader, uint64_t deviceId)
  {
    if (!(getFlowMode() == HW))
      return;
    if (device_trace) {
      db->getDynamicInfo().setTraceBufferFull(deviceId, offloader->trace_buffer_full());
    }
  }

  void DeviceOffloadPlugin::broadcast(VPDatabase::MessageType msg, void* /*blob*/)
  {
    switch(msg)
    {
    case VPDatabase::READ_COUNTERS:
      {
        readCounters() ;
      }
      break ;
    case VPDatabase::READ_TRACE:
      {
        readTrace() ;
      }
      break ;
    case VPDatabase::DUMP_TRACE:
      {
        XDPPlugin::trySafeWrite("VP_TRACE", true);
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
