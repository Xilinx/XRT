/**
 * Copyright (C) 2022 Xilinx, Inc
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

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"

#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/device_offload/hw_emu/hw_emu_device_offload_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"

// Anonymous namespace for local helper functions
namespace {

  static std::string getDebugIPLayoutPath(void* handle)
  {
    constexpr int MAX_PATH_LENGTH = 512 ;

    char pathBuf[MAX_PATH_LENGTH] ;
    xclGetDebugIPlayoutPath(handle, pathBuf, MAX_PATH_LENGTH) ;
    pathBuf[MAX_PATH_LENGTH-1] = '\0' ;  // In case of too long paths

    std::string path(pathBuf) ;

    if (path == "")
      return path ;

    // Full paths to the hardware emulation debug_ip_layout for different
    //  xclbins on the same device are different.  On disk, they are laid
    //  out as follows:
    // .run/<pid>/hw_em/device_0/binary_0/debug_ip_layout
    // .run/<pid>/hw_em/device_0/binary_1/debug_ip_layout
    //  Since both of these should refer to the same device, we only use
    //  the path up to the device name.
    path = path.substr(0, path.find_last_of("/") - 1) ;// remove debug_ip_layout
    path = path.substr(0, path.find_last_of("/") - 1) ;// remove binary_x
    return path ;
  }

} // end anonymous namespace

namespace xdp {

  HWEmuDeviceOffloadPlugin::HWEmuDeviceOffloadPlugin()
    : DeviceOffloadPlugin()
  {
    db->registerInfo(info::device_offload) ;
  }

  HWEmuDeviceOffloadPlugin::~HWEmuDeviceOffloadPlugin()
  {
    if (VPDatabase::alive()) {
      readTrace() ;
      readCounters() ;
      XDPPlugin::endWrite(false) ;
      db->unregisterPlugin(this) ;
    }

    clearOffloaders();
  }

  void HWEmuDeviceOffloadPlugin::readTrace()
  {
    for (auto o : offloaders) {
      auto offloader = std::get<0>(o.second) ;
      flushTraceOffloader(offloader);
      checkTraceBufferFullness(offloader, o.first);
    }
  }

  void HWEmuDeviceOffloadPlugin::flushDevice(void* handle)
  {
    std::string path = getDebugIPLayoutPath(handle) ;
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

  void HWEmuDeviceOffloadPlugin::updateDevice(void* userHandle)
  {
    std::string path = getDebugIPLayoutPath(userHandle) ;
    if (path == "")
      return ;

    uint64_t deviceId = db->addDevice(path) ;
    if (devicesSeen.find(deviceId) == devicesSeen.end()) {
      devicesSeen.emplace(deviceId) ;
      addDevice(path) ; // Base class functionality to add writer
    }

    // Clear out any previous interface we might have had for talking to this
    //  particular device.
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
      if (xclGetDeviceInfo2(userHandle, &info) == 0) {
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
        devInterface->setDevice(new HalDevice(userHandle)) ;
        devInterface->readDebugIPlayout() ;      
      }
      catch(std::exception& /*e*/) {
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
    startContinuousThreads(deviceId) ;
    devInterface->startCounters() ;

    // Once the device has been set up, add additional information to 
    //  the static database
    (db->getStaticInfo()).setMaxReadBW(deviceId, devInterface->getMaxBwRead()) ;
    (db->getStaticInfo()).setMaxWriteBW(deviceId, devInterface->getMaxBwWrite());
  }

} // end namespace xdp
