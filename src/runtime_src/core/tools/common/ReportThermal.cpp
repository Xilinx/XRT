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
#include "ReportThermal.h"
#include "core/common/device.h"
#include "core/common/sensor.h"

#include <boost/property_tree/json_parser.hpp>

void
ReportThermal::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportThermal::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  xrt::device device(_pDevice->get_device_id());
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::thermal>();
  boost::property_tree::read_json(ss, _pt);
}

void 
ReportThermal::writeReport( const xrt_core::device* /*_pDevice*/,
                            const boost::property_tree::ptree& _pt, 
                            const std::vector<std::string>& /*_elementsFilter*/,
                            std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  bool thermals_present = false;
  _output << "Thermals\n";
  const boost::property_tree::ptree& thermals = _pt.get_child("thermals", empty_ptree);

  for(auto& kv : thermals) {
    const boost::property_tree::ptree& pt_temp = kv.second;
    if(!pt_temp.get<bool>("is_present", false))
      continue;

    thermals_present = true;
    _output << boost::format("  %-23s: %s C\n") % pt_temp.get<std::string>("description") % pt_temp.get<std::string>("temp_C");
  }

  if(!thermals_present) 
    _output << "  No temperature sensors are present" << std::endl;

  _output << std::endl;
}
