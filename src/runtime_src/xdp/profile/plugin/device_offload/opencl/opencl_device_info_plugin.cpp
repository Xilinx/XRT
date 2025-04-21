/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

// Includes from xrt_coreutil
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/xclbin_parser.h"

// Includes from xilinxopencl
#include "xocl/core/device.h"
#include "xocl/core/platform.h"

// Includes from XDP
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/device_info.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/plugin/device_offload/opencl/opencl_device_info_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"

// Anonymous namespace for helper functions used only in this file
namespace {

  static std::string debugIPLayoutPath(xrt_xocl::device* device)
  {
    std::string path = device->getDebugIPlayoutPath().get() ;

    // If the path to the debug_ip_layout.rtd file is too long,
    //  the call to getDebugIPlayoutPath returns an empty string
    if (path == "")
      return path ;

    if (xdp::getFlowMode() == xdp::HW_EMU && !xdp::isEdge()) {
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

  OpenCLDeviceInfoPlugin::OpenCLDeviceInfoPlugin()
    : PLDeviceOffloadPlugin()
  {
    // Software emulation currently has minimal device support for guidance

    // Since we are using xocl and xrt level objects in this plugin,
    //  we need a pointer to the shared platform to make sure the
    //  xrt_xocl::device objects aren't destroyed before we get a chance
    //  to access the information we need.
    platform = xocl::get_shared_platform() ;
  }

  OpenCLDeviceInfoPlugin::~OpenCLDeviceInfoPlugin()
  {
    if (VPDatabase::alive())
      db->unregisterPlugin(this) ;
  }

  // readTrace can be called from either the destructor or from a broadcast
  //  message from another plugin that needs the trace updated before it can
  //  progress.
  void OpenCLDeviceInfoPlugin::readTrace()
  {
    // Intentionally left blank so we don't call the base class function
    //  when called and we don't want to actually do anything
  }

  void OpenCLDeviceInfoPlugin::writeAll(bool openNewFiles)
  {
    // Intentionally left blank so we don't call the base class function
    //  when called and we don't want to actually do anything
  }

  // This function will only be called if an active device is going to
  //  be reprogrammed.  We can assume the device is good before the call
  //  and bad after this call (until the next update device)
  void OpenCLDeviceInfoPlugin::flushDevice(void* d)
  {
    // Intentionally left blank so we don't call the base class function.
    //  This plugin no longer communicates with the actual device so
    //  there is no information to be flushed.
  }

  void OpenCLDeviceInfoPlugin::updateDevice(void* d)
  {
    if (getFlowMode() == SW_EMU){
      updateSWEmulationGuidance() ;
      return ;
    }

    // The OpenCL level expects an xrt_xocl::device to be passed in
    xrt_xocl::device* device = static_cast<xrt_xocl::device*>(d) ;

    std::string path = debugIPLayoutPath(device) ;
    if (path == "")
      return ;

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
    //  be needed later.  OpenCL device info does not require a connection
    //  to the physical PL part of the device.
    (db->getStaticInfo()).updateDeviceFromHandle(deviceId, nullptr, device->get_xcl_handle()) ;
    (db->getStaticInfo()).setDeviceName(deviceId, device->getName()) ;
  }

  void OpenCLDeviceInfoPlugin::updateOpenCLInfo(uint64_t deviceId)
  {
    // The argument list is now available due to the parsing of the
    // SYSTEM_METADATA section, so we don't need to update it based off of
    // OpenCL metadata.
  }

  void OpenCLDeviceInfoPlugin::updateSWEmulationGuidance()
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
        for (const auto& arg : cu->get_args()) {
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
