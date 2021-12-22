/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "core/common/xclbin_parser.h"

// Includes from xilinxopencl
#include "xocl/core/platform.h"
#include "xocl/core/device.h"

// Includes from XDP
#include "xdp/profile/plugin/device_offload/opencl/opencl_device_offload_plugin.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/utility.h"

// Anonymous namespace for helper functions used only in this file
namespace {
  static std::string 
  getMemoryNameFromID(const std::shared_ptr<xocl::compute_unit> cu,
                      const size_t index)
  {
    std::string memoryName = "" ;
    try {
      auto memidx_mask = cu->get_memidx(index) ;
      for (unsigned int memidx = 0 ; memidx < memidx_mask.size() ; ++memidx) {
        if (memidx_mask.test(memidx)) {
          // Get bank tag string from index
          memoryName = "DDR";
          auto device_id = cu->get_device() ;
          if (device_id->is_active())
            memoryName = device_id->get_xclbin().memidx_to_banktag(memidx);
          break;
        }
      }
    } catch (const std::runtime_error&) {
      memoryName = "DDR" ;
    }

    // If we find the old "bank" format, just return it as is since our
    //  monitor name could also have "bank" in it.  We'll check
    //  if converting the name to DDR works separately.

    return memoryName.substr(0, memoryName.find_last_of("[")) ;
  }

  static std::string convertBankToDDR(const std::string& name)
  {
    auto loc = name.find("bank") ;
    if (loc == std::string::npos)
      return name ;

    std::string ddr = "DDR[" ;
    ddr += name.substr(loc + 4) ;
    ddr += "]" ;
    return ddr ;
  }

  static std::string debugIPLayoutPath(xrt_xocl::device* device)
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

  OpenCLDeviceOffloadPlugin::OpenCLDeviceOffloadPlugin()
    : DeviceOffloadPlugin()
  {
    // Software emulation currently has minimal device support for guidance

    // Since we are using xocl and xrt level objects in this plugin,
    //  we need a pointer to the shared platform to make sure the
    //  xrt_xocl::device objects aren't destroyed before we get a chance
    //  to access the information we need.
    platform = xocl::get_shared_platform() ;
  }

  OpenCLDeviceOffloadPlugin::~OpenCLDeviceOffloadPlugin()
  {
    if (VPDatabase::alive())
      db->unregisterPlugin(this) ;
  }

  // readTrace can be called from either the destructor or from a broadcast
  //  message from another plugin that needs the trace updated before it can
  //  progress.
  void OpenCLDeviceOffloadPlugin::readTrace()
  {
    // Intentionally left blank so we don't call the base class function
    //  when called and we don't want to actually do anything
  }

  void OpenCLDeviceOffloadPlugin::writeAll(bool openNewFiles)
  {
    // Intentionally left blank so we don't call the base class function
    //  when called and we don't want to actually do anything
  }

  // This function will only be called if an active device is going to
  //  be reprogrammed.  We can assume the device is good before the call
  //  and bad after this call (until the next update device)
  void OpenCLDeviceOffloadPlugin::flushDevice(void* d)
  {
    // Intentionally left blank so we don't call the base class function.
    //  This plugin no longer communicates with the actual device so
    //  there is no information to be flushed.
  }

  void OpenCLDeviceOffloadPlugin::updateDevice(void* d)
  {
    if (getFlowMode() == SW_EMU){
      updateSWEmulationGuidance() ;
      return ;
    }

    // The OpenCL level expects an xrt_xocl::device to be passed in
    xrt_xocl::device* device = static_cast<xrt_xocl::device*>(d) ;

    std::string path = debugIPLayoutPath(device) ;

    uint64_t deviceId = 0;
    deviceId = db->addDevice(path) ;

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

    updateOpenCLInfo(deviceId) ;
  }

  void OpenCLDeviceOffloadPlugin::updateOpenCLInfo(uint64_t deviceId)
  {
    // *******************************************************
    // OpenCL specific info 1: Argument lists for each monitor
    // *******************************************************
    // Argument information on each port is only available on via
    //  accessing XOCL constructs.  We should only add port information
    //  based on the debug monitors that exist, however, so we need to
    //  cross-reference our data structures with the XOCL constructs.
    DeviceInfo* storedDevice = (db->getStaticInfo()).getDeviceInfo(deviceId) ;
    if (storedDevice == nullptr)
      return ;
    XclbinInfo* xclbin = storedDevice->currentXclbin() ;
    if (xclbin == nullptr)
      return ;

    for (auto iter : xclbin->cus) {
      ComputeUnitInstance* cu = iter.second ;

      // Find the Compute unit on the XOCL side that matches this compute unit
      std::shared_ptr<xocl::compute_unit> matchingCU ;
      for (auto xoclDeviceId : platform->get_device_range()) {
        for (auto& xoclCU : xocl::xocl(xoclDeviceId)->get_cus()) {
          if (xoclCU->get_name() == cu->getName()) {
            matchingCU = xoclCU ;
            break ;
          }
        }
      }

      // Now go through all the monitors on the compute unit and set
      //  information in our data structures based on XOCL info.
      std::vector<uint32_t>* AIMIds = cu->getAIMs() ;
      for (uint32_t AIMIndex : (*AIMIds)) {
        Monitor* monitor =
          (db->getStaticInfo()).getAIMonitor(deviceId, xclbin, AIMIndex) ;
        if (!monitor)
          continue ;

        // Construct the argument list of each port
        std::string arguments = "" ;
        for (const auto& arg : matchingCU->get_args()) {
          if (arg.index == xrt_core::xclbin::kernel_argument::no_index)
            continue;

          if (arg.type != xrt_core::xclbin::kernel_argument::argtype::global
              && arg.type != xrt_core::xclbin::kernel_argument::argtype::stream)
            continue;

          // Is this particular argument attached to the right port?
          std::string lowerPort = arg.port ;
          std::transform(lowerPort.begin(), lowerPort.end(), lowerPort.begin(),
                         [](char c) { return (char)(std::tolower(c)); }) ;
          if ((monitor->name).find(lowerPort) == std::string::npos)
            continue ;

          // Is this particular argument heading to the right memory?
          std::string memoryName = getMemoryNameFromID(matchingCU, arg.index) ;
          std::string convertedName = convertBankToDDR(memoryName) ;
          if (monitor->name.find(memoryName) == std::string::npos &&
              monitor->name.find(convertedName) == std::string::npos )
            continue ;

          if (arguments != "")
            arguments += "|" ;
          arguments += arg.name ;

          // Also, set the port width for this monitor explicitly
          monitor->portWidth = arg.port_width ;
        }
        monitor->args = arguments ;
      }

      std::vector<uint32_t>* ASMIds = cu->getASMs() ;
      for (uint32_t ASMIndex : (*ASMIds)) {
        Monitor* monitor =
          (db->getStaticInfo()).getASMonitor(deviceId, xclbin, ASMIndex) ;
        if (!monitor)
          continue ;

        // Construct the argument list of each port
        std::string arguments = "" ;
        for (auto arg : matchingCU->get_args()) {
          if (arg.index == xrt_core::xclbin::kernel_argument::no_index)
            continue;

          if (arg.type != xrt_core::xclbin::kernel_argument::argtype::global
              && arg.type != xrt_core::xclbin::kernel_argument::argtype::stream)
            continue;

          // Is this particular argument attached to the right port?
          std::string lowerPort = arg.port ;
          std::transform(lowerPort.begin(), lowerPort.end(), lowerPort.begin(),
                         [](char c) { return (char)(std::tolower(c)); }) ;
          if ((monitor->name).find(lowerPort) == std::string::npos)
            continue ;

          // Is this particular argument heading to the right memory?
          std::string memoryName = getMemoryNameFromID(matchingCU, arg.index) ;
          std::string convertedName = convertBankToDDR(memoryName) ;
          if (monitor->name.find(memoryName) == std::string::npos &&
              monitor->name.find(convertedName) == std::string::npos) {
            continue ;
          }

          if (arguments != "")
            arguments += "|" ;
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
    if (platform == nullptr)
      return ;

    // There is just some software emulation specific information
    //  we need to add in order to handle guidance rules

    // Make the connection between kernel name and compute units
    for (auto xrt_device_id : platform->get_device_range()) {
      for (auto& cu : xocl::xocl(xrt_device_id)->get_cus()) {
        (db->getStaticInfo()).addSoftwareEmulationCUInstance(cu->get_kernel_name()) ;
      }
    }

    // Keep track of which memories are used
    for (auto device: platform->get_device_range()) {
      if (!device->is_active())
        continue ;
      auto mem_tp = device->get_axlf_section<const mem_topology*>(axlf_section_kind::MEM_TOPOLOGY) ;
      if (!mem_tp)
        continue ;
      std::string devName = device->get_unique_name() ;
      for (int i = 0 ; i < mem_tp->m_count ; ++i) {
        std::string mem_tag(reinterpret_cast<const char*>(mem_tp->m_mem_data[i].m_tag));
        if (mem_tag.rfind("bank", 0) == 0)
          mem_tag = "DDR[" + mem_tag.substr(4,4) + "]";
        (db->getStaticInfo()).addSoftwareEmulationMemUsage(devName + "|" + mem_tag, mem_tp->m_mem_data[i].m_used) ;
      }
    }

    // Add the bit widths for each argument and port
    std::set<std::string> bitWidthStrings ;
    for (auto device : platform->get_device_range()) {
      for (auto& cu : xocl::xocl(device)->get_cus()) {
        for (auto arg : cu->get_args()) {
          if (arg.index == xrt_core::xclbin::kernel_argument::no_index)
            continue;

          if (arg.type != xrt_core::xclbin::kernel_argument::argtype::global
              && arg.type != xrt_core::xclbin::kernel_argument::argtype::stream)
            continue;

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
    for (const auto& str : bitWidthStrings) {
      (db->getStaticInfo()).addSoftwareEmulationPortBitWidth(str) ;
    }
  }
} // end namespace xdp
