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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBDatabase.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;
#include "common/system.h"
#include "common/device.h"


// 3rd Party Library - Include Files

// System - Include Files

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBDatabase;


// ------ F U N C T I O N S ---------------------------------------------------
void
XBDatabase::create_complete_device_tree(boost::property_tree::ptree & _pt)
{
  // Work with a clean property tree
  _pt.clear();

  // Get the handle to the devices
  xrt_core::get_devices(_pt);

  // Now start to fill in the missing data
  boost::property_tree::ptree & ptDevices = _pt.get_child("devices");
  for (auto& kv : ptDevices) {
    boost::property_tree::ptree & ptDevice = kv.second;
    auto device_id = ptDevice.get<unsigned int>("device_id", (unsigned int) -1);
    if (device_id == (unsigned int) -1) {
      std::string errMsg = "Internal Error: Invalid device ID";
      throw std::runtime_error(errMsg);
    }

    auto device = xrt_core::get_userpf_device(device_id);

    // Platform information
    boost::property_tree::ptree ptPlatform;

    // Get and add generic information
    {
      boost::property_tree::ptree pt;
      device->get_platform_info(pt);
      ptPlatform.add_child("info", pt);
    }

    // Get and add ROM information
    {
      boost::property_tree::ptree pt;
      device->get_rom_info(pt);
      ptPlatform.add_child("rom", pt);
    }

    // Get and add XMC information
    {
      boost::property_tree::ptree pt;
      device->get_xmc_info(pt);
      ptPlatform.add_child("xmc", pt);
    }

    // Get and add thermal pcb information
    {
      boost::property_tree::ptree pt;
      device->read_thermal_pcb(pt);
      ptPlatform.add_child("physical.thermal.pcb", pt);
    }

    // Get and add thermal fpga information
    {
      boost::property_tree::ptree pt;
      device->read_thermal_fpga(pt);
      ptPlatform.add_child("physical.thermal.fpga", pt);
    }

    // Get and add fan information
    {
      boost::property_tree::ptree pt;
      device->read_fan_info(pt);
      ptPlatform.add_child("physical.fan", pt);
    }

    // Get and add thermal cage information
    {
      boost::property_tree::ptree pt;
      device->read_thermal_cage(pt);
      ptPlatform.add_child("physical.thermal.cage", pt);
    }

    // Get and add electrical information
    {
      boost::property_tree::ptree pt;
      device->read_electrical(pt);
      ptPlatform.add_child("physical.electrical", pt);
    }

    // Get and add power information
    {
      boost::property_tree::ptree pt;
      device->read_power(pt);
      ptPlatform.add_child("physical.power", pt);
    }

    // Get and add firewall information
    {
      boost::property_tree::ptree pt;
      device->read_firewall(pt);
      ptPlatform.add_child("firewall", pt);
    }

    // Get and add pcie dma status information
    {
      boost::property_tree::ptree pt;
      device->read_dma_stats(pt);
      ptPlatform.add_child("pcie_dma", pt);
    }

    // Add the platform to the device tree
    ptDevice.add_child("platform", ptPlatform);
  }
}
