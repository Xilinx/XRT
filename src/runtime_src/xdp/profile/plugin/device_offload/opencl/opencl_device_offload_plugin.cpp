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

#include <string>

// Includes from xrt_coreutil
#include "core/common/system.h"

// Includes from xilinxopencl
#include "xocl/core/platform.h"
#include "xocl/core/device.h"

// Includes from XDP
#include "xdp/profile/plugin/device_offload/opencl/opencl_device_offload_plugin.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/xrt_device/xdp_xrt_device.h"
#include "xdp/profile/database/events/creator/device_event_trace_logger.h"
#include "xdp/profile/writer/vp_base/vp_writer.h"

namespace xdp {

  OpenCLDeviceOffloadPlugin::OpenCLDeviceOffloadPlugin() : DeviceOffloadPlugin()
  {
    // If we aren't the plugin that is handling the device offload,
    //  don't do anything
    if (!active) return ;

    // Software emulation currently has no device support
    if (getFlowMode() == SW_EMU) return ;

    // Since we are using xocl and xrt level objects in this plugin,
    //  we need a pointer to the shared platform to make sure the
    //  xrt_xocl::device objects aren't destroyed before we get a chance
    //  to offload the trace at the end
    platform = xocl::get_shared_platform() ;
  }

  OpenCLDeviceOffloadPlugin::~OpenCLDeviceOffloadPlugin()
  {
    if (!active) return ;
    if (getFlowMode() == SW_EMU) return ;

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
      }

      for (auto w : writers)
      {
	w->write(false) ;
      }
      db->unregisterPlugin(this) ;
    }

    clearOffloaders();

  }

  void OpenCLDeviceOffloadPlugin::writeAll(bool openNewFiles)
  {
    if (!active) return ;
    if (getFlowMode() == SW_EMU) return ;

    DeviceOffloadPlugin::writeAll(openNewFiles) ;
  }

  // This function will only be called if an active device is going to
  //  be reprogrammed.  We can assume the device is good.
  void OpenCLDeviceOffloadPlugin::flushDevice(void* d)
  {
    if (!active) return ;
    if (getFlowMode() == SW_EMU) return ;

    xrt_xocl::device* device = static_cast<xrt_xocl::device*>(d) ;

    std::string path = device->getDebugIPlayoutPath().get() ;

    uint64_t deviceId = db->addDevice(path) ;
    
    if (offloaders.find(deviceId) != offloaders.end())
    {
      std::get<0>(offloaders[deviceId])->read_trace() ;
    }
  }

  void OpenCLDeviceOffloadPlugin::updateDevice(void* d)
  {
    if (!active) return ;
    if (getFlowMode() == SW_EMU) return ;

    // The OpenCL level expects an xrt_xocl::device to be passed in
    xrt_xocl::device* device = static_cast<xrt_xocl::device*>(d) ;

    // In both hardware and hardware emulation, the debug ip layout path
    //  is used as a unique identifier of the physical device
    std::string path = device->getDebugIPlayoutPath().get() ;

    uint64_t deviceId = 0;
    try {
      deviceId = db->getDeviceId(path) ;
    }
    catch(std::exception& e)
    {
      // This is the first time we encountered this particular device
      addDevice(path) ;
    }

    deviceId = db->addDevice(path) ;

    clearOffloader(deviceId);

    // Update the static database with all the information that will
    //  be needed later.
    (db->getStaticInfo()).updateDevice(deviceId, device->get_handle()) ;
    (db->getStaticInfo()).setDeviceName(deviceId, device->getName()) ;

    // For the OpenCL level, we must create a device inteface using
    //  the xdp::XrtDevice to communicate with the physical device
    DeviceIntf* devInterface = (db->getStaticInfo()).getDeviceIntf(deviceId);
    if(nullptr == devInterface) {
      // If DeviceIntf is not already created, create a new one to communicate with physical device
      devInterface = new DeviceIntf() ;
      try {
        devInterface->setDevice(new XrtDevice(device)) ;
        devInterface->readDebugIPlayout() ;
      }
      catch(std::exception& e)
      {
        // Read debug IP Layout could throw an exception
        delete devInterface ;
        return ;
      }
      (db->getStaticInfo()).setDeviceIntf(deviceId, devInterface);
    }

    configureDataflow(deviceId, devInterface) ;
    addOffloader(deviceId, devInterface) ;
    configureTraceIP(devInterface) ;
    // Disable AMs for unsupported features
    configureFa(deviceId, devInterface) ;
    configureCtx(deviceId, devInterface) ;

    if (getFlowMode() == HW) devInterface->clockTraining() ;
  }
  
} // end namespace xdp
