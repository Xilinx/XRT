/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 */
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "query_requests.h"
#include <map>
#include <string>

#include <boost/algorithm/string.hpp>

namespace xrt_core { namespace query {

std::string
xrt_core::query::p2p_config::
to_string(xrt_core::query::p2p_config::value_type value)
{
  static std::map<xrt_core::query::p2p_config::value_type, std::string> p2p_config_map =
  {
   { xrt_core::query::p2p_config::value_type::disabled,      "disabled" },
   { xrt_core::query::p2p_config::value_type::enabled,       "enabled" },
   { xrt_core::query::p2p_config::value_type::error,         "error" },
   { xrt_core::query::p2p_config::value_type::reboot,        "reboot" },
   { xrt_core::query::p2p_config::value_type::not_supported, "not supported" },
  };

  return p2p_config_map[value];
}

std::pair<xrt_core::query::p2p_config::value_type, std::string>
xrt_core::query::p2p_config::
parse(const xrt_core::query::p2p_config::result_type& config)
{
  int64_t bar = -1;
  int64_t rbar = -1;
  int64_t remap = -1;
  int64_t exp_bar = -1;

  // parse the query
  for(const auto& val : config) {
    auto pos = val.find(':') + 1;
    if(val.find("rbar") == 0)
      rbar = std::stoll(val.substr(pos));
    else if(val.find("exp_bar") == 0)
      exp_bar = std::stoll(val.substr(pos));
    else if(val.find("bar") == 0)
      bar = std::stoll(val.substr(pos));
    else if(val.find("remap") == 0)
      remap = std::stoll(val.substr(pos));
  }

  // return the config with a message
  if (bar == -1) {
    return {xrt_core::query::p2p_config::value_type::not_supported,
            "P2P config failed. P2P is not supported. Can't find P2P BAR."};
  }
  else if (rbar != -1 && rbar > bar) {
    return {xrt_core::query::p2p_config::value_type::reboot, 
            "Warning:Please WARM reboot to enable p2p now."};
  }
  else if (remap > 0 && remap != bar) {
    return {xrt_core::query::p2p_config::value_type::error,
            "Error:P2P config failed. P2P remapper is not set correctly"};
  }
  else if (bar == exp_bar) {
    return {xrt_core::query::p2p_config::value_type::enabled, ""};
  }
  return {xrt_core::query::p2p_config::value_type::disabled,
          "P2P bar is not enabled"};
}

std::string
xrt_core::query::interface_uuids::
to_uuid_upper_string(const std::string& value)
{
  auto uuid_str = to_uuid_string(value);
  boost::to_upper(uuid_str);
  return uuid_str;
}
  
}} // query, xrt_core
