/**
 * Copyright (C) 2022 Xilinx, Inc
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
#include "ReportVmrStatus.h"
#include "core/common/info_vmr.h"

void
ReportVmrStatus::getPropertyTreeInternal(const xrt_core::device * device, 
                                              boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(device, pt);
}

void 
ReportVmrStatus::getPropertyTree20202( const xrt_core::device * device, 
                                           boost::property_tree::ptree &pt) const
{
  // There can only be 1 root node
  pt = xrt_core::vmr::vmr_info(device);
}

void 
ReportVmrStatus::writeReport( const xrt_core::device* /*_pDevice*/,
                               const boost::property_tree::ptree& pt, 
                               const std::vector<std::string>& /*_elementsFilter*/, 
                               std::ostream & output) const
{
  static boost::format fmt_basic("  %-20s : %s\n");
  boost::property_tree::ptree pt_empty;
  const boost::property_tree::ptree& ptree = pt.get_child("vmr", pt_empty);

  output << "Vmr Status" << std::endl;
  if (ptree.empty()) {
    output << "  Information Unavailable" << std::endl;
    return;
  }
  for(auto& ks : ptree) {
    const boost::property_tree::ptree& vmr_stat = ks.second;
    output << fmt_basic % vmr_stat.get<std::string>("label") 
                        % vmr_stat.get<std::string>("value");
  }
  output << std::endl;
}
