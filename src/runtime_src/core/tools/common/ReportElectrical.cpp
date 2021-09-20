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
#include "ReportElectrical.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

void
ReportElectrical::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportElectrical::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                        boost::property_tree::ptree &_pt) const
{
  xrt::device device(_pDevice->get_device_id());
  boost::property_tree::ptree pt_electrical;
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::electrical>();
  boost::property_tree::read_json(ss, pt_electrical);

  // There can only be 1 root node
  _pt.add_child("electrical", pt_electrical);
}

void 
ReportElectrical::writeReport( const xrt_core::device* /*_pDevice*/,
                               const boost::property_tree::ptree& _pt, 
                               const std::vector<std::string>& /*_elementsFilter*/,
                               std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "Electrical\n";
  const boost::property_tree::ptree& electricals = _pt.get_child("electrical.power_rails", empty_ptree);
  _output << boost::format("  %-23s: %s Watts\n") % "Max Power" % _pt.get<std::string>("electrical.power_consumption_max_watts");
  _output << boost::format("  %-23s: %s Watts\n") % "Power" % _pt.get<std::string>("electrical.power_consumption_watts");
  _output << boost::format("  %-23s: %s\n\n") % "Power Warning" % _pt.get<std::string>("electrical.power_consumption_warning");
  _output << boost::format("  %-23s: %6s   %6s\n") % "Power Rails" % "Voltage" % "Current";
  for(auto& kv : electricals) {
    const boost::property_tree::ptree& pt_sensor = kv.second;
    std::string name = pt_sensor.get<std::string>("description");
    bool volts_is_present = pt_sensor.get<bool>("voltage.is_present");
    std::string volts = pt_sensor.get<std::string>("voltage.volts");
    bool amps_is_present = pt_sensor.get<bool>("current.is_present");
    std::string amps = pt_sensor.get<std::string>("current.amps");

    if(volts_is_present && amps_is_present)
      _output << boost::format("  %-23s: %6s V, %6s A\n") % name % volts % amps;
    else if(volts_is_present)
      _output << boost::format("  %-23s: %6s V\n") % name % volts;
    else if(amps_is_present)
      _output << boost::format("  %-23s: %16s A\n") % name % amps;
  }
  _output << std::endl;
  
}
