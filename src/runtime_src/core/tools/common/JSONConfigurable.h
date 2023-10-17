/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc.
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

#ifndef __JSONConfigurable_h_
#define __JSONConfigurable_h_

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

class JSONConfigurable {
private:
  static boost::property_tree::ptree
  parse_configuration( const std::vector<std::string>& targets,
                       const boost::property_tree::ptree& configuration);

public:
  JSONConfigurable() {};

  virtual const std::string& getConfigName() const = 0;

  // Parses configuration ptree and returns a map of deviceCategory to items that are in the configuration ptree.
  template <class T, class = std::enable_if_t<std::is_base_of_v<JSONConfigurable, T>>>
  static std::map<std::string, std::vector<std::shared_ptr<T>>>
  extractMatchingConfigurations(const std::vector<std::shared_ptr<T>>& items, const boost::property_tree::ptree& configuration)
  {
    std::map<std::string, std::vector<std::shared_ptr<T>>> output;
    for (const auto& relevantItem : configuration) {
      std::vector<std::shared_ptr<T>> matches;
      for (const auto& contentTree : relevantItem.second.get_child("contents")) {
        for (const auto& item : items) {
          if (boost::iequals(contentTree.second.get_value<std::string>(), item->getConfigName())) {
            matches.push_back(item);
            break;
          }
        }
      }
      output.insert(std::make_pair(relevantItem.first, matches));
    }

    return output;
  };

  static boost::property_tree::ptree
  parse_configuration_tree( const std::vector<std::string>& targets,
                            const boost::property_tree::ptree& configuration);
};

#endif
