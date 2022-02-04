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
#define XRT_CORE_COMMON_SOURCE
#include "info_vmr.h"
#include "query_requests.h"

#include <boost/algorithm/string.hpp>

// Too much typing
using ptree_type = boost::property_tree::ptree;
namespace xq = xrt_core::query;

namespace {

static std::string 
pretty_label(std::string label)
{
  std::replace(label.begin(), label.end(), '_', ' ');
  std::transform(label.begin()+1, label.end(), label.begin()+1, ::tolower);
  return label;
}

} //unnamed namespace

namespace xrt_core { 
namespace vmr {

ptree_type
vmr_info(const xrt_core::device* device)
{
  ptree_type pt_vmr_status_array;
  ptree_type pt_vmr_stats;
  std::vector<std::string> vmr_status_raw;
  try {
    vmr_status_raw = xrt_core::device_query<xq::vmr_status>(device);
  }
  catch (const xq::exception& ex) {
    // only available for versal
    return pt_vmr_status_array;
  }

  //parse one line at a time
  for (auto& stat_raw : vmr_status_raw) {
    ptree_type pt_stat;
    std::vector<std::string> stat;
    boost::split(stat, stat_raw, boost::is_any_of(":")); // eg: HAS_FPT:1
    pt_stat.add("label", pretty_label(stat.at(0)));      // eg: Has ftp
    pt_stat.add("value", stat.at(1));                    // eg: 1
    pt_vmr_stats.push_back(std::make_pair("", pt_stat));
  }
  pt_vmr_status_array.add_child("vmr", pt_vmr_stats);
  return pt_vmr_status_array;
}

}} // vmr, xrt
