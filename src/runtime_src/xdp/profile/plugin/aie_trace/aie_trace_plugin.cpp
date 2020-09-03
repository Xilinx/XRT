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

#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"

#include "core/common/xrt_profiling.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/device/device_intf.h"
//#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"


#if 0
//#include <string>
//#include <vector>
//#include <iostream>

// For HAL applications
#include "core/common/xrt_profiling.h"

#include "xdp/profile/writer/device_trace/device_trace_writer.h"

#include "xdp/profile/plugin/device_offload/hal/hal_device_offload_plugin.h"
#include "xdp/profile/database/database.h"

#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/database/events/creator/device_event_trace_logger.h"

#include "core/common/system.h"
#endif



namespace xdp {

  AieTracePlugin::AieTracePlugin()
                : XDPPlugin()
  {
    db->registerPlugin(this);

    // Open all the devices to store the handles
    uint32_t index = 0;
    void* handle = xclOpen(index, "/dev/null", XCL_INFO);

    while(nullptr != handle) {
      deviceHandles.push_back(handle);

      // Use sysfs path for debug_ip_layout to find the unique Device ID
      char pathBuf[512];
      memset(pathBuf, 0, 512);
      xclGetDebugIPlayoutPath(handle, pathBuf, 512);

      std::string sysfsPath(pathBuf);

      uint64_t deviceId = db->addDevice(sysfsPath);

      deviceIdToHandle[deviceId] = handle;

      // Move to the next device
      ++index;
      handle = xclOpen(index, "/dev/null", XCL_INFO);
    }
  }

  AieTracePlugin::~AieTracePlugin()
  {
    if(VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch(...) {
      }
      db->unregisterPlugin(this);
    }

    // If the database is dead, then we must have already forced a 
    //  write at the database destructor so we can just move on

    for(auto h : deviceHandles) {
      xclClose(h);
    }
  }

  void AieTracePlugin::updateAIEDevice(void* handle)
  {
    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);

    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id
    void* ownedHandle = deviceIdToHandle[deviceId];

    if(!(db->getStaticInfo()).isDeviceReady(deviceId)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceId, userHandle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(userHandle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
        }
      }
    }

    uint64_t numAIETraceOutput = (db->getStaticInfo()).getNumAIETraceStream(deviceId);
    if(0 == numAIETraceOutput) {
      // no AIE Trace Stream to offload trace, so return
      return;
    }

    void* dIntf = (db->getStaticInfo())->getDeviceIntf(deviceId);
    DeviceIntf* deviceIntf = dynamic_cast<DeviceIntf*>(reinterpret_cast<DeviceIntf*>(dIntf));
    if(nullptr == deviceIntf) {
      // If DeviceIntf is not already created, create a new one to communicate with physical device
      deviceIntf = new DeviceIntf();
      try {
        deviceIntf->setDevice(new HalDevice(ownedHandle));
        deviceIntf->readDebugIPlayout();
      } catch(std::exception& e) {
        // Read debug IP layout could throw an exception
        delete deviceIntf;
        return;
      }
      (db->getStaticInfo()).setDeviceIntf(deviceId, devInterface);
      // configure dataflow etc. may not be required here as those are for PL side
    }

    // Create trace output files
    for(uint64_t n = 0 ; n < numAIETraceOutput ; n++) {
      // Consider both Device Id and Stream Id to create the output file name
      std::string fileName = "aie_trace_" + std::to_string(deviceId) + "_" + std::to_string(n) + ".txt";
      writers.push_back(new AIETraceWriter(fileName.c_str(),
                            "" /*version*/,
                            "" /*creationTime*/,
                            "" /*xrtVersion*/,
                            "" /*toolVersion*/));
      (db->getStaticInfo()).addOpenedFile(fileName, "AIE_EVENT_TRACE");
    }

    // Create AIE Trace Offloader
    uint64_t aieTraceBufSz = GetTS2MMBufSize(true /*isAIETrace*/);
    bool     isPLIO = ((db->getStaticInfo()).getNumTracePLIO(deviceId)) ? true : false;

    uint64_t aieMemorySz = 0;
    if(isPLIO) { // check only one memory for PLIO and for GMIO , assume bank 0 for now
      aieMemorySz = ((db->getStaticInfo()).getMemory(deviceId, deviceIntf->getAIETs2mmMemIndex(0))->size) * 1024;
    } else {
      aieMemorySz = ((db->getStaticInfo()).getMemory(deviceId, 0)->size) * 1024;
    }

    if(aieMemorySz > 0 && aieTraceBufSz > aieMemorySz) {
      aieTraceBufSz = aieMemorySz;
      std::string msg = "Trace buffer size is too big for memory resource.  Using " + std::to_string(aieMemorySz) + " instead." ;
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", msg);
    }
    AIETraceDataLogger* aieTraceLogger = new AIETraceDataLogger(deviceId);

    AIETraceOffload* aieTraceOffloader = new AIETraceOffload(deviceIntf, aieTraceLogger,
                                              isPLIO,          // isPLIO 
                                              aieTraceBufSz,   // total trace buffer size
                                              numAIETraceOutput);  // numStream

    if(!aieTraceOffloader->initReadTrace()) {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_ALLOC_FAIL) ; // FOR AIE ?
      delete aieTraceOffloader;
      delete aieTraceLogger;
      return;
    }
    aieOffloaders[deviceId] = std::make_tuple(aieTraceOffloader, aieTraceLogger, devInterface) ;

  }

  void AieTracePlugin::flushAIEDevice(void* handle)
  {
    // Delete the offloaders for the handle
  }

  void AieTracePlugin::writeAll(bool openNewFiles)
  {
    for(auto w : writers) {
      w->write(openNewFiles);
    }
  }

}

