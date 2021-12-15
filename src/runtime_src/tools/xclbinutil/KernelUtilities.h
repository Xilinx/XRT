/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef __KernelUtilities_h_
#define __KernelUtilities_h_

// Include files
#include <boost/property_tree/ptree.hpp>

namespace XclBinUtilities {
  void addKernel(const boost::property_tree::ptree & ptKernel, boost::property_tree::ptree & ptEmbeddedData);
  void addKernel(const boost::property_tree::ptree & ptKernel, const boost::property_tree::ptree & ptMemTopology, boost::property_tree::ptree & ptIPLayout, boost::property_tree::ptree & ptConnectivity);
};
#endif
