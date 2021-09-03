/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include "ReportCmcStatus.h"
#include "core/common/query_requests.h"
#include "core/common/utils.h"

void
ReportCmcStatus::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void
ReportCmcStatus::getPropertyTree20202( const xrt_core::device * _pDevice,
                                           boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  pt.put("Description", "CMC");
  try {
    pt.put("heartbeat_err_time", xrt_core::device_query<xrt_core::query::heartbeat_err_time>(_pDevice));
    pt.put("heartbeat_count", xrt_core::device_query<xrt_core::query::heartbeat_count>(_pDevice));
    pt.put("heartbeat_err_code", xrt_core::device_query<xrt_core::query::heartbeat_err_code>(_pDevice));
    pt.put("heartbeat_stall", xrt_core::device_query<xrt_core::query::heartbeat_stall>(_pDevice));
    pt.put("status", xrt_core::utils::parse_cmc_status(static_cast<unsigned int>(xrt_core::device_query<xrt_core::query::heartbeat_err_code>(_pDevice))));
  } catch(...) {}
  try {
    pt.put("scaling_enabled", xrt_core::device_query<xrt_core::query::xmc_scaling_enabled>(_pDevice));
    pt.put("scaling_support", xrt_core::device_query<xrt_core::query::xmc_scaling_support>(_pDevice));
    pt.put("scaling_critical_power_threshold", xrt_core::device_query<xrt_core::query::xmc_scaling_critical_pow_threshold>(_pDevice));
    pt.put("scaling_critical_temp_threshold", xrt_core::device_query<xrt_core::query::xmc_scaling_critical_temp_threshold>(_pDevice));
    pt.put("scaling_threshold_power_limit", xrt_core::device_query<xrt_core::query::xmc_scaling_threshold_power_limit>(_pDevice));
    pt.put("scaling_threshold_temp_limit", xrt_core::device_query<xrt_core::query::xmc_scaling_threshold_temp_limit>(_pDevice));
    pt.put("scaling_power_override_en", xrt_core::device_query<xrt_core::query::xmc_scaling_power_override_enable>(_pDevice));
    pt.put("scaling_power_override", xrt_core::device_query<xrt_core::query::xmc_scaling_power_override>(_pDevice));
    pt.put("scaling_temp_override_en", xrt_core::device_query<xrt_core::query::xmc_scaling_temp_override_enable>(_pDevice));
    pt.put("scaling_temp_override", xrt_core::device_query<xrt_core::query::xmc_scaling_temp_override>(_pDevice));
  } catch(const xrt_core::query::no_such_key&) {}
  // There can only be 1 root node
  _pt.add_child("cmc", pt);
}

void
ReportCmcStatus::writeReport( const xrt_core::device* /*_pDevice*/,
                            const boost::property_tree::ptree& _pt,
                            const std::vector<std::string>& /*_elementsFilter*/,
                            std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "CMC\n";
  auto& cmc = _pt.get_child("cmc");
  if(cmc.empty()) {
    _output << "  Information unavailable" << std::endl;
    return;
  }
  try {
    uint32_t err_code = cmc.get<uint32_t>("heartbeat_err_code");
    _output << boost::format("  %-22s : 0x%x %s\n") % "Status" % err_code % cmc.get<std::string>("status");
    if (err_code)
      _output << boost::format("  %-22s : %s sec\n\n") % "err time" % cmc.get<std::string>("heartbeat_err_time");
  } catch(...) {}
  _output << boost::format("  %-22s :\n") % "Runtime clock scaling feature";
  _output << boost::format("  \t%-22s : %s\n") % "Supported" % cmc.get<std::string>("scaling_support");
  _output << boost::format("  \t%-22s : %s\n") % "Enabled" % cmc.get<std::string>("scaling_enabled");
  _output << boost::format("  \t%-22s:\n") % "Critical threshold (clock shutdown) limits";
  _output << boost::format("  \t\t%-22s : %s\n") % "Power" % cmc.get<std::string>("scaling_critical_power_threshold");
  _output << boost::format("  \t\t%-22s : %s\n") % "Temperature" % cmc.get<std::string>("scaling_critical_temp_threshold");
  _output << boost::format("  \t%-22s:\n") % "Throttling threshold limits";
  _output << boost::format("  \t\t%-22s : %s\n") % "Power" % cmc.get<std::string>("scaling_threshold_power_limit");
  _output << boost::format("  \t\t%-22s : %s\n") % "Temperature" % cmc.get<std::string>("scaling_threshold_temp_limit");
  _output << boost::format("  \t%-22s:\n") % "Power threshold override";
  _output << boost::format("  \t\t%-22s : %s\n") % "Override" % cmc.get<std::string>("scaling_power_override_en");
  _output << boost::format("  \t\t%-22s : %s\n") % "Override limit" % cmc.get<std::string>("scaling_power_override");
  _output << boost::format("  \t%-22s:\n") % "Temperature threshold override";
  _output << boost::format("  \t\t%-22s : %s\n") % "Override" % cmc.get<std::string>("scaling_temp_override_en");
  _output << boost::format("  \t\t%-22s : %s\n") % "Override limit" % cmc.get<std::string>("scaling_temp_override");
}
