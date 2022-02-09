/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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
#include "ReportMechanical.h"

#include "core/common/sensor.h"

void
ReportMechanical::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                           boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void
ReportMechanical::getPropertyTree20202( const xrt_core::device * _pDevice,
                                        boost::property_tree::ptree &_pt) const
{
  // There can only be 1 root node
  _pt.add_child("mechanical", xrt_core::sensor::read_mechanical(_pDevice));
}

void
ReportMechanical::writeReport( const xrt_core::device* /*_pDevice*/,
                               const boost::property_tree::ptree& _pt,
                               const std::vector<std::string>& /*_elementsFilter*/,
                               std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "Mechanical\n";
  _output << "  Fans\n";
  const boost::property_tree::ptree& fans = _pt.get_child("mechanical.fans", empty_ptree);
  for(auto& kv : fans) {
    const boost::property_tree::ptree& pt_fan = kv.second;
    if(!pt_fan.get<bool>("is_present", false)) {
      _output << "    Not present"  << std::endl;
      continue;
    }
    _output << boost::format("    %-10s\n") % pt_fan.get<std::string>("description");
    _output << boost::format("      %-22s: %s C\n") % "Critical Trigger Temp" % pt_fan.get<std::string>("critical_trigger_temp_C");
    _output << boost::format("      %-22s: %s RPM\n") % "Speed" % pt_fan.get<std::string>("speed_rpm");
  }
  _output << std::endl;
}
