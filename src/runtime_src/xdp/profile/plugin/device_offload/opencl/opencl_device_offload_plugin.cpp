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
#include "core/common/message.h"

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

// Anonymous namespace for helper functions used only in this file
namespace {
  static std::string 
  getMemoryNameFromID(const std::shared_ptr<xocl::compute_unit> cu,
                      const std::string& arg_id)
  {
    std::string memoryName = "" ;
    try {
      unsigned int index = (unsigned int)(std::stoi(arg_id)) ;
      auto memidx_mask = cu->get_memidx(index) ;
      for (unsigned int memidx = 0 ; memidx < memidx_mask.size() ; ++memidx)
      {
        if (memidx_mask.test(memidx)) {
          // Get bank tag string from index
          memoryName = "DDR";
          auto device_id = cu->get_device() ;
          if (device_id->is_active())
            memoryName = device_id->get_xclbin().memidx_to_banktag(memidx);
          break;
        }
      }
    } catch (const std::runtime_error&)
    {
      memoryName = "DDR" ;
    }

    // Catch old bank format and report as DDR
    if (memoryName.find("bank") != std::string::npos)
      memoryName = "DDR";

    return memoryName.substr(0, memoryName.find_last_of("[")) ;
  }

  static std::string
  debugIPLayoutPath(xrt_xocl::device* device)
  {
    std::string path = device->getDebugIPlayoutPath().get() ;

    if (xdp::getFlowMode() == xdp::HW_EMU) {
      // Full paths to the hardware emulation debug_ip_layout for different
      //  xclbins on the same device are different.  On disk, they are laid
      //  out as follows:
      // .run/<pid>/hw_em/device_0/binary_0/debug_ip_layout
      // .run/<pid>/hw_em/device_0/binary_1/debug_ip_layout
      //  Since both of these should refer to the same device, we only use
      //  the path up to the device name.
      path = path.substr(0, path.find_last_of("/") - 1) ;
      path = path.substr(0, path.find_last_of("/") - 1) ;
    }
    return path ;
  }
} // end anonymous namespace

namespace xdp {

  OpenCLDeviceOffloadPlugin::OpenCLDeviceOffloadPlugin() : DeviceOffloadPlugin()
  {
    // If we aren't the plugin that is handling the device offload,
    //  don't do anything
    if (!active) return ;

    // Software emulation currently has minimal device support for guidance

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
        uint64_t deviceId = o.first ;

        if (deviceIdsToBeFlushed.find(deviceId) != deviceIdsToBeFlushed.end())
        {
          auto offloader = std::get<0>(o.second) ;
          if (offloader->continuous_offload())
          {
            offloader->stop_offload() ;
          }
          else
          {
            offloader->read_trace() ;
            offloader->read_trace_end() ;
          }
          readCounters() ;
        }
      }

      XDPPlugin::endWrite(false);
      db->unregisterPlugin(this) ;
    } // If db alive
    clearOffloaders();

  }

  void OpenCLDeviceOffloadPlugin::writeAll(bool openNewFiles)
  {
    if (!active) return ;
    if (getFlowMode() == SW_EMU) return ;

    DeviceOffloadPlugin::writeAll(openNewFiles) ;
  }

  // This function will only be called if an active device is going to
  //  be reprogrammed.  We can assume the device is good before the call
  //  and bad after this call (until the next update device)
  void OpenCLDeviceOffloadPlugin::flushDevice(void* d)
  {
    if (!active) return ;
    if (getFlowMode() == SW_EMU) return ;

    xrt_xocl::device* device = static_cast<xrt_xocl::device*>(d) ;

    std::string path = debugIPLayoutPath(device) ;

    uint64_t deviceId = db->addDevice(path) ;
    
    if (offloaders.find(deviceId) != offloaders.end())
    {
      auto offloader = std::get<0>(offloaders[deviceId]) ;
      if (offloader->continuous_offload())
      {
        offloader->stop_offload() ;
      }
      else
      {
        offloader->read_trace() ;
        offloader->read_trace_end() ;
      }
    }
    readCounters() ;

    deviceIdsToBeFlushed.erase(deviceId) ;

    clearOffloader(deviceId) ;
    (db->getStaticInfo()).deleteCurrentlyUsedDeviceInterface(deviceId) ;
  }

  void OpenCLDeviceOffloadPlugin::updateDevice(void* d)
  {
    if (!active) return ;
    if (getFlowMode() == SW_EMU){
      updateSWEmulationGuidance() ;
      return ;
    }

    // The OpenCL level expects an xrt_xocl::device to be passed in
    xrt_xocl::device* device = static_cast<xrt_xocl::device*>(d) ;

    std::string path = debugIPLayoutPath(device) ;

    uint64_t deviceId = 0;
    if((getFlowMode() == HW || getFlowMode() == HW_EMU) && 
          (xrt_core::config::get_timeline_trace() || 
           xrt_core::config::get_data_transfer_trace() != "off" ||
           xrt_core::config::get_stall_trace()  != "off")) {
      try {
        deviceId = db->getDeviceId(path) ;
      }
      catch(std::exception& /*e*/) {
        // This is the first time we encountered this particular device
        addDevice(path) ;
      }
    }

    deviceId = db->addDevice(path) ;

    clearOffloader(deviceId);

    if (!(db->getStaticInfo()).validXclbin(device->get_xcl_handle())) {
      std::string msg =
	"Device profiling is only supported on xclbins built using " ;
      msg += std::to_string((db->getStaticInfo()).earliestSupportedToolVersion()) ;
      msg += " tools or later.  To enable device profiling please rebuild." ;

      xrt_core::message::send(xrt_core::message::severity_level::warning,
			      "XRT",
			      msg) ;
      return ;
    }

    // Update the static database with all the information that will
    //  be needed later.
    (db->getStaticInfo()).updateDevice(deviceId, device->get_xcl_handle()) ;
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
      catch(std::exception& /*e*/)
      {
        // Read debug IP Layout could throw an exception
        delete devInterface ;
        return ;
      }
      (db->getStaticInfo()).setDeviceIntf(deviceId, devInterface);
    }

    configureDataflow(deviceId, devInterface) ;
    addOffloader(deviceId, devInterface) ;

    if(getFlowMode() == HW && (xrt_core::config::get_timeline_trace() || 
          xrt_core::config::get_data_transfer_trace() != "off" ||
          xrt_core::config::get_stall_trace()  != "off")) {
      configureTraceIP(devInterface);
      devInterface->clockTraining() ;
    }
    if(getFlowMode() == HW_EMU) {
      configureTraceIP(devInterface);
      devInterface->clockTraining();
    }
    devInterface->startCounters() ;

    // Disable AMs for unsupported features
    configureFa(deviceId, devInterface) ;
    configureCtx(deviceId, devInterface) ;

    // Once the device has been set up, add additional information to 
    //  the static database specific to OpenCL runs
    (db->getStaticInfo()).setMaxReadBW(deviceId, devInterface->getMaxBwRead()) ;
    (db->getStaticInfo()).setMaxWriteBW(deviceId, devInterface->getMaxBwWrite());
    updateOpenCLInfo(deviceId) ;
    deviceIdsToBeFlushed.emplace(deviceId) ;
  }

  void OpenCLDeviceOffloadPlugin::updateOpenCLInfo(uint64_t deviceId)
  {
    // *******************************************************
    // OpenCL specific info 1: Argument lists for each monitor
    // *******************************************************
    DeviceInfo* storedDevice = (db->getStaticInfo()).getDeviceInfo(deviceId) ;
    if (storedDevice == nullptr) return ;
    XclbinInfo* xclbin = storedDevice->currentXclbin() ;
    if (xclbin == nullptr) return ;
    for (auto iter : xclbin->cus)
    {
      ComputeUnitInstance* cu = iter.second ;

      // Find the Compute unit on the XOCL side that matches this compute unit
      std::shared_ptr<xocl::compute_unit> matchingCU ;
      for (auto xoclDeviceId : platform->get_device_range())
      {
        for (auto& xoclCU : xocl::xocl(xoclDeviceId)->get_cus())
        {
          if (xoclCU->get_name() == cu->getName())
          {
            matchingCU = xoclCU ;
            break ;
          }
        }
      }

      std::vector<uint32_t>* AIMIds = cu->getAIMs() ;
      std::vector<uint32_t>* ASMIds = cu->getASMs() ;

      for (size_t i = 0 ; i < AIMIds->size() ; ++i)
      {
        uint32_t AIMIndex = (*AIMIds)[i] ;
        Monitor* monitor = (db->getStaticInfo()).getAIMonitor(deviceId, xclbin, AIMIndex) ;
        if (!monitor) continue ;

        // Construct the argument list of each port
        std::string arguments = "" ;
        for (auto arg : matchingCU->get_symbol()->arguments)
        {
          if ((arg.address_qualifier != 1 && arg.address_qualifier != 4) ||
              arg.atype != xocl::xclbin::symbol::arg::argtype::indexed)
            continue ;
          // Is this particular argument attached to the right port?
          std::string lowerPort = arg.port ;
          std::transform(lowerPort.begin(), lowerPort.end(), lowerPort.begin(),
                         [](char c) { return (char)(std::tolower(c)); }) ;
          if ((monitor->name).find(lowerPort) == std::string::npos)
            continue ;

          // Is this particular argument heading to the right memory?
          std::string memoryName = getMemoryNameFromID(matchingCU, arg.id) ;
          if ((monitor->name).find(memoryName) == std::string::npos)
            continue ;

          if (arguments != "") arguments += "|" ;
          arguments += arg.name ;

          // Also, set the port width for this monitor explicitly
          monitor->portWidth = arg.port_width ;
        }
        monitor->args = arguments ;
      }
      for (size_t i = 0 ; i < ASMIds->size() ; ++i)
      {
        uint32_t ASMIndex = (*ASMIds)[i] ;
        Monitor* monitor = (db->getStaticInfo()).getASMonitor(deviceId, xclbin, ASMIndex) ;
        if (!monitor) continue ;

        // Construct the argument list of each port
        std::string arguments = "" ;
        for (auto arg : matchingCU->get_symbol()->arguments)
        {
          if ((arg.address_qualifier != 1 && arg.address_qualifier != 4) ||
              arg.atype != xocl::xclbin::symbol::arg::argtype::indexed)
            continue ;
          // Is this particular argument attached to the right port?
          std::string lowerPort = arg.port ;
          std::transform(lowerPort.begin(), lowerPort.end(), lowerPort.begin(),
                         [](char c) { return (char)(std::tolower(c)); }) ;
          if ((monitor->name).find(lowerPort) == std::string::npos)
            continue ;

          // Is this particular argument heading to the right memory?
          std::string memoryName = getMemoryNameFromID(matchingCU, arg.id) ;
          if ((monitor->name).find(memoryName) == std::string::npos)
            continue ;

          if (arguments != "") arguments += "|" ;
          arguments += arg.name ;

          // Also, set the port width for this monitor explicitly
          monitor->portWidth = arg.port_width ;
        }
        monitor->args = arguments ;
      }
    }
  }

  void OpenCLDeviceOffloadPlugin::updateSWEmulationGuidance()
  {
    if (platform == nullptr) return ; 
    // There is just some software emulation specific information
    //  we need to add in order to handle guidance rules
    for (auto xrt_device_id : platform->get_device_range()) {
      for (auto& cu : xocl::xocl(xrt_device_id)->get_cus()) {
        (db->getStaticInfo()).addSoftwareEmulationCUInstance(cu->get_kernel_name()) ;
      }
    }

    for (auto device: platform->get_device_range()) {
      auto mem_tp = device->get_axlf_section<const mem_topology*>(axlf_section_kind::MEM_TOPOLOGY) ;
      if (!mem_tp) continue ;
      std::string devName = device->get_unique_name() ;
      for (int i = 0 ; i < mem_tp->m_count ; ++i) {
        std::string mem_tag(reinterpret_cast<const char*>(mem_tp->m_mem_data[i].m_tag));
        if (mem_tag.rfind("bank", 0) == 0)
          mem_tag = "DDR[" + mem_tag.substr(4,4) + "]";
        (db->getStaticInfo()).addSoftwareEmulationMemUsage(devName + "|" + mem_tag, mem_tp->m_mem_data[i].m_used) ;
      }
    }

    std::set<std::string> bitWidthStrings ;
    for (auto device : platform->get_device_range()) {
      for (auto& cu : xocl::xocl(device)->get_cus()) {
        for (auto arg : cu->get_symbol()->arguments) {
          if ((arg.address_qualifier != 1  && arg.address_qualifier != 4) ||
              arg.atype != xocl::xclbin::symbol::arg::argtype::indexed)
            continue ;
          std::string bitWidth = "" ;
          bitWidth += cu->get_name() ;
          bitWidth += "/" ;
          bitWidth += arg.port ;
          std::transform(bitWidth.begin(), bitWidth.end(), bitWidth.begin(),
                         [](char c) { return (char)std::tolower(c); }) ;
          bitWidth += "," ;
          bitWidth += std::to_string(arg.port_width) ;
          bitWidthStrings.emplace(bitWidth) ;
        }
      }
    }
    for (auto iter : bitWidthStrings) {
      (db->getStaticInfo()).addSoftwareEmulationPortBitWidth(iter) ;
    }

  }
} // end namespace xdp
