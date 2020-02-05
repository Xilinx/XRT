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

// For counting devices
#include "xclhal2.h"

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

    // There should be both a Device Interface and a writer for
    //  each device.
    unsigned int deviceIndex = 0 ;
    void* handle = xclOpen(deviceIndex, "/dev/null", XCL_INFO) ;
    while (handle != nullptr)
    {
      std::string fileName = 
	"hal_device_trace_" + std::to_string(deviceIndex) + ".csv" ;
      
      DeviceIntf* nextInterface = new DeviceIntf() ;
      nextInterface->setDevice(new HalDevice(handle)) ;

      writers.push_back(new HALDeviceTraceWriter(fileName.c_str(),
						 version,
						 //pid,
						 creationTime,
						 XRTVersion,
						 nextInterface)) ;
      devices.push_back(nextInterface) ;
      ++deviceIndex ;
      handle = xclOpen(deviceIndex, "/dev/null", XCL_INFO) ;			
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

    // The void* coming in is an xclDeviceHandle.  It won't be the same
    //  pointer as the device interfaces we have stored, so we will
    //  need some way to find the underlying device by comparing 
    //  xclDeviceHandles
    /*
      DeviceIntf* interface = nullptr ;
      for (auto d : devices)
      {
        if (d->getAbstractDevice()->getRawDevice() == handle)
        {
          interface = d ;
          break ;
        }
      }
      if (interface == nullptr) return ; // We are not monitoring this device
     */

    // Only read the device info if we are reloading a device, not loading
    //  it the first time.
    if (encounteredHandles.find(handle) == encounteredHandles.end()) return ;

    DeviceIntf* devInterface = new DeviceIntf() ;
    devInterface->setDevice(new HalDevice(handle)) ;
    devInterface->readDebugIPlayout() ;
    xclCounterResults counters ;
    devInterface->readCounters(XCL_PERF_MON_MEMORY, counters) ;
    (db->getStats()).updateCounters(devInterface, counters) ;

    // Next, read trace and update the dynamic database with appropriate events
    xclTraceResultsVector trace ;
    if (devInterface->hasFIFO())
    {
      devInterface->readTrace(XCL_PERF_MON_MEMORY, trace) ;
    }
    else if (devInterface->hasTs2mm())
    {
      // TODO: Sync the data and parse it.
      /*
      void* hostBuffer = nullptr ; // Need to sync the data
      devInterface->parseTraceData(hostBuffer, 
				   devInterface->getWordCountTs2mm(),
				   trace) ;
      */
    }
    (db->getDynamicInfo()).addDeviceEvents(devInterface, trace) ;
    delete devInterface ;
  }

  void HALPlugin::flushDeviceInfo(void* handle)
  {
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
    for (auto devInterface : devices)
    {
      devInterface->readDebugIPlayout() ;
      xclCounterResults counters ;
      devInterface->readCounters(XCL_PERF_MON_MEMORY, counters) ;
      (db->getStats()).updateCounters(counters) ;
      
      // Next, read trace and update the dynamic database with
      //  appropriate events
      xclTraceResultsVector trace ;
      if (devInterface->hasFIFO())
      {
	devInterface->readTrace(XCL_PERF_MON_MEMORY, trace) ;
      }
      else if (devInterface->hasTs2mm())
      {
	void* hostBuffer = nullptr ; // Need to sync the data
	devInterface->parseTraceData(hostBuffer, 
				     devInterface->getWordCountTs2mm(),
				     trace) ;
      }
      (db->getDynamicInfo()).addDeviceEvents(devInterface, trace) ;
    }
  }

  // This function should be started in a separate thread
  void HALPlugin::continuousOffload()
  {
    {
      for (auto w : writers)
      {
	//if (w->isDeviceWriter()) w->readDevice() ;
	w->write(true) ;
      }
    }
  }

}
