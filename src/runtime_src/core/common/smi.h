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
#include <memory>
#include <map>

namespace xrt_core::smi {

using tuple_vector = std::vector<std::tuple<std::string, std::string, std::string>>; 

// This is a basic option structure that contains the name, description, and type of the option.
struct basic_option {
  std::string m_name;
  std::string m_description;
  std::string m_type;
};

class option : public basic_option {
  std::string m_alias;
  std::string m_default_value;
  std::string m_value_type;

  // OptionOption are special type of options that can have further suboptions.
  bool b_is_optionOption;

public:

  option(std::string name, 
         std::string alias, 
         std::string description,
         std::string type, 
         std::string default_value, 
         std::string value_type, 
         bool is_optionOption = false)
      : basic_option{std::move(name), std::move(description), std::move(type)}, 
        m_alias(std::move(alias)), 
        m_default_value(std::move(default_value)), 
        m_value_type(std::move(value_type)),
        b_is_optionOption(is_optionOption) {} 

  virtual 
  ~option() = default; 

  XRT_CORE_COMMON_EXPORT
  virtual 
  boost::property_tree::ptree 
  to_ptree() const;

  // Default implementation throws an error 
  virtual tuple_vector
  get_description_array() const { 
    throw std::runtime_error("Illegal call to get_description_array()");
   } 

  bool 
  get_is_optionOption() const { 
    return b_is_optionOption; 
  }
};

// This class is used to represent an option with a multiline description.
// For example, --run can have multiple test names as its description.
// These subnames are also queried using the generic API get_list
class listable_description_option : public option {
  std::vector<basic_option> m_description_array;
public:
  listable_description_option(std::string name, 
                              std::string alias, 
                              std::string description,
                              std::string type, 
                              std::string default_value, 
                              std::string value_type, 
                              std::vector<basic_option> description_array)
      : option(std::move(name), std::move(alias), std::move(description), 
               std::move(type), std::move(default_value), std::move(value_type)), 
        m_description_array(std::move(description_array)) {}

  XRT_CORE_COMMON_EXPORT
  boost::property_tree::ptree 
  to_ptree() const override;

  XRT_CORE_COMMON_EXPORT
  tuple_vector
  get_description_array() const override; 
};

class subcommand {
  std::string m_name;
  std::string m_description;
  std::string m_type;
  std::map<std::string, std::shared_ptr<option>> m_options;

public:

  const std::map<std::string, std::shared_ptr<option>>&
  get_options() const 
  { return m_options; }

  tuple_vector
  get_option_options() const; 

  boost::property_tree::ptree 
  construct_subcommand_json() const;

  subcommand(std::string name, 
             std::string description, 
             std::string type, 
             std::map<std::string, std::shared_ptr<option>> options)
      : m_name(std::move(name)), 
        m_description(std::move(description)), 
        m_type(std::move(type)), 
        m_options(std::move(options)) {}
};

// This class provides utilities for xrt-smi specific option/subcommand handling.
// Each shim (including smi_default) should create objects of smi class and populate
// them with their custom fields. Currently, shims create singleton instanced of it. 
class smi {
  std::map<std::string, subcommand> m_subcommands;

public:
  void
  add_subcommand(std::string name, subcommand subcmd) {
    m_subcommands.emplace(std::move(name), std::move(subcmd));
  }

  XRT_CORE_COMMON_EXPORT
  std::string
  build_smi_config() const;

  XRT_CORE_COMMON_EXPORT
  tuple_vector
  get_list(const std::string& subcommand, const std::string& suboption) const;

  XRT_CORE_COMMON_EXPORT
  tuple_vector
  get_option_options(const std::string& subcommand) const;

};

XRT_CORE_COMMON_EXPORT
smi*
instance();

XRT_CORE_COMMON_EXPORT
tuple_vector
get_list(const std::string& subcommand, const std::string& suboption);

XRT_CORE_COMMON_EXPORT
tuple_vector
get_option_options(const std::string& subcommand);

} // namespace xrt_core::smi
