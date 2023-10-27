// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "tools/common/JSONConfigurable.h"
#include "core/common/error.h"

#include <iostream>
#include <set>

const std::map<std::string, std::string>
JSONConfigurable::device_type_map = {
  {"aie", "AIE"},
  {"alveo", "Alveo"},
  {"common", "common"}
};

// Parse configuration for children corresponding to current device categories before parsing for target(s).
std::map<std::string, boost::property_tree::ptree>
JSONConfigurable::parse_configuration_tree(
  const boost::property_tree::ptree& configuration)
{
  std::map<std::string, boost::property_tree::ptree> target_mappings;
  // ptree parsing requires two for loops when iterating through an array
  // One to access the current element. A second to access the data within said element.
  for (const auto& device_config_tree : configuration) {
    for (const auto& [device_name, device_config] : device_config_tree.second) {
      for (const auto& command_config_tree : device_config) {
        for (const auto& [command_name, command_config] : command_config_tree.second) {
          target_mappings[command_name].push_back({device_name, command_config});
        }
      }
    }
  }

  return target_mappings;
}

std::set<std::shared_ptr<JSONConfigurable>>
JSONConfigurable::extract_common_options(std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>> device_options)
{
  std::set<std::shared_ptr<JSONConfigurable>> common_options;
  std::map<std::shared_ptr<JSONConfigurable>, int> config_map;
  int deviceClassCount = 0;
  for (const auto& options_pair : device_options) {
    for (const auto& option : options_pair.second) {
      const auto it = config_map.find(option);
      if (it == config_map.end())
        config_map.emplace(option, 1);
      else
        it->second++;
    }
    deviceClassCount++;
  }

  for (const auto& config_pair : config_map) {
    if (config_pair.second == deviceClassCount)
      common_options.emplace(config_pair.first);
  }

  return common_options;
}
