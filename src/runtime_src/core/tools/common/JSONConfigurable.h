/**
 * Copyright (C) 2020 Xilinx, Inc
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
  public:
    JSONConfigurable() {};

    virtual const std::string& get_name() const = 0;

    // template <class T, class = std::enable_if_t<std::is_base_of_v<JSONConfigurable, T>>>
    // static std::vector<T>
    // extractMatchingConfigurations(const std::vector<T>& items, const boost::property_tree::ptree& configuration)
    // {
    //   std::vector<T> output;
    //   for (const auto& item : items) {
    //     const std::string device_category = item.get_device_category();
    //     boost::property_tree::ptree::value_type subtree;
    //     try {
    //       subtree = configuration.get_child(device_category);
    //     } catch (const std::exception& e) {
    //       throw std::runtime_error("Error: No JSON branch for '" + device_category + "'\n" + e.what());
    //     }
    //     if (!device_category.compare(subtree.first))
    //       output.push_back(item);
    //   }
    //   return output;
    // }
};

std::vector<boost::property_tree::ptree> 
parse_configuration_tree( const std::vector<std::string>& targets,
                          const boost::property_tree::ptree& configuration);

#endif
