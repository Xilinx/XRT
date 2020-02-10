/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include "system_pcie.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include <boost/format.hpp>

namespace xrt_core {

void
system_pcie::
get_devices(boost::property_tree::ptree& pt) const
{
  auto cards = get_total_devices();
  using index_type = decltype(cards.first);

  boost::property_tree::ptree pt_devices;
  for (index_type device_id = 0; device_id < cards.first; ++device_id) {
    boost::property_tree::ptree pt_device;

    // Key: device_id
    pt_device.put("device_id", std::to_string(device_id));

    // Key: pcie
    auto device = get_userpf_device(device_id);
    boost::property_tree::ptree pt_pcie;
    device->get_info(pt_pcie);
    pt_device.add_child("pcie", pt_pcie);

    // Create our array of data
    pt_devices.push_back(std::make_pair("", pt_device));
  }

  pt.add_child("devices", pt_devices);
}

uint16_t
system_pcie::
bdf2index(const std::string& bdfStr) const
{
  // Extract bdf from bdfStr.
  uint16_t dom = 0, b= 0, d = 0, f = 0;
  char dummy;
  std::stringstream s(bdfStr);
  size_t n = std::count(bdfStr.begin(), bdfStr.end(), ':');
  
  if (n == 1)
    s >> std::hex >> b >> dummy >> d >> dummy >> f;
  else if (n == 2)
    s >> std::hex >> dom >> dummy >> b >> dummy >> d >> dummy >> f;
  if ((n != 1 && n != 2) || s.fail()) {
    std::string errMsg = boost::str( boost::format("Can't extract BDF from '%s'") % bdfStr);
    throw error(errMsg);
	}

  for (uint16_t i = 0; i < get_total_devices(false).first; i++) {
    auto device = get_mgmtpf_device(i);
    auto bdf = device_query<query::pcie_bdf>(device);
    if (b == std::get<0>(bdf) && d == std::get<1>(bdf) && f == std::get<2>(bdf))
      return i;
  }
  std::string errMsg = boost::str( boost::format("No mgmt PF found for '%s'") % bdfStr);
  throw error(errMsg);
}

} // xrt_core
