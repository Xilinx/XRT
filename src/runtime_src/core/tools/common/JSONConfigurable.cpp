// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "tools/common/JSONConfigurable.h"
#include "core/common/error.h"

// Find children corresponding to target(s) and return them in a ptree.
boost::property_tree::ptree
JSONConfigurable::parse_configuration(
  const std::vector<std::string>& targets,
  const boost::property_tree::ptree& configuration)
{
  boost::property_tree::ptree output;
  try {
    for (const std::string& target: targets) {
      for (const auto& configTree : configuration) {
        for (const auto& config : configTree.second.get_child("contents")) {
          const auto& pt = config.second;
          if (boost::iequals(target, pt.get<std::string>("name")))
            output.push_back({ configTree.second.get<std::string>("name"), pt });
        }
      }
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(e.what());
  }
  return output;
}

// Parse configuration for children corresponding to current device categories before parsing for target(s).
boost::property_tree::ptree
JSONConfigurable::parse_configuration_tree(
  const std::vector<std::string>& targets,
  const boost::property_tree::ptree& configuration)
{
  // currentDeviceCategories must be updated to use a query for device categories of cards present.
  const std::vector<std::string> currentDeviceCategories = {"common", "alveo", "aie"};
  auto cmdTrees = parse_configuration(currentDeviceCategories, configuration);
  return parse_configuration(targets, cmdTrees);
}
