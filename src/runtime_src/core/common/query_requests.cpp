// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "query_requests.h"
#include "core/include/xclerr_int.h"
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

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
   { xrt_core::query::p2p_config::value_type::no_iomem,        "no iomem" },
   { xrt_core::query::p2p_config::value_type::not_supported, "not supported" },
  };

  return p2p_config_map[value];
}

std::map<std::string, int64_t>
xrt_core::query::p2p_config::
to_map(const xrt_core::query::p2p_config::result_type& config)
{
  static std::vector<std::string> config_whitelist = {"bar", "rbar", "max_bar", "exp_bar"};
  std::map<std::string, int64_t> config_map;
  for (auto& str_untrimmed : config) {
    const std::string str = boost::trim_copy(str_untrimmed);
    // str is in key:value format obtained from p2p_config query
    const auto pos = str.find(":");
    const std::string config_item_untrimmed = str.substr(0, pos);
    const std::string config_item = boost::trim_copy(config_item_untrimmed);
    if (std::find(config_whitelist.begin(), config_whitelist.end(), config_item) != config_whitelist.end()) {
      try {
        const int64_t value = std::stoll(str.substr(pos + 1));
        config_map[config_item] = value;
      } catch (const std::exception& ex) {
        // Failed to parse a non long long BAR value for a whitelisted value. Something has gone very wrong in the p2p sysfs node
        throw xrt_core::system_error(EINVAL, boost::str(boost::format("ERROR: P2P configuration failed to parse sysfs data: %s") % ex.what()));
      }
    }
  }
  return config_map;
}

std::pair<xrt_core::query::p2p_config::value_type, std::string>
xrt_core::query::p2p_config::
parse(const xrt_core::query::p2p_config::result_type& config)
{
  const auto config_map = xrt_core::query::p2p_config::to_map(config);

  // return the config with a message
  if (config_map.find("bar") == config_map.end())
    return {xrt_core::query::p2p_config::value_type::not_supported, "P2P config failed. P2P is not supported. Can't find P2P BAR."};

  if (config_map.find("rbar") != config_map.end() && config_map.at("rbar") > config_map.at("bar"))
    return {xrt_core::query::p2p_config::value_type::no_iomem, "Warning: Please WARM reboot to enable p2p now."};

  if (config_map.find("remap") != config_map.end() && config_map.at("remap") > 0 && config_map.at("remap") != config_map.at("bar"))
    return {xrt_core::query::p2p_config::value_type::error, "Error: P2P config failed. P2P remapper is not set correctly"};

  if (config_map.find("exp_bar") != config_map.end() && config_map.at("exp_bar") == config_map.at("bar"))
    return {xrt_core::query::p2p_config::value_type::enabled, ""};

  return {xrt_core::query::p2p_config::value_type::disabled, "P2P bar is not enabled"};
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

std::tuple<uint64_t, uint64_t, uint64_t>
xrt_core::query::xocl_errors::
to_ex_value(const std::vector<char>& buf, xrtErrorClass ecl)
{
  if (buf.empty())
    return { 0, 0, 0 };

  auto errors_buf = reinterpret_cast<const xcl_errors*>(buf.data());
  if (errors_buf->num_err <= 0)
    return { 0, 0, 0 };

  if (errors_buf->num_err > XCL_ERROR_CAPACITY)
    throw xrt_core::system_error(EINVAL, "Invalid data in sysfs");

  uint64_t error_code = 0;
  uint64_t time_stamp = 0;
  uint64_t ex_error_code = 0;
  for (int i = errors_buf->num_err - 1; i >= 0; i--) {
    if (XRT_ERROR_CLASS(errors_buf->errors[i].err_code) == ecl) {
      error_code = errors_buf->errors[i].err_code;
      time_stamp = errors_buf->errors[i].ts;
      ex_error_code = errors_buf->errors[i].ex_error_code;
      break;
    }
  }

  return { error_code, time_stamp, ex_error_code };
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

std::map<xrt_core::query::xclbin_slots::slot_id, xrt::uuid>
xrt_core::query::xclbin_slots::
to_map(const result_type& value)
{
  std::map<xrt_core::query::xclbin_slots::slot_id, xrt::uuid> s2u;
  for (const auto& data : value)
    s2u.emplace(data.slot, xrt::uuid{data.uuid});
  return s2u;
}

xrt_core::query::cu_read_range::range_data
xrt_core::query::cu_read_range::
to_range(const std::string& range_str)
{
  using tokenizer = boost::tokenizer< boost::char_separator<char> >;
  xrt_core::query::cu_read_range::range_data range = {0, 0};

  tokenizer tokens(range_str);
  const int radix = 16;
  tokenizer::iterator tok_it = tokens.begin();
  range.start = std::stoul(std::string(*tok_it++), nullptr, radix);
  range.end = std::stoul(std::string(*tok_it++), nullptr, radix);

  return range;
}

xrt_core::query::ert_status::ert_status_data
xrt_core::query::ert_status::
to_ert_status(const result_type& strs)
{
  using tokenizer = boost::tokenizer< boost::char_separator<char> >;
  xrt_core::query::ert_status::ert_status_data ert_status = {0};

  for (auto& line : strs) {
    // Format on each line: "<name>: <value>"
    boost::char_separator<char> sep(":");
    tokenizer tokens(line, sep);
    auto tok_it = tokens.begin();
    if (line.find("Connected:") != std::string::npos) {
      ert_status.connected = std::stoi(std::string(*(++tok_it)));
    }
  }

  return ert_status;
}

}} // query, xrt_core
