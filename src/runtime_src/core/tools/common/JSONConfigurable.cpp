// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "tools/common/JSONConfigurable.h"
#include "core/common/error.h"

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
namespace po = boost::program_options;

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
            output.push_back(std::make_pair("", pt));
        }
      }
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(e.what());
  }
  return output;
}

boost::property_tree::ptree
JSONConfigurable::parse_configuration_tree(
  const std::vector<std::string>& targets,
  const boost::property_tree::ptree& configuration)
{
  const std::vector<std::string> currentDeviceCategories = {"common", "alveo", "aie"};
  auto cmdTrees = parse_configuration(currentDeviceCategories, configuration);
  return parse_configuration(targets, cmdTrees);
}
