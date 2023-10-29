// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __JSONConfigurable_h_
#define __JSONConfigurable_h_

#include "XBUtilitiesCore.h"

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

/**
 * The JSONConfigurable class serves as the base class of all items
 * that are represented via the device class option JSON file. It allows
 * for centralized interpretation of the input JSON files and encourages
 * derived classes to use this class for option display.
 */
class JSONConfigurable {

private:
  static std::map<std::string, boost::property_tree::ptree>
  extract_device_configs(const boost::property_tree::ptree& config,
                         const std::string& target)
  {
    std::map<std::string, boost::property_tree::ptree> output;

    for (const auto& device_config : config) {
      for (auto& option_data_tree : device_config.second) {
        for (const auto& option_data : option_data_tree.second) {
          if (!boost::iequals(option_data.first, target))
            continue;

          output[device_config.first] = option_data.second;
        }
      }
    }

    return output;
  }

  template <class OutputType, class InputType, class = std::enable_if_t<std::is_base_of_v<JSONConfigurable, InputType>>>
  static std::map<std::string, std::vector<std::shared_ptr<OutputType>>>
  convert_device_configs(const std::map<std::string, boost::property_tree::ptree>& config,
                         const std::vector<std::shared_ptr<InputType>>& items)
  {
    std::map<std::string, std::vector<std::shared_ptr<OutputType>>> output;

    for (const auto& targetPair : config) {
      std::vector<std::shared_ptr<OutputType>> matches;
      for (const auto& option : targetPair.second) {
        for (const auto& item : items) {
          if (!boost::iequals(option.second.get_value<std::string>(), item->getConfigName()))
            continue;

          matches.push_back(item);
          break;
        }
      }
      output[targetPair.first] = matches;
    }

    return output;
  }

public:
  JSONConfigurable() {};

  virtual const std::string& getConfigName() const = 0;
  virtual const std::string& getConfigDescription() const = 0;
  virtual bool getConfigHidden() const = 0;

  static const std::map<std::string, std::string> device_type_map;

  static std::map<std::string, boost::property_tree::ptree>
  parse_configuration_tree(const boost::property_tree::ptree& configuration);

  static std::set<std::shared_ptr<JSONConfigurable>>
  extract_common_options(std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>> device_options);

  // Parses configuration ptree and returns a map of deviceCategory to items that are in the configuration ptree.
  template <class OutputType, class InputType, class = std::enable_if_t<std::is_base_of_v<JSONConfigurable, InputType>>>
  static std::map<std::string, std::vector<std::shared_ptr<OutputType>>>
  extract_subcmd_config(const std::vector<std::shared_ptr<InputType>>& items,
                        const std::map<std::string, boost::property_tree::ptree>& configuration,
                        const std::string& subcommand,
                        const std::string& target)
  {
    auto it = configuration.find(subcommand);
    if (it == configuration.end())
      return {};

    const auto device_configs = extract_device_configs(it->second, target);
    return convert_device_configs<OutputType, InputType>(device_configs, items);
  };
};

#endif
