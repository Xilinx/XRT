/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#include <array>
#include <string>
#include <vector>

// For HAL applications
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/include/xrt/xrt_device.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/device_offload/hal/hal_device_offload_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/device_trace/device_trace_writer.h"

namespace xdp {

  HALDeviceOffloadPlugin::HALDeviceOffloadPlugin() : PLDeviceOffloadPlugin()
  {
    db->registerInfo(info::device_offload) ;
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
  }

  void HALDeviceOffloadPlugin::readTrace()
  {
    for (const auto& o : offloaders) {
      auto offloader = std::get<0>(o.second) ;
      flushTraceOffloader(offloader);
      checkTraceBufferFullness(offloader, o.first);
    }
  }

  void HALDeviceOffloadPlugin::init()
  {
    if (initialized) {
      return;
    }
    // Open all existing devices so that XDP can access the owned handles
    uint32_t numDevices = xrt_core::get_total_devices(true).second;
    uint32_t index = 0;
    while (index < numDevices) {
      try {
        xrtDevices.push_back(std::make_unique<xrt::device>(index));

        auto ownedHandle = xrtDevices[index]->get_handle()->get_device_handle();
        std::string path = util::getDebugIpLayoutPath(ownedHandle);

        if ("" != path) {
          uint64_t deviceId = (db->getStaticInfo()).getDeviceContextUniqueId(ownedHandle);
          createWriters(deviceId); // Base class functionality to add writer

          // Now, map device ID of this device with device handle owned by XDP
          deviceIdToHandle[deviceId] = ownedHandle;
        }

        // Move on to the next device
        ++index;
      } catch (const std::runtime_error& e) {
        std::string msg = "Could not open device at index " + std::to_string(index) + e.what();
        xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
        ++index;
        continue;
      }
    }
    initialized = true;
  }

  // This function will only be called if an active device is going
  //  to be reprogrammed.  We can assume the device is good.
  void HALDeviceOffloadPlugin::flushDevice(void* handle)
  {
    if (!handle)
      return;

    // NOTE: In load xclin style, multiple calls to loadXclbin have to flush before updateDevice
    // This makes sure we do not flush if the app style is not set
    if ((db->getStaticInfo()).getAppStyle() == AppStyle::APP_STYLE_NOT_SET) {
      return ;
    }

    // For HAL devices, the pointer passed in is an xrtDeviceHandle
    std::string path = util::getDebugIpLayoutPath(handle);
    
    uint64_t deviceId = (db->getStaticInfo()).getDeviceContextUniqueId(handle);

    if (offloaders.find(deviceId) != offloaders.end()) {
      auto offloader = std::get<0>(offloaders[deviceId]) ;
      flushTraceOffloader(offloader);
    }
    readCounters();

    clearOffloader(deviceId) ;
  }

  void HALDeviceOffloadPlugin::updateDevice(void* userHandle, bool hw_context_flow)
  {
    if (!userHandle)
      return ;

    if (!((db->getStaticInfo()).continueXDPConfig(hw_context_flow)))
      return;

    if (hw_context_flow && (!(db->getStaticInfo()).xclbinContainsPl(userHandle, hw_context_flow)))
      return ;

    auto device = util::convertToCoreDevice(userHandle, hw_context_flow);
#if ! defined (XRT_X86_BUILD) && ! defined (XDP_CLIENT_BUILD)
    if (1 == device->get_device_id() && xrt_core::config::get_xdp_mode() == "xdna") {  // Device 0 for xdna(ML) and device 1 for zocl(PL)
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Got ZOCL device when xdp_mode is set to XDNA. PL Trace is not yet supported for this combination.");
      return;
    }
    else if(0 == device->get_device_id() && xrt_core::config::get_xdp_mode() == "zocl") {
    #ifdef XDP_VE2_ZOCL_BUILD
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "Got XDNA device when xdp_mode is set to ZOCL. PL Trace is not yet supported for this combination.");
      return;
    #else
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Got EDGE device when xdp_mode is set to ZOCL. PL Trace should be available.");
    #endif
      }
#endif

    void* ownedHandle = nullptr;
    uint64_t deviceId = (db->getStaticInfo()).getDeviceContextUniqueId(userHandle);
    if (hw_context_flow) {
      createWriters(deviceId);
    }
    else {
      // For HAL devices, the pointer passed in is an xrtDeviceHandle.
      //  We will query information on that passed in handle, but we
      //  should use our own locally opened handle to access the physical
      //  device.
      //  NOTE: Applicable to LOAD_XCLBIN_STYLE app style. 
      init();
      ownedHandle = deviceIdToHandle[deviceId] ;
    }

    clearOffloader(deviceId); 

    if (!(db->getStaticInfo()).validXclbin(userHandle, hw_context_flow)) {
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
    // will be needed later
    if(hw_context_flow)
      // TODO: should we use updateDeviceFromCoreDeviceHwCtxFlow or updateDeviceFromCoreDevice
      db->getStaticInfo().updateDeviceFromCoreDevice(deviceId, device, true, std::make_unique<HalDevice>(device->get_device_handle()));
    else
      db->getStaticInfo().updateDeviceFromHandle(deviceId, std::make_unique<HalDevice>(ownedHandle), userHandle) ;

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
