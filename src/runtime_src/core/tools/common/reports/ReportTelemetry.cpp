/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportTelemetry.h"
#include "tools/common/XBUtilitiesCore.h"
#include "core/common/info_telemetry.h"

#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>

void
ReportTelemetry::getPropertyTreeInternal(const xrt_core::device * device, 
                                              boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(device, pt);
}

void 
ReportTelemetry::getPropertyTree20202( const xrt_core::device * device, 
                                           boost::property_tree::ptree &pt) const
{
  // There can only be 1 root node
  pt = xrt_core::telemetry::telemetry_info(device);
}

void 
ReportTelemetry::writeReport( const xrt_core::device* /*_pDevice*/,
                               const boost::property_tree::ptree& pt, 
                               const std::vector<std::string>& /*_elementsFilter*/, 
                               std::ostream & output) const
{
  static boost::format fmt_basic("  %-20s : %s\n");
  boost::property_tree::ptree pt_empty;
  const boost::property_tree::ptree& ptree = pt.get_child("telemetry", pt_empty);

  if (ptree.empty()) {
    output << "  Information Unavailable" << std::endl;
    return;
  }

  output << "Telemetry" << std::endl;
  for (auto& ks : ptree) {
    const boost::property_tree::ptree& telemetry_stat = ks.second;
    output << fmt_basic % telemetry_stat.get<std::string>("label") % telemetry_stat.get<std::string>("value");
  }
  output << std::endl;
}
