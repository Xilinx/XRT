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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportFirewall.h"
#include "core/common/query_requests.h"
#include "core/common/utils.h"

void
ReportFirewall::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                         boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportFirewall::getPropertyTree20202( const xrt_core::device * _pDevice,
                                       boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  try {
    pt.put("Description","Firewall Information");
    pt.put("firewall_level", xrt_core::device_query<xrt_core::query::firewall_detect_level>(_pDevice));
    pt.put("firewall_status", boost::format("0x%x") % xrt_core::device_query<xrt_core::query::firewall_detect_level>(_pDevice));
    pt.put("status", xrt_core::utils::parse_firewall_status(static_cast<unsigned int>(xrt_core::device_query<xrt_core::query::firewall_detect_level>(_pDevice))));
  } catch(...) {}
  // There can only be 1 root node
  _pt.add_child("firewall", pt);
}


void 
ReportFirewall::writeReport(const xrt_core::device* /*_pDevice*/,
                            const boost::property_tree::ptree& _pt,
                            const std::vector<std::string>& /*_elementsFilter*/,
                            std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "Firewall\n";
  if (_pt.empty()) {
    _output << "  Information unavailable" << std::endl; 
    return;
  }
  _output << boost::format("  %s %d: %s %s\n\n") % "Level" 
              % _pt.get<std::string>("firewall.firewall_level", "--") 
              % _pt.get<std::string>("firewall.firewall_status", "--") 
              % _pt.get<std::string>("firewall.status", "--");

}


