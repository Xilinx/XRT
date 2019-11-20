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


#include "device_pcie.h"
#include "common/utils.h"
#include "include/xrt.h"
#include <string>
#include <iostream>

xrt_core::device_pcie::device_pcie()
{
  // Do nothing
}

xrt_core::device_pcie::~device_pcie()
{
  // Do nothing
}

void 
xrt_core::device_pcie::get_devices(boost::property_tree::ptree &_pt) const
{
  size_t cardsFound = get_total_devices();

  boost::property_tree::ptree ptDevices;
  for (unsigned int deviceID = 0; deviceID < cardsFound; ++deviceID) {
    boost::property_tree::ptree ptDevice;
    std::string valueString;

    // Key: device_id
    ptDevice.put("device_id", std::to_string(deviceID).c_str());

    // Key: pcie 
    boost::property_tree::ptree ptPcie;
    get_device_info(deviceID, ptPcie);
    ptDevice.add_child("pcie", ptPcie);

    // Create our array of data
    ptDevices.push_back(std::make_pair("", ptDevice)); 
  }

  _pt.add_child("devices", ptDevices);
}

void 
xrt_core::device_pcie::get_device_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_PCIE_VENDOR, _pt);
  query_device_and_put(_deviceID, QR_PCIE_DEVICE, _pt);
  query_device_and_put(_deviceID, QR_PCIE_SUBSYSTEM_VENDOR, _pt);
  query_device_and_put(_deviceID, QR_PCIE_SUBSYSTEM_ID, _pt);
  query_device_and_put(_deviceID, QR_PCIE_LINK_SPEED, _pt);
  query_device_and_put(_deviceID, QR_PCIE_EXPRESS_LANE_WIDTH, _pt);
  query_device_and_put(_deviceID, QR_DMA_THREADS_RAW, _pt);
}




