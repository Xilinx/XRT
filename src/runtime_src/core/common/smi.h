// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
// Local include files
#include "config.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>

#include <string>
#include <tuple>
#include <vector>

namespace xrt_core::smi {

using tuple_vector = std::vector<std::tuple<std::string, std::string, std::string>>; 

struct basic_option {
  std::string name;
  std::string description;
  std::string type;
};

struct option : public basic_option {
  std::string alias;
  std::string default_value;
  std::string value_type;
  std::vector<basic_option> description_array;

  option(const std::string name, 
         const std::string alias, 
         const std::string description,
         const std::string type, 
         const std::string default_value, 
         const std::string value_type, 
         const std::vector<basic_option>& description_array = {})
      : basic_option{std::move(name), std::move(description), std::move(type)}, 
        alias(std::move(alias)), 
        default_value(std::move(default_value)), 
        value_type(std::move(value_type)), 
        description_array(std::move(description_array)) {}

  boost::property_tree::ptree to_ptree() const;
};

// Each shim's smi class derives from this class
// and adds its custom functionalities. Currently only validate tests and examine
// reports differ between each shim but going forward, each shim can define its 
// custom behavior for xrt-smi as required. This also gives us the flexibility
// to add device specific xrt-smi behavior.
class smi_base {
protected:

  tuple_vector validate_test_desc;
  tuple_vector examine_report_desc;

  std::vector<basic_option> 
  construct_option_description(const tuple_vector&) const;

  boost::property_tree::ptree 
  construct_validate_subcommand() const;

  boost::property_tree::ptree 
  construct_examine_subcommand() const;

  boost::property_tree::ptree 
  construct_configure_subcommand() const;

public:
  XRT_CORE_COMMON_EXPORT
  std::string
  get_smi_config() const;

  XRT_CORE_COMMON_EXPORT
  const tuple_vector&
  get_validate_tests() const
  { return validate_test_desc; }

  XRT_CORE_COMMON_EXPORT
  const tuple_vector&
  get_examine_reports() const
  { return examine_report_desc; }

  XRT_CORE_COMMON_EXPORT
  smi_base();
};

XRT_CORE_COMMON_EXPORT
std::string get_smi_config();

} // namespace xrt_core::smi
