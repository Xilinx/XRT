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
 auto cards = get_total_devices();
  using index_type = decltype(cards.first);

  boost::property_tree::ptree pt_devices;
  for (index_type device_id = 0; device_id < cards.first; ++device_id) {
    boost::property_tree::ptree pt_device;

    // Key: device_id
    pt_device.put("device_id", std::to_string(device_id));

    // Key: edge
    auto device = get_userpf_device(device_id);
    boost::property_tree::ptree pt_edge;
    device->get_info(pt_edge);
    pt_device.add_child("edge", pt_edge);

    // Create our array of data
    pt_devices.push_back(std::make_pair("", pt_device));
  }

  pt.add_child("devices", pt_devices);

}

} // xrt_core
