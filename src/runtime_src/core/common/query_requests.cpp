/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 */
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "query_requests.h"
#include "core/include/xclerr_int.h"
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

std::string
xrt_core::query::oem_id::
parse(const xrt_core::query::oem_id::result_type& value)
{
  static const std::map<int, std::string> oemid_map =
  {
   {0x10da, "Xilinx"},
   {0x02a2, "Dell"},
   {0x12a1, "IBM"},
   {0xb85c, "HP"},
   {0x2a7c, "Super Micro"},
   {0x4a66, "Lenovo"},
   {0xbd80, "Inspur"},
   {0x12eb, "Amazon"},
   {0x2b79, "Google"}
  };

  try {
    constexpr int base = 16;
    unsigned int oem_id_val = std::stoul(value, nullptr, base);
    auto oemstr = oemid_map.find(oem_id_val);
    return oemstr != oemid_map.end() ? oemstr->second : "N/A";
  }
  catch (const std::exception&) {
    // conversion failed
  }
  return "N/A";
}

std::string
xrt_core::query::clock_freq_topology_raw::
parse(const std::string& clock)
{
  static const std::map<std::string, std::string> clock_map =
  {
   {"DATA_CLK", "Data"},
   {"KERNEL_CLK", "Kernel"},
   {"SYSTEM_CLK", "System"},
  };

  auto clock_str = clock_map.find(clock);
  return clock_str != clock_map.end() ? clock_str->second : "N/A";
}

std::pair<uint64_t, uint64_t>
xrt_core::query::xocl_errors::
to_value(const std::vector<char>& buf, xrtErrorClass ecl)
{
  if (buf.empty())
    return {0, 0};

  auto errors_buf = reinterpret_cast<const xcl_errors *>(buf.data());
  if (errors_buf->num_err <= 0)
    return {0, 0};

  if (errors_buf->num_err > XCL_ERROR_CAPACITY)
    throw xrt_core::system_error(EINVAL, "Invalid data in sysfs");

  for (int i = errors_buf->num_err-1; i >= 0; i--) {
    if (XRT_ERROR_CLASS(errors_buf->errors[i].err_code) == ecl)
      return {errors_buf->errors[i].err_code, errors_buf->errors[i].ts};
  }
  return {0, 0};
}

std::vector<xclErrorLast>
xrt_core::query::xocl_errors::
to_errors(const std::vector<char>& buf)
{
  if (buf.empty())
    return {};
  auto errors_buf = reinterpret_cast<const xcl_errors *>(buf.data());
  if (errors_buf->num_err <= 0)
    return {};
  if (errors_buf->num_err > XCL_ERROR_CAPACITY)
    throw xrt_core::system_error(EINVAL, "Invalid data in sysfs");

  std::vector<xclErrorLast> errors;
  for (int i = 0; i < errors_buf->num_err; i++)
    errors.emplace_back(errors_buf->errors[i]);

  return errors;
}
  
}} // query, xrt_core
