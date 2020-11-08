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

#include <string>
#include <vector>
#include <iostream>

// For HAL applications
#include "core/common/xrt_profiling.h"

#include "xdp/profile/writer/device_trace/device_trace_writer.h"

#include "xdp/profile/plugin/device_offload/hal/hal_device_offload_plugin.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/database/events/creator/device_event_trace_logger.h"

#include "core/common/system.h"
#include "core/common/message.h"

namespace xdp {

  HALDeviceOffloadPlugin::HALDeviceOffloadPlugin() : DeviceOffloadPlugin()
  {
    // If we aren't the plugin that is handling the device offload, don't
    //  do anything.
    if (!active) return ;

    // Open all of the devices that exist so we can keep our own pointer
    //  to access them.
    uint32_t index = 0 ;
    void* handle = xclOpen(index, "/dev/null", XCL_INFO) ;
    
    while (handle != nullptr)
    {
      // First, keep track of all open handles
      deviceHandles.push_back(handle) ;

      // Second, add all the information and a writer for this device
      char pathBuf[512] ;
      memset(pathBuf, 0, 512) ;
      xclGetDebugIPlayoutPath(handle, pathBuf, 512) ;

      std::string path(pathBuf) ;
      addDevice(path) ;

      // Now, keep track of the device ID for this device so we can use
      //  our own handle
      deviceIdToHandle[db->addDevice(path)] = handle ;

      // Move on to the next device
      ++index ;
      handle = xclOpen(index, "/dev/null", XCL_INFO) ;
    }
  }

  HALDeviceOffloadPlugin::~HALDeviceOffloadPlugin()
  {
    if (!active) return ;

    if (VPDatabase::alive())
    {
      // If we are destroyed before the database, we need to
      //  do a final flush of our devices, then write
      //  all of our writers, then finally unregister ourselves
      //  from the database.
      for (auto o : offloaders)
      {
        auto offloader = std::get<0>(o.second) ;

        offloader->read_trace() ;
        offloader->read_trace_end() ;
        if(offloader->trace_buffer_full()) {
          std::string msg;
          if(offloader->has_ts2mm()) {
            msg = TS2MM_WARN_MSG_BUF_FULL;
          } else {
            msg = FIFO_WARN_MSG;
          } 
          xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", msg);
        }

      }
      for (auto w : writers)
      {
        w->write(false) ;
      }
      db->unregisterPlugin(this) ;
    }

    clearOffloaders();

    for (auto h : deviceHandles)
    {
      xclClose(h) ;
    }
  }

  void HALDeviceOffloadPlugin::writeAll(bool openNewFiles)
  {
    if (!active) return ;
    DeviceOffloadPlugin::writeAll(openNewFiles) ;
  }

  // This function will only be called if an active device is going
  //  to be reprogrammed.  We can assume the device is good.
  void HALDeviceOffloadPlugin::flushDevice(void* handle)
  {
    if (!active) return ;
    
    // For HAL devices, the pointer passed in is an xrtDeviceHandle
    char pathBuf[512] ;
    memset(pathBuf, 0, 512) ;
    xclGetDebugIPlayoutPath(handle, pathBuf, 512) ;

    std::string path(pathBuf) ;
    
    uint64_t deviceId = db->addDevice(path) ;

    if (offloaders.find(deviceId) != offloaders.end())
    {
      std::get<0>(offloaders[deviceId])->read_trace() ;
    }    
  }

  void HALDeviceOffloadPlugin::updateDevice(void* userHandle)
  {
    if (!active) return ;

    // For HAL devices, the pointer passed in is an xrtDeviceHandle.
    //  We will query information on that passed in handle, but we
    //  should user our own locally opened handle to access the physical
    //  device.
    char pathBuf[512] ;
    memset(pathBuf, 0, 512) ;
    xclGetDebugIPlayoutPath(userHandle, pathBuf, 512) ;

    std::string path(pathBuf) ;

    uint64_t deviceId = db->addDevice(path) ;
    void* ownedHandle = deviceIdToHandle[deviceId] ;
  
    clearOffloader(deviceId); 
    
    // Update the static database with all the information that
    //  will be needed later
    (db->getStaticInfo()).updateDevice(deviceId, userHandle) ;
    {
      struct xclDeviceInfo2 info ;
      if (xclGetDeviceInfo2(userHandle, &info) == 0)
      {
	(db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
      }
    }

    // For the HAL level, we must create a device interface using 
    //  the xdp::HalDevice to communicate with the physical device
    DeviceIntf* devInterface = (db->getStaticInfo()).getDeviceIntf(deviceId);
    if(nullptr == devInterface) {
      // If DeviceIntf is not already created, create a new one to communicate with physical device
      devInterface = new DeviceIntf() ;
      try {
        devInterface->setDevice(new HalDevice(ownedHandle)) ;
        devInterface->readDebugIPlayout() ;      
      }
      catch(std::exception& e)
      {
        // Read debug IP layout could throw an exception
        delete devInterface ;
        return;
      }
      (db->getStaticInfo()).setDeviceIntf(deviceId, devInterface);
    }

    configureDataflow(deviceId, devInterface) ;
    addOffloader(deviceId, devInterface) ;
    configureTraceIP(devInterface) ;
    // Disable AMs for unsupported features
    configureFa(deviceId, devInterface) ;
    configureCtx(deviceId, devInterface) ;

    devInterface->clockTraining() ;
  }
  
} // end namespace xdp
