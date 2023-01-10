/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#include <array>
#include <string>
#include <vector>

// For HAL applications
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/device_offload/hal/hal_device_offload_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/device_trace/device_trace_writer.h"

namespace xdp {

  HALDeviceOffloadPlugin::HALDeviceOffloadPlugin() : DeviceOffloadPlugin()
  {
    db->registerInfo(info::device_offload) ;

    // Open all of the devices that exist so we can keep our own pointer
    //  to access them.
    uint32_t index = 0 ;
    void* handle = xclOpen(index, "/dev/null", XCL_INFO) ;
    
    while (handle != nullptr)
    {
      // First, keep track of all open handles
      deviceHandles.push_back(handle) ;

      // Second, add all the information and a writer for this device
      std::array<char, sysfs_max_path_length> pathBuf = {0};
      xclGetDebugIPlayoutPath(handle, pathBuf.data(), (sysfs_max_path_length-1) ) ;
      std::string path(pathBuf.data());
      if (path != "") {
        addDevice(path) ;

        // Now, keep track of the device ID for this device so we can use
        //  our own handle
        deviceIdToHandle[db->addDevice(path)] = handle ;
      }

      // Move on to the next device
      ++index ;
      handle = xclOpen(index, "/dev/null", XCL_INFO) ;
    }
  }

  HALDeviceOffloadPlugin::~HALDeviceOffloadPlugin()
  {
    if (VPDatabase::alive())
    {
      // If we are destroyed before the database, we need to
      //  do a final flush of our devices, then write
      //  all of our writers, then finally unregister ourselves
      //  from the database.

      readTrace() ;
      readCounters() ;
      XDPPlugin::endWrite();
      db->unregisterPlugin(this) ;
    }

    clearOffloaders();

    for (auto h : deviceHandles)
    {
      xclClose(h) ;
    }
  }

  void HALDeviceOffloadPlugin::readTrace()
  {
    for (auto o : offloaders) {
      auto offloader = std::get<0>(o.second) ;
      flushTraceOffloader(offloader);
      checkTraceBufferFullness(offloader, o.first);
    }
  }

  // This function will only be called if an active device is going
  //  to be reprogrammed.  We can assume the device is good.
  void HALDeviceOffloadPlugin::flushDevice(void* handle)
  {
    // For HAL devices, the pointer passed in is an xrtDeviceHandle
    char pathBuf[maxPathLength] ;
    memset(pathBuf, 0, maxPathLength) ;
    xclGetDebugIPlayoutPath(handle, pathBuf, maxPathLength-1) ;

    std::string path(pathBuf) ;
    if (path == "")
      return ;
    
    uint64_t deviceId = db->addDevice(path) ;

    if (offloaders.find(deviceId) != offloaders.end()) {
      auto offloader = std::get<0>(offloaders[deviceId]) ;
      flushTraceOffloader(offloader);
    }
    readCounters();

    clearOffloader(deviceId) ;
    (db->getStaticInfo()).deleteCurrentlyUsedDeviceInterface(deviceId) ;
  }

  void HALDeviceOffloadPlugin::updateDevice(void* userHandle)
  {
    // For HAL devices, the pointer passed in is an xrtDeviceHandle.
    //  We will query information on that passed in handle, but we
    //  should user our own locally opened handle to access the physical
    //  device.
    char pathBuf[maxPathLength] ;
    memset(pathBuf, 0, maxPathLength) ;
    xclGetDebugIPlayoutPath(userHandle, pathBuf, maxPathLength-1) ;

    std::string path(pathBuf) ;
    if (path == "")
      return ;

    uint64_t deviceId = db->addDevice(path) ;
    void* ownedHandle = deviceIdToHandle[deviceId] ;
  
    clearOffloader(deviceId); 

    if (!(db->getStaticInfo()).validXclbin(userHandle)) {
      std::string msg =
        "Device profiling is only supported on xclbins built using " ;
      msg += std::to_string((db->getStaticInfo()).earliestSupportedToolVersion()) ;
      msg += " tools or later.  To enable device profiling please rebuild." ;

      xrt_core::message::send(xrt_core::message::severity_level::warning,
                              "XRT",
                              msg) ;
      return ;
    }
    
    // Update the static database with all the information that
    //  will be needed later
    (db->getStaticInfo()).updateDevice(deviceId, userHandle) ;
    {
      struct xclDeviceInfo2 info ;
      if (xclGetDeviceInfo2(userHandle, &info) == 0)
        (db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
    }

    // For the HAL level, we must create a device interface using 
    //  the xdp::HalDevice to communicate with the physical device
    DeviceIntf* devInterface = (db->getStaticInfo()).getDeviceIntf(deviceId);
    if (devInterface == nullptr)
      devInterface = db->getStaticInfo().createDeviceIntf(deviceId, new HalDevice(ownedHandle));

    configureDataflow(deviceId, devInterface) ;
    addOffloader(deviceId, devInterface) ;
    configureTraceIP(devInterface) ;
    // Disable AMs for unsupported features
    configureFa(deviceId, devInterface) ;
    configureCtx(deviceId, devInterface) ;

    devInterface->clockTraining() ;
    startContinuousThreads(deviceId) ;
    devInterface->startCounters() ;

    // Once the device has been set up, add additional information to 
    //  the static database
    (db->getStaticInfo()).setHostMaxReadBW(deviceId, devInterface->getHostMaxBwRead()) ;
    (db->getStaticInfo()).setHostMaxWriteBW(deviceId, devInterface->getHostMaxBwWrite());
    (db->getStaticInfo()).setKernelMaxReadBW(deviceId, devInterface->getKernelMaxBwRead()) ;
    (db->getStaticInfo()).setKernelMaxWriteBW(deviceId, devInterface->getKernelMaxBwWrite());
  }
  
} // end namespace xdp
