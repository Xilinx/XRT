// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportElectrical.h"
#include "Table2D.h"

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
  _output << boost::format("  %-23s: %s Watts\n") % "Max Power" % _pt.get<std::string>("electrical.power_consumption_max_watts", "N/A");
  _output << boost::format("  %-23s: %s Watts\n") % "Power" % _pt.get<std::string>("electrical.power_consumption_watts", "N/A");
  _output << boost::format("  %-23s: %s\n\n") % "Power Warning" % _pt.get<std::string>("electrical.power_consumption_warning", "N/A");
  
  const std::vector<Table2D::HeaderData> table_headers = {
    {"Power Rails", Table2D::Justification::left},
    {":", Table2D::Justification::left},
    {"Voltage", Table2D::Justification::right},
    {"Current", Table2D::Justification::right}
  };
  Table2D elec_table(table_headers);

  for(auto& kv : electricals) {
    const boost::property_tree::ptree& pt_sensor = kv.second;
    const auto voltage = pt_sensor.get<std::string>("voltage.volts", "N/A");
    const auto amps = pt_sensor.get<std::string>("current.amps", "N/A");

    const std::vector<std::string> entry_data = {
      pt_sensor.get<std::string>("description"),
      ":",
      (voltage == "N/A") ? voltage : voltage + " V",
      (amps == "N/A") ? amps : amps + " A",
    };
    elec_table.addEntry(entry_data);
  }

  if (elec_table.empty())
    _output << "  No electrical sensors found\n";
  else
    _output << elec_table.toString("  ");

  _output << std::endl;

}
