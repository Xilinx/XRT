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

#ifndef __XBReport_h_
#define __XBReport_h_

// Include files
// Please keep these to the bare minimum
#include <boost/property_tree/ptree.hpp>

namespace XBReport {
  void report_thermal_devices(const std::vector<uint16_t>& device_indices, 
                                boost::property_tree::ptree& pt, bool json);
  void report_fans_devices(const std::vector<uint16_t>& device_indices, 
                                boost::property_tree::ptree& pt, bool json);
  void report_electrical_devices(const std::vector<uint16_t>& device_indices, 
                                boost::property_tree::ptree& pt, bool json);
  void report_shell_on_devices(const std::vector<uint16_t>& device_indices, 
                                boost::property_tree::ptree& pt, bool json);

};

#endif

