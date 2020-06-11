/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include <cstring>

#include "xdp/profile/plugin/hal/hal_plugin.h"
#include "xdp/profile/writer/hal/hal_host_trace_writer.h"
#include "xdp/profile/writer/hal/hal_device_trace_writer.h"
#include "xdp/profile/writer/hal/hal_summary_writer.h"

#include "xdp/profile/writer/vp_base/vp_run_summary.h"

#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/device_trace_offload.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/tracedefs.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/device_event_from_trace.h"
#include "xdp/profile/database/events/creator/device_event_trace_logger.h"

#include "core/common/xrt_profiling.h"
#include "core/common/message.h"

#define MAX_PATH_SZ 512

namespace xdp {

  HALPlugin::HALPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;

    std::string version = "1.0" ;

    std::string creationTime = xdp::getCurrentDateTime() ;
    std::string xrtVersion   = xdp::getXRTVersion() ;
    std::string toolVersion  = xdp::getToolVersion() ;

    // Based upon the configuration, create the appropriate writers
    writers.push_back(new HALHostTraceWriter("hal_host_trace.csv",
					     version,
					     creationTime,
					     xrtVersion,
                         toolVersion)) ;
    (db->getStaticInfo()).addOpenedFile("hal_host_trace.csv", "VP_TRACE");
#ifdef HAL_SUMMARY
    writers.push_back(new HALSummaryWriter("hal_summary.csv"));
#endif

    // There should be both a writer for each device.
    unsigned int index = 0 ;
    void* handle = xclOpen(index, "/dev/null", XCL_INFO) ;

    char  pathBuf[MAX_PATH_SZ];
    while (handle != nullptr)
    {
      memset(pathBuf, 0, MAX_PATH_SZ);
      xclGetDebugIPlayoutPath(handle, pathBuf, MAX_PATH_SZ);

      std::string sysfsPath(pathBuf);
      uint64_t deviceId = db->addDevice(sysfsPath);

      deviceHandles[deviceId] = handle;

      std::string fileName = "hal_device_trace_" + std::to_string(deviceId) + ".csv" ;
      writers.push_back(new HALDeviceTraceWriter(fileName.c_str(), deviceId,
						 version,
						 creationTime,
						 xrtVersion,
                         toolVersion));
      (db->getStaticInfo()).addOpenedFile(fileName.c_str(), "VP_TRACE");
      ++index;
      handle = xclOpen(index, "/dev/null", XCL_INFO) ;			
    }
    writers.push_back(new VPRunSummaryWriter("hal.run_summary"));
  }

  HALPlugin::~HALPlugin()
  {
    if (VPDatabase::alive())
    {
      // We were destroyed before the database, so flush our events to the 
      //  database, write the writers, and unregister ourselves from
      //  the database.
      writeAll(false) ;
      db->unregisterPlugin(this) ;
    }
    // If the database is dead, then we must have already forced a 
    //  write at the database destructor so we can just move on

    // clear all the members
    for(auto itr : devices) {
      delete itr.second;
    }
    devices.clear();
    for(auto itr : deviceTraceOffloaders) {
      delete itr.second;
    }
    deviceTraceOffloaders.clear();
    for(auto itr : deviceTraceLoggers) {
      delete itr.second;
    }
    deviceTraceLoggers.clear();

    for(auto itr : deviceHandles) {
      xclClose(itr.second);
    }
    deviceHandles.clear();
    devHandleIdMap.clear();
  }

  uint64_t HALPlugin::getDeviceId(void* handle)
  {
    if(devHandleIdMap.find(handle) != devHandleIdMap.end()) {
      return devHandleIdMap[handle];
    }

    char pathBuf[MAX_PATH_SZ];
    xclGetDebugIPlayoutPath(handle, pathBuf, MAX_PATH_SZ);

    std::string sysfsPath(pathBuf);
    uint64_t uniqDevId = db->addDevice(sysfsPath);

    // save to improve performance, as xclGetDebugIPlayoutPath is time consuming
    devHandleIdMap[handle] = uniqDevId;
    return uniqDevId;
  }

  void HALPlugin::updateDevice(void* handle, const void* /*binary*/)
  {
    if(handle == nullptr)  return;

    uint64_t deviceId = getDeviceId(handle);

    if(deviceHandles.find(deviceId) == deviceHandles.end()) return;

    (db->getStaticInfo()).updateDevice(deviceId, handle);

    {
      struct xclDeviceInfo2* info = new xclDeviceInfo2;
      if(xclGetDeviceInfo2(handle, info) == 0) {
        (db->getStaticInfo()).setDeviceName(deviceId, std::string(info->mName));
      }
      delete info;
    }

    resetDevice(deviceId);

    // Update DeviceIntf in HALPlugin
    DeviceIntf* devInterface = new DeviceIntf();
    devInterface->setDevice(new HalDevice(deviceHandles[deviceId]));
    devices[deviceId] = devInterface;

    devInterface->readDebugIPlayout();
    devInterface->startCounters();

    uint32_t numAM = devInterface->getNumMonitors(XCL_PERF_MON_ACCEL);
    bool* dataflowConfig = new bool[numAM];
    db->getStaticInfo().getDataflowConfiguration(deviceId, dataflowConfig, numAM);
    devInterface->configureDataflow(dataflowConfig);
    delete [] dataflowConfig;

    devInterface->startTrace(2); /* data_transfer_trace=fine, by default */
    devInterface->clockTraining();

    bool init_done = true;

    uint64_t traceBufSz = 0;
    if(devInterface->hasTs2mm()) {
      // Get Trace Buffer Size in bytes
      traceBufSz = GetTS2MMBufSize();
      uint64_t memorySz = (db->getStaticInfo().getMemory(deviceId, devInterface->getTS2MmMemIndex())->size) * 1024;
      if (memorySz > 0 && traceBufSz > memorySz) {
        traceBufSz = memorySz;
        std::string msg = "Trace Buffer size is too big for Memory Resource. Using " + std::to_string(memorySz)
                          + " Bytes instead.";
        xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", msg);
      }
    }

    DeviceTraceLogger*  deviceTraceLogger    = new TraceLoggerCreatingDeviceEvents(deviceId);
    DeviceTraceOffload* deviceTraceOffloader = new DeviceTraceOffload(devInterface, deviceTraceLogger, 10, traceBufSz, false);
    init_done = deviceTraceOffloader->read_trace_init();
    if (init_done) {
      deviceTraceLoggers[deviceId]    = deviceTraceLogger;
      deviceTraceOffloaders[deviceId] = deviceTraceOffloader;
    } else {
      if (devInterface->hasTs2mm()) {
        xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_ALLOC_FAIL);
      }
      delete deviceTraceLogger;
      delete deviceTraceOffloader;
    }
  }

  void HALPlugin::resetDevice(uint64_t deviceId)
  {
    // Reset DeviceIntf, DeviceTraceLogger, DeviceTraceOffloader
    if(devices.find(deviceId) != devices.end()) {
      delete devices[deviceId];
      devices[deviceId] = nullptr;
    }
    if(deviceTraceOffloaders.find(deviceId) != deviceTraceOffloaders.end()) {
      delete deviceTraceOffloaders[deviceId];
      deviceTraceOffloaders[deviceId] = nullptr;
    }
    if(deviceTraceLoggers.find(deviceId) != deviceTraceLoggers.end()) {
      delete deviceTraceLoggers[deviceId];
      deviceTraceLoggers[deviceId] = nullptr;
    }
  }

  void HALPlugin::writeAll(bool openNewFiles)
  {
    flushDevices() ;
    for (auto w : writers)
    {
      w->write(openNewFiles) ;
    }
  }

  void HALPlugin::readDeviceInfo(void* handle)
  {
    if (handle == nullptr) return ;

    uint64_t deviceId = getDeviceId(handle);

    auto itr = devices.find(deviceId);
    if(itr == devices.end()) {
      return;
    }

    DeviceIntf* devInterface = itr->second;
   
    // Debug IP Layout must have been read earlier, but still double check for now
    devInterface->readDebugIPlayout();

    xclCounterResults counters ;
    devInterface->readCounters(counters) ;
    (db->getStats()).updateCounters(deviceId, counters) ;

    // Next, read trace and update the dynamic database with appropriate events
    DeviceTraceOffload* deviceTraceOffloader = deviceTraceOffloaders[deviceId];
    if(!deviceTraceOffloader) {
      return;
    }
    deviceTraceOffloader->read_trace();
  }

  void HALPlugin::flushDeviceInfo(void* handle)
  {
    if (handle == nullptr) return ;

    uint64_t deviceId = getDeviceId(handle);

    auto itr = devices.find(deviceId);
    if(itr == devices.end()) {
      return;
    }
    
    // The void* passed in to this function is a low level xclDeviceHandle
    for (auto w : writers)
    {
      if (w->isDeviceWriter() && w->isSameDevice(handle))
      {
	      w->write(true) ;
      }
    }
  }

  // This function should be started in a separate thread
  void HALPlugin::flushDevices()
  {
    for (auto itr : devices)
    {
      uint64_t deviceId = itr.first;
      DeviceIntf* devInterface = itr.second;

      // Debug IP Layout should have been read but double check for now
      devInterface->readDebugIPlayout() ;

      xclCounterResults counters ;
      devInterface->readCounters(counters) ;
      (db->getStats()).updateCounters(counters) ;

      // Next, read trace and update the dynamic database with appropriate events
      DeviceTraceOffload* deviceTraceOffloader = deviceTraceOffloaders[deviceId];
      if(!deviceTraceOffloader) {
        continue;
      }
      deviceTraceOffloader->read_trace();
      deviceTraceOffloader->read_trace_end();
    }
  }

  // This function should be started in a separate thread
  void HALPlugin::continuousOffload()
  {
    for (auto w : writers)
    {
	//if (w->isDeviceWriter()) w->readDevice() ;
	    w->write(true);
    }
  }

}


