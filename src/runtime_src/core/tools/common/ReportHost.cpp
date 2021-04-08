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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportHost.h"
#include "XBUtilities.h"
#include "core/common/system.h"

// 3rd Party Library - Include Files
#include <string>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#define BYTES_TO_MEGABYTES 0x100000ll

void
ReportHost::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                     boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void
ReportHost::getPropertyTree20202( const xrt_core::device * /*_pDevice*/,
                                  boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree pt_os_info;
  boost::property_tree::ptree pt_xrt_info;

  xrt_core::get_os_info(pt_os_info);
  pt.add_child("os", pt_os_info);

  xrt_core::get_xrt_info(pt_xrt_info);
  pt.add_child("xrt", pt_xrt_info);

  auto dev_pt = XBUtilities::get_available_devices(m_is_user);
  pt.add_child("devices", dev_pt);

  // There can only be 1 root node
  _pt.add_child("host", pt);
}


void
ReportHost::writeReport(const xrt_core::device * _pDevice,
                        const std::vector<std::string> & /*_elementsFilter*/,
                        std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "System Configuration\n";
  try {
    _output << boost::format("  %-20s : %s\n") % "OS Name" % _pt.get<std::string>("host.os.sysname");
    _output << boost::format("  %-20s : %s\n") % "Release" % _pt.get<std::string>("host.os.release");
    _output << boost::format("  %-20s : %s\n") % "Version" % _pt.get<std::string>("host.os.version");
    _output << boost::format("  %-20s : %s\n") % "Machine" % _pt.get<std::string>("host.os.machine");
    _output << boost::format("  %-20s : %s\n") % "CPU Cores" % _pt.get<std::string>("host.os.cores");
    _output << boost::format("  %-20s : %lld MB\n") % "Memory" % (std::strtoll(_pt.get<std::string>("host.os.memory_bytes").c_str(),nullptr,16) / BYTES_TO_MEGABYTES);
    _output << boost::format("  %-20s : %s\n") % "Distribution" % _pt.get<std::string>("host.os.distribution","N/A");
    boost::property_tree::ptree& available_libraries = _pt.get_child("host.os.libraries", empty_ptree);
    for(auto& kl : available_libraries) {
      boost::property_tree::ptree& lib = kl.second;
      std::string lib_name = lib.get<std::string>("name", "N/A");
      boost::algorithm::to_upper(lib_name);
      _output << boost::format("  %-20s : %s\n") % lib_name
          % lib.get<std::string>("version", "N/A");
    }
    _output << boost::format("  %-20s : %s\n") % "Model" % _pt.get<std::string>("host.os.model");
    _output << std::endl;
    _output << "XRT\n";
    _output << boost::format("  %-20s : %s\n") % "Version" % _pt.get<std::string>("host.xrt.version", "N/A");
    _output << boost::format("  %-20s : %s\n") % "Branch" % _pt.get<std::string>("host.xrt.branch", "N/A");
    _output << boost::format("  %-20s : %s\n") % "Hash" % _pt.get<std::string>("host.xrt.hash", "N/A");
    _output << boost::format("  %-20s : %s\n") % "Hash Date" % _pt.get<std::string>("host.xrt.build_date", "N/A");
    boost::property_tree::ptree& available_drivers = _pt.get_child("host.xrt.drivers", empty_ptree);
    for(auto& drv : available_drivers) {
      boost::property_tree::ptree& driver = drv.second;
      std::string drv_name = driver.get<std::string>("name", "N/A");
      boost::algorithm::to_upper(drv_name);
      _output << boost::format("  %-20s : %s, %s\n") % drv_name
          % driver.get<std::string>("version", "N/A") % driver.get<std::string>("hash", "N/A");
    }
    _output << std::endl;
  }
  catch (const boost::property_tree::ptree_error &ex) {
    throw xrt_core::error(boost::str(boost::format("%s. Please contact your Xilinx representative to fix the issue")
         % ex.what()));
  }

  _output << "Devices present\n";
  boost::property_tree::ptree& available_devices = _pt.get_child("host.devices", empty_ptree);

  if(available_devices.empty())
    _output << "  0 devices found" << std::endl;
  
  for(auto& kd : available_devices) {
    boost::property_tree::ptree& dev = kd.second;
    std::string note = dev.get<bool>("is_ready") ? "" : "NOTE: Device not ready for use";
    _output << boost::format("  [%s] : %s %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv") % note;
  }
  _output << std::endl;


}
