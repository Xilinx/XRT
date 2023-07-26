// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include <boost/algorithm/string.hpp>
#include "ReportDynamicRegion.h"
#include "Table2D.h"

#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

namespace qr = xrt_core::query;

void
ReportDynamicRegion::getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportDynamicRegion::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  xrt::device device(_pDevice->get_device_id());
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::dynamic_regions>();
  boost::property_tree::read_json(ss, _pt);
}

void 
ReportDynamicRegion::writeReport( const xrt_core::device* _pDevice,
                       const boost::property_tree::ptree& _pt, 
                       const std::vector<std::string>& /*_elementsFilter*/,
                       std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;
  boost::format cuFmt("    %-8s%-50s%-16s%-8s%-8s\n");

  //check if a valid CU report is generated
  const boost::property_tree::ptree& pt_dfx = _pt.get_child("dynamic_regions", empty_ptree);

  const auto device_status = xrt_core::device_query_default<xrt_core::query::device_status>(_pDevice, 2);
  _output << boost::format("  Device Status: %s\n") % xrt_core::query::device_status::parse_status(device_status);
  if(pt_dfx.empty()) {
    _output << boost::format("  No hardware contexts running on device\n\n");
    return;
  }


  for(auto& k_dfx : pt_dfx) {
    const boost::property_tree::ptree& dfx = k_dfx.second;
    _output << boost::format("  Hardware Context ID: %s\n") % dfx.get<std::string>("id", "N/A");
    
    _output << boost::format("    Xclbin UUID: %s\n") % dfx.get<std::string>("xclbin_uuid", "N/A");
    const std::vector<Table2D::HeaderData> pl_table_headers = {
      {"Index", Table2D::Justification::left},
      {"Name", Table2D::Justification::left},
      {"Base Address", Table2D::Justification::left},
      {"Usage", Table2D::Justification::left},
      {"Status", Table2D::Justification::left}
    };
    Table2D pl_table(pl_table_headers);
    const std::vector<Table2D::HeaderData> ps_table_headers = {
      {"Index", Table2D::Justification::left},
      {"Name", Table2D::Justification::left},
      {"Usage", Table2D::Justification::left},
      {"Status", Table2D::Justification::left}
    };
    Table2D ps_table(ps_table_headers);

    const boost::property_tree::ptree& pt_cu = dfx.get_child("compute_units", empty_ptree);
    // Sort compute units into PL and PS groups
    try {
      int index = 0;
      for(auto& kv : pt_cu) {
        const boost::property_tree::ptree& cu = kv.second;
        const std::string cu_status = cu.get_child("status").get<std::string>("bit_mask");
        const uint32_t status_val = std::stoul(cu_status, nullptr, 16);

        if(boost::iequals(cu.get<std::string>("type"), "PL")) {
          const std::vector<std::string> entry_data = {std::to_string(index++), cu.get<std::string>("name"), cu.get<std::string>("base_address") , cu.get<std::string>("usage"), xrt_core::utils::parse_cu_status(status_val)};
          pl_table.addEntry(entry_data);
        }
        else if(boost::iequals(cu.get<std::string>("type"), "PS")) {
          const std::vector<std::string> entry_data = {std::to_string(index++), cu.get<std::string>("name"), cu.get<std::string>("usage"), xrt_core::utils::parse_cu_status(status_val)};
          ps_table.addEntry(entry_data);
        }
      }
    }
    catch( std::exception const& e) {
      _output << "ERROR: " <<  e.what() << std::endl;
    }

    if (!pl_table.empty()) {
      _output << "    PL Compute Units\n";
      _output << pl_table.toString("      ") << "\n";
    }

    if (!ps_table.empty()) {
      _output << "    PS Compute Units\n";
      _output << ps_table.toString("      ") << "\n";
    }
  }

  _output << std::endl;
}
