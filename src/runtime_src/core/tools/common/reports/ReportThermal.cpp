// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportThermal.h"
#include "core/common/device.h"
#include "core/common/sensor.h"
#include "tools/common/Table2D.h"

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
  _pt = xrt_core::sensor::read_thermals(_pDevice);
}

void
ReportThermal::writeReport( const xrt_core::device* /*_pDevice*/,
                            const boost::property_tree::ptree& _pt,
                            const std::vector<std::string>& /*_elementsFilter*/,
                            std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "Thermals\n";
  const boost::property_tree::ptree& thermals = _pt.get_child("thermals", empty_ptree);

  const std::vector<Table2D::HeaderData> table_headers = {
    {"Temperature", Table2D::Justification::left},
    {"Celcius", Table2D::Justification::left}
  };
  Table2D temp_table(table_headers);

  for(auto& kv : thermals) {
    const boost::property_tree::ptree& pt_temp = kv.second;
    if(!pt_temp.get<bool>("is_present", false))
      continue;

    const std::vector<std::string> entry_data = {pt_temp.get<std::string>("description"), pt_temp.get<std::string>("temp_C")};
    temp_table.addEntry(entry_data);
  }

  if(temp_table.empty())
    _output << "  No temperature sensors are present" << std::endl;
  else
    _output << temp_table.toString("  ") << std::endl;

  _output << std::endl;
}
