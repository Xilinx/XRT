/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (c) 2022 Advanced Micro Devices, Inc.
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
  boost::property_tree::ptree cmc_tree;
  cmc_tree.put("Description", "CMC");

  try {
    boost::property_tree::ptree heartbeat_data;
    heartbeat_data.put("Description", "CMC heartbeat");
    heartbeat_data.put("heartbeat_err_time", xrt_core::device_query<xrt_core::query::heartbeat_err_time>(_pDevice));
    heartbeat_data.put("heartbeat_count", xrt_core::device_query<xrt_core::query::heartbeat_count>(_pDevice));
    heartbeat_data.put("heartbeat_err_code", xrt_core::device_query<xrt_core::query::heartbeat_err_code>(_pDevice));
    heartbeat_data.put("heartbeat_stall", xrt_core::device_query<xrt_core::query::heartbeat_stall>(_pDevice));
    heartbeat_data.put("status", xrt_core::utils::parse_cmc_status(static_cast<unsigned int>(xrt_core::device_query<xrt_core::query::heartbeat_err_code>(_pDevice))));
    cmc_tree.add_child("cmc_heartbeat", heartbeat_data);
  }
  catch(const xrt_core::query::no_such_key&) {}
  catch(const xrt_core::query::sysfs_error&) {}

  boost::property_tree::ptree runtime_tree;
  runtime_tree.put("Description", "Runtime Clock Scaling");
  try {
    runtime_tree.put("enabled", xrt_core::device_query<xrt_core::query::xmc_scaling_enabled>(_pDevice));
    runtime_tree.put("supported", xrt_core::device_query<xrt_core::query::xmc_scaling_support>(_pDevice));
    boost::property_tree::ptree shutdown_data;
    shutdown_data.put("power_watts", xrt_core::device_query<xrt_core::query::xmc_scaling_critical_pow_threshold>(_pDevice));
    shutdown_data.put("temp_celsius", xrt_core::device_query<xrt_core::query::xmc_scaling_critical_temp_threshold>(_pDevice));
    runtime_tree.add_child("shutdown_threshold_limits", shutdown_data);
    boost::property_tree::ptree override_data;
    override_data.put("power_watts", xrt_core::device_query<xrt_core::query::xmc_scaling_threshold_power_limit>(_pDevice));
    override_data.put("temp_celsius", xrt_core::device_query<xrt_core::query::xmc_scaling_threshold_temp_limit>(_pDevice));
    runtime_tree.add_child("override_threshold_limits", override_data);
    boost::property_tree::ptree power_threshold_data;
    power_threshold_data.put("enabled", xrt_core::device_query<xrt_core::query::xmc_scaling_power_override_enable>(_pDevice));
    power_threshold_data.put("power_watts", xrt_core::device_query<xrt_core::query::xmc_scaling_power_override>(_pDevice));
    runtime_tree.add_child("power_threshold_override", power_threshold_data);
    boost::property_tree::ptree temp_threshold_data;
    temp_threshold_data.put("enabled", xrt_core::device_query<xrt_core::query::xmc_scaling_temp_override_enable>(_pDevice));
    temp_threshold_data.put("temp_celsius", xrt_core::device_query<xrt_core::query::xmc_scaling_temp_override>(_pDevice));
    runtime_tree.add_child("temp_threshold_override", temp_threshold_data);
  }
  catch(const xrt_core::query::no_such_key&) {}
  catch(const xrt_core::query::sysfs_error&) {}
  cmc_tree.add_child("scaling", runtime_tree);

  // There can only be 1 root node
  _pt.add_child("cmc", cmc_tree);
}

void
ReportCmcStatus::writeReport( const xrt_core::device* /*_pDevice*/,
                            const boost::property_tree::ptree& _pt,
                            const std::vector<std::string>& /*_elementsFilter*/,
                            std::ostream & _output) const
{
  _output << "CMC\n";
  auto& cmc = _pt.get_child("cmc");
  if(cmc.empty()) {
    _output << "  Information unavailable" << std::endl;
    return;
  }

  try {
    boost::property_tree::ptree cmc_hb = cmc.get_child("cmc_heartbeat");
    uint32_t err_code = cmc_hb.get<uint32_t>("heartbeat_err_code");
    _output << boost::format("  %s : 0x%x %s\n") % "Status" % err_code % cmc_hb.get<std::string>("status");
    if (err_code)
      _output << boost::format("  %s : %s sec\n\n") % "err time" % cmc_hb.get<std::string>("heartbeat_err_time");
  } catch(...) {
    _output << "  Heartbeat information unavailable\n";
  }

  try {
    boost::property_tree::ptree cmc_scale = cmc.get_child("scaling");
    _output << boost::format("  %-22s:\n") % cmc_scale.get<std::string>("Description");
    if (!cmc_scale.get<bool>("supported")) {
      _output << "    Information unavailable\n";
      return;
    }
    if (!cmc_scale.get<bool>("enabled")) {
      _output << "    Not enabled\n";
      return;
    }

    cmc_scale = cmc.get_child("scaling").get_child("shutdown_threshold_limits");
    _output << boost::format("    %-22s:\n") % "Critical threshold (clock shutdown) limits";
    _output << boost::format("      %s : %s W\n") % "Power" % cmc_scale.get<std::string>("power_watts");
    _output << boost::format("      %s : %s C\n") % "Temperature" % cmc_scale.get<std::string>("temp_celsius");
    cmc_scale = cmc.get_child("scaling").get_child("override_threshold_limits");
    _output << boost::format("    %-22s:\n") % "Throttling threshold limits";
    _output << boost::format("      %s : %s W\n") % "Power" % cmc_scale.get<std::string>("power_watts");
    _output << boost::format("      %s : %s C\n") % "Temperature" % cmc_scale.get<std::string>("temp_celsius");
    cmc_scale = cmc.get_child("scaling").get_child("power_threshold_override");
    _output << boost::format("    %-22s:\n") % "Power threshold override";
    _output << boost::format("      %s : %s\n") % "Override" % cmc_scale.get<std::string>("enabled");
    _output << boost::format("      %s : %s W\n") % "Override limit" % cmc_scale.get<std::string>("power_watts");
    cmc_scale = cmc.get_child("scaling").get_child("temp_threshold_override");
    _output << boost::format("    %-22s:\n") % "Temperature threshold override";
    _output << boost::format("      %s : %s\n") % "Override" % cmc_scale.get<std::string>("enabled");
    _output << boost::format("      %s : %s C\n") % "Override limit" % cmc_scale.get<std::string>("temp_celsius");
  } catch(...) {
    _output << "    Information unavailable\n";
  }
}
