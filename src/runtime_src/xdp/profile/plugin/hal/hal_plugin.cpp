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

#include <cstring>

#include "hal_plugin.h"
#include "xdp/profile/writer/hal/hal_host_trace_writer.h"
#include "xdp/profile/writer/hal/hal_device_trace_writer.h"
#include "xdp/profile/writer/hal/hal_summary_writer.h"

#include "xdp/profile/writer/util.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/database/database.h"

#include "core/common/xrt_profiling.h"

#define MAX_PATH_SZ 512

namespace xdp {

  HALPlugin::HALPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;

    std::string version = "1.0" ;

    std::string creationTime = xdp::WriterI::getCurrentDateTime() ;
    std::string XRTVersion = xdp::WriterI::getToolVersion() ;

    // Based upon the configuration, create the appropriate writers
    writers.push_back(new HALHostTraceWriter("hal_host_trace.csv",
					     version,
					     //pid,
					     creationTime,
					     XRTVersion)) ;
    writers.push_back(new HALSummaryWriter("hal_summary.csv")) ;

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

      std::string fileName = "hal_device_trace_" + std::to_string(deviceId) + ".csv" ;
      writers.push_back(new HALDeviceTraceWriter(fileName.c_str(), deviceId,
						 version,
						 //pid,
						 creationTime,
						 XRTVersion, nullptr));
      ++index;
      handle = xclOpen(index, "/dev/null", XCL_INFO) ;			
    }
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
  }

  uint64_t HALPlugin::getDeviceId(void* handle)
  {
    char pathBuf[MAX_PATH_SZ];
    xclGetDebugIPlayoutPath(handle, pathBuf, MAX_PATH_SZ);

    std::string sysfsPath(pathBuf);
    return db->addDevice(sysfsPath);
  }

  void HALPlugin::updateDevice(void* handle, const void* binary)
  {
    if(handle == nullptr)  return;

    uint64_t deviceId = getDeviceId(handle);

    (db->getStaticInfo()).updateDevice(deviceId, binary);


    struct xclDeviceInfo2* info = new xclDeviceInfo2;
    xclGetDeviceInfo2(handle, info);
    (db->getStaticInfo()).setDeviceName(deviceId, std::string(info->mName));

    // Update DeviceIntf in HALPlugin
    if(devices.find(deviceId) != devices.end()) {
      delete devices[deviceId];
      devices[deviceId] = nullptr;
    }
    DeviceIntf* devInterface = new DeviceIntf();
    devInterface->setDevice(new HalDevice(handle));
    devices[deviceId] = devInterface;

    devInterface->readDebugIPlayout();
    devInterface->startCounters();
    devInterface->startTrace(3);
  }

  void HALPlugin::setEncounteredDeviceHandle(void* handle)
  {
    encounteredHandles.emplace(handle) ;
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
    xclTraceResultsVector trace ;
    if (devInterface->hasFIFO())
    {
      devInterface->readTrace(trace) ;
    }
    else if (devInterface->hasTs2mm())
    {
      // TODO: Sync the data and parse it.
      /*
      void* hostBuffer = nullptr ; // Need to sync the data
      devInterface->parseTraceData(hostBuffer, devInterface->getWordCountTs2mm(), trace) ;
      */
    }
    (db->getDynamicInfo()).addDeviceEvents(deviceId, trace, db);
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
      
      // Next, read trace and update the dynamic database with
      //  appropriate events
      xclTraceResultsVector trace ;
      if (devInterface->hasFIFO())
      {
	      devInterface->readTrace(trace) ;
      }
      else if (devInterface->hasTs2mm())
      {
	      void* hostBuffer = nullptr ; // Need to sync the data
	      devInterface->parseTraceData(hostBuffer, devInterface->getWordCountTs2mm(), trace) ;
      }
      (db->getDynamicInfo()).addDeviceEvents(deviceId, trace, db) ;
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
//
