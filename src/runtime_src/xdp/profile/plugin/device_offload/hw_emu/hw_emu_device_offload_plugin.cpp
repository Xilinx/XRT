/**
 * Copyright (C) 2022 Xilinx, Inc
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include <string>

#include "core/common/message.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/device_offload/hw_emu/hw_emu_device_offload_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"

// Anonymous namespace for local helper functions
namespace {

  static std::string ProcessHwEmuDebugIpLayoutPath(void* handle)
  {
    std::string path = xdp::util::getDebugIpLayoutPath(handle);
    return path ;
  }

} // end anonymous namespace

namespace xdp {

  HWEmuDeviceOffloadPlugin::HWEmuDeviceOffloadPlugin()
    : PLDeviceOffloadPlugin()
  {
    db->registerInfo(info::device_offload) ;
  }

  HWEmuDeviceOffloadPlugin::~HWEmuDeviceOffloadPlugin()
  {
    if (VPDatabase::alive()) {
      readTrace() ;
      readCounters() ;
      XDPPlugin::endWrite() ;

      // On Alveo hardware emulation (where there is only one device)
      // we have to remove the device interface at this point
      if (!isEdge()) {
        for (auto deviceId : devicesSeen) {
          db->getStaticInfo().removeDeviceIntf(deviceId);
        }
      }

      db->unregisterPlugin(this) ;
    }

    clearOffloaders();
  }

  void HWEmuDeviceOffloadPlugin::readTrace()
  {
    for (const auto& o : offloaders) {
      auto offloader = std::get<0>(o.second) ;
      flushTraceOffloader(offloader);
      checkTraceBufferFullness(offloader, o.first);
    }
  }

  void HWEmuDeviceOffloadPlugin::flushDevice(void* handle)
  {
    std::string path = ProcessHwEmuDebugIpLayoutPath(handle) ;
    if (path == "")
      return ;

    uint64_t deviceId = db->addDevice(path) ;

    if (offloaders.find(deviceId) != offloaders.end()) {
      auto offloader = std::get<0>(offloaders[deviceId]) ;
      flushTraceOffloader(offloader);
    }
    readCounters();

    clearOffloader(deviceId) ;
  }

  void HWEmuDeviceOffloadPlugin::updateDevice(void* userHandle)
  {
    std::string path = ProcessHwEmuDebugIpLayoutPath(userHandle) ;
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
    db->getStaticInfo().updateDeviceFromHandle(deviceId, std::move(std::make_unique<HalDevice>(userHandle)), userHandle) ;

    // For the HAL level, we must create a device interface using
    //  the xdp::HalDevice to communicate with the physical device
    PLDeviceIntf* devInterface = (db->getStaticInfo()).getDeviceIntf(deviceId);

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
