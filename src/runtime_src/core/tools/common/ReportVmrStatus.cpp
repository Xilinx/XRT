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
#include "XBUtilitiesCore.h"
#include "core/common/info_vmr.h"

#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>

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

  //Only vck5000 cards support vmr
  output << "Vmr Status" << std::endl;
  if (ptree.empty()) {
    output << "  Information Unavailable" << std::endl;
    return;
  }

  //vmr should be supported, but info was unavailable
  const boost::property_tree::ptree& version_ptree = pt.get_child("vmr.vmr_version", pt_empty);
  if (version_ptree.empty())
    throw std::runtime_error("Information Unavailable");

  //list of non verbose labels
  const std::vector<std::string> non_verbose_labels = { 
      "build flags", 
      "vitis version", 
      "git hash", 
      "git branch", 
      "git hash date" 
    };

  for(auto& ks : version_ptree) {
    const boost::property_tree::ptree& vmr_stat = ks.second;
    const auto it = std::find_if(non_verbose_labels.begin(), non_verbose_labels.end(),
                      [&vmr_stat](const auto& str) { return boost::iequals(vmr_stat.get<std::string>("label"), str); });
    
    if(XBUtilities::getVerbose())
      output << fmt_basic % vmr_stat.get<std::string>("label") % vmr_stat.get<std::string>("value");
    else if(it != std::end(non_verbose_labels))
      output << fmt_basic % vmr_stat.get<std::string>("label") % vmr_stat.get<std::string>("value");
  }
  output << std::endl;
}
