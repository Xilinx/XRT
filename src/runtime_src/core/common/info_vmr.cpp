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
  if(!label.empty()) {
    label[0] = static_cast<char>(toupper(label[0]));
    std::transform(label.begin()+1, label.end(), label.begin()+1, 
                    [](int c) { return static_cast<char>(::tolower(c)); });
  }
  return label;
}

} //unnamed namespace

namespace xrt_core::vmr {

ptree_type
vmr_info(const xrt_core::device* device)
{
  ptree_type pt_vmr_status_array;
  ptree_type pt_vmr_stats;
  auto vmr_status = xrt_core::device_query_default<xq::vmr_status>(device, {});
  auto vmr_version = xrt_core::device_query_default<xq::extended_vmr_status>(device, {});
  vmr_status.insert(vmr_status.begin(), vmr_version.begin(), vmr_version.end());

  // only available for versal
  if (vmr_status.empty())
    return pt_vmr_status_array;

  //parse one line at a time
  for (const auto& stat_raw : vmr_status) {
    ptree_type pt_stat;
    const auto idx = stat_raw.find_first_of(':');
    if (idx != std::string::npos) {
      pt_stat.add("label", pretty_label(stat_raw.substr(0, idx)));
      pt_stat.add("value", stat_raw.substr(idx + 1));
    }
    else {
      throw std::runtime_error("Incorrect vmr stat format");
    }
    pt_vmr_stats.push_back(std::make_pair("", pt_stat));
  }
  pt_vmr_status_array.add_child("vmr", pt_vmr_stats);
  return pt_vmr_status_array;
}

bool
get_vmr_status(const xrt_core::device* device, vmr_status_type status)
{
  std::string status_string;
  switch (status) {
    case vmr_status_type::boot_on_default:
      status_string = "Boot on default";
      break;
    case vmr_status_type::has_fpt:
      status_string = "Has fpt";
      break;
    default:
      throw xrt_core::error(boost::str(boost::format("Unexpected key for VMR Status type %u") % static_cast<uint32_t>(status)));
  }


  const auto pt = vmr_info(device);
  boost::property_tree::ptree pt_empty;
  const boost::property_tree::ptree& ptree = pt.get_child("vmr", pt_empty);
  for (const auto& ks : ptree) {
    const boost::property_tree::ptree& vmr_stat = ks.second;
    const auto val = vmr_stat.get<std::string>("label");
    const auto nval = vmr_stat.get<std::string>("value");
    if (boost::iequals(val, status_string))
      return boost::iequals(vmr_stat.get<std::string>("value"), "1");
  }

  throw xrt_core::error(boost::str(boost::format("Did not find %s label within VMR status") % status_string));
}

} // vmr, xrt
