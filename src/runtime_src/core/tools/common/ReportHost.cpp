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
#include "core/common/system.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>

void
ReportHost::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                     boost::property_tree::ptree &_pt) const
{
  // Defer to the 20201 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20201(_pDevice, _pt);
}

void 
ReportHost::getPropertyTree20201( const xrt_core::device * /*_pDevice*/, 
                                  boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree pt_os_info;
  boost::property_tree::ptree pt_xrt_info;

  xrt_core::get_os_info(pt_os_info);
  pt.add_child("os", pt_os_info);

  xrt_core::get_xrt_info(pt_xrt_info);
  pt.add_child("xrt", pt_xrt_info);

  // There can only be 1 root node
  _pt.add_child("host", pt);
}


void 
ReportHost::writeReport(const xrt_core::device * _pDevice,
                        const std::vector<std::string> & /*_elementsFilter*/, 
                        std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "System Configuration\n";
  _output << boost::format("  %-20s : %s\n") % "OS Name" % _pt.get<std::string>("host.os.sysname", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Release" % _pt.get<std::string>("host.os.release", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Version" % _pt.get<std::string>("host.os.version", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Machine" % _pt.get<std::string>("host.os.machine", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Glibc" % _pt.get<std::string>("host.os.glibc", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Distribution" % _pt.get<std::string>("host.os.linux", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Now" % _pt.get<std::string>("host.os.now", "N/A");
  _output << std::endl;
  _output << "XRT\n";
  _output << boost::format("  %-20s : %s\n") % "Version" % _pt.get<std::string>("host.xrt.version", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Branch" % _pt.get<std::string>("host.xrt.branch", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Hash" % _pt.get<std::string>("host.xrt.hash", "N/A");
  _output << boost::format("  %-20s : %s\n") % "Hash Date" % _pt.get<std::string>("host.xrt.date", "N/A");
  _output << boost::format("  %-20s : %s\n") % "XOCL" % _pt.get<std::string>("host.xrt.xocl", "N/A");
  _output << boost::format("  %-20s : %s\n") % "XCLMGMT" % _pt.get<std::string>("host.xrt.xclmgmt", "N/A");
  _output << std::endl;

}


