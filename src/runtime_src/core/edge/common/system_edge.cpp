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

#include "system_edge.h"

namespace xrt_core {

void
system_edge::
get_devices(boost::property_tree::ptree& pt) const
{
}

uint16_t
system_edge::
bdf2index(const std::string& bdfStr) const
{
  std::string errMsg = boost::str( boost::format("No mgmt PF found for '%s'") % bdfStr);
  throw error(errMsg);
}

} // xrt_core
