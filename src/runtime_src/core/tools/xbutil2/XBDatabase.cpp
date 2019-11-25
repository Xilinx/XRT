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
#include "XBUtilities.h"
namespace XBU = XBUtilities;
#include "common/device_core.h"


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
  const xrt_core::device_core &CoreDevice = xrt_core::device_core::instance();

  // Get a collection of the devices present
  CoreDevice.get_devices(_pt);

  // Now start to fill in the missing data
  boost::property_tree::ptree & ptDevices = _pt.get_child("devices");
  for (auto& kv : ptDevices) {
    boost::property_tree::ptree & ptDevice = kv.second;
    uint64_t device_id = ptDevice.get<uint64_t>("device_id", (uint64_t) -1);
    if (device_id == (uint64_t) -1) {
      std::string errMsg = "Internal Error: Invalid device ID";
      throw std::runtime_error(errMsg);
    }

    // Platform information
    boost::property_tree::ptree ptPlatform;

    // Get and add generic information
    {
      boost::property_tree::ptree pt;
      CoreDevice.get_device_platform_info(device_id, pt);
      ptPlatform.add_child("info", pt);
    }

    // Get and add ROM information
    {
      boost::property_tree::ptree pt;
      CoreDevice.get_device_rom_info(device_id, pt);
      ptPlatform.add_child("rom", pt);
    }

    // Get and add XMC information
    {
      boost::property_tree::ptree pt;
      CoreDevice.get_device_xmc_info(device_id, pt);
      ptPlatform.add_child("xmc", pt);
    }

    // Get and add thermal pcb information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_thermal_pcb(device_id, pt);
      ptPlatform.add_child("physical.thermal.pcb", pt);
    }

    // Get and add thermal fpga information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_thermal_fpga(device_id, pt);
      ptPlatform.add_child("physical.thermal.fpga", pt);
    }

    // Get and add thermal fpga information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_thermal_fpga(device_id, pt);
      ptPlatform.add_child("physical.thermal.fpga", pt);
    }

    // Get and add fan information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_fan_info(device_id, pt);
      ptPlatform.add_child("physical.fan", pt);
    }

    // Get and add thermal cage information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_thermal_cage(device_id, pt);
      ptPlatform.add_child("physical.thermal.cage", pt);
    }

    // Get and add electrical information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_electrical(device_id, pt);
      ptPlatform.add_child("physical.electrical", pt);
    }

    // Get and add power information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_power(device_id, pt);
      ptPlatform.add_child("physical.power", pt);
    }

    // Get and add firewall information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_firewall(device_id, pt);
      ptPlatform.add_child("firewall", pt);
    }

    // Get and add pcie dma status information
    {
      boost::property_tree::ptree pt;
      CoreDevice.read_device_dma_stats(device_id, pt);
      ptPlatform.add_child("pcie_dma", pt);
    }

    // Add the platform to the device tree
    ptDevice.add_child("platform", ptPlatform);
  }
}


