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
#include "common/device_core.h"

// 3rd Party Library - Include Files

// System - Include Files
#include <iostream>
#include <iomanip>

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBDatabase;


// ------ F U N C T I O N S ---------------------------------------------------
void
XBDatabase::dump(boost::property_tree::ptree & _pt, std::ostream& ostr)
{
		std::ios::fmtflags f(ostr.flags());
		ostr << std::left << std::endl;
		ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
		ostr << std::setw(32) << "Shell" << std::setw(32) << "FPGA" << "IDCode" << std::endl;
		ostr << std::setw(32) << _pt.get<std::string>("platform.rom.vbnv", "N/A")
			<< std::setw(32) << _pt.get<std::string>("platform.rom.fpga_name", "N/A")
			<< _pt.get<std::string>("platform.info.idcode", "N/A") << std::endl;
		ostr << std::setw(16) << "Vendor" << std::setw(16) << "Device" << std::setw(16) << "SubDevice"
			<< std::setw(16) << "SubVendor" << std::setw(16) << "SerNum" << std::endl;
		ostr << std::setw(16) << _pt.get<std::string>("pcie.vendor", "N/A")
			<< std::setw(16) << _pt.get<std::string>("pcie.device", "N/A")
			<< std::setw(16) << _pt.get<std::string>("pcie.subsystem_id", "N/A")
			<< std::setw(16) << _pt.get<std::string>("pcie.subsystem_vendor", "N/A")
			<< std::setw(16) << _pt.get<std::string>("pcie.serial_number", "N/A") << std::endl;
		ostr << std::setw(16) << "DDR size (MB)" << std::setw(16) << "DDR count" << std::setw(16)
			<< "Clock0" << std::setw(16) << "Clock1" << std::setw(16) << "Clock2" << std::endl;
		ostr << std::setw(16) << strtoull(_pt.get<std::string>("platform.rom.ddr_size_bytes", "N/A").c_str(), nullptr, 16) / (1024 * 1024)
			<< std::setw(16) << _pt.get("platform.rom.widdr_countdth", -1)
			<< std::setw(16) << _pt.get("platform.info.clock0", -1)
			<< std::setw(16) << _pt.get("platform.info.clock1", -1)
			<< std::setw(16) << _pt.get("platform.info.clock2", -1) << std::endl;

		ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

		//dumpPartitionInfo(ostr);
		ostr.flags(f);
}

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

#if 0
    // Get and add generic information
    {
      boost::property_tree::ptree pt;
      CoreDevice.get_device_platform_info(device_id, pt);
      ptPlatform.add_child("info", pt);
    }

#endif
    // Get and add ROM information
    {
      boost::property_tree::ptree pt;
      CoreDevice.get_device_rom_info(device_id, pt);
      ptPlatform.add_child("rom", pt);
    }

#if 0
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
#endif
	{
	  //boost::property_tree::ptree pt;
	  struct ip_layout *ipLayout = NULL;
	  unsigned long ipSize = CoreDevice.get_ip_layoutsize(device_id);
	  ipLayout = (struct ip_layout*)malloc(ipSize);
	  CoreDevice.get_ip_layout(device_id, &ipLayout, ipSize);
	  if (ipLayout != NULL) {
		  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
		  std::cout << "Compute Unit Status:" << std::endl;
		  for (int i = 0; i < ipLayout->m_count; i++) {
			  struct ip_data* data = &ipLayout->m_ip_data[i];
			  if (data->m_type != IP_KERNEL)
				  continue;
			  size_t len = strlen((char*)data->m_name);
			  std::string name((const char*)data->m_name, len);

			  printf("[%d]: %s @0x%llx\t(TBD)\n", i, name.c_str(), data->m_base_address);
		  }
	  }
		//ptPlatform.add_child("ip_layout", pt);
	}

	{
	  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
	  struct mem_topology topoInfo;
	  CoreDevice.get_mem_topology(device_id, &topoInfo);
	  std::cout << "Memory Status:" << std::endl;
	  std::cout << "Tag" << std::setw(16) << "Type" << std::setw(16)
		  << "Temp(C)" << std::setw(16) << "Size (GB)" << std::setw(16)
		  << "Mem Usage" << std::setw(16) << "BO count" << std::endl;
	  struct mem_raw_info memRaw;
	  CoreDevice.get_mem_rawinfo(device_id, &memRaw);

	  for (size_t i = 0; i < topoInfo.m_count; i++) {
		  std::cout << topoInfo.m_mem_data[i].m_tag << std::setw(16);
		  switch (topoInfo.m_mem_data[i].m_type) {
		  case MEM_DDR3: std::cout << "MEM_DDR3" << std::setw(16); break;
		  case MEM_DDR4: std::cout << "MEM_DDR4" << std::setw(16); break;
		  case MEM_DRAM: std::cout << "MEM_DRAM" << std::setw(16); break;
		  case MEM_STREAMING: std::cout << "MEM_STREAMING" << std::setw(16); break;
		  case MEM_PREALLOCATED_GLOB: std::cout << "MEM_PREALLOCATED_GLOB" << std::setw(16); break;
		  case MEM_ARE: std::cout << "MEM_ARE" << std::setw(16); break;
		  case MEM_HBM: std::cout << "MEM_HBM" << std::setw(16); break;
		  case MEM_BRAM: std::cout << "MEM_BRAM" << std::setw(16); break;
		  case MEM_URAM: std::cout << "MEM_URAM" << std::setw(16); break;
		  case MEM_STREAMING_CONNECTION: std::cout << "MEM_STREAMING_CONNECTION" << std::setw(16); break;
		  }
		  std::cout << ptPlatform.get<std::string>("physical.thermal.fpga.temp_c", "N/A") << std::setw(16);
		  std::cout << topoInfo.m_mem_data[i].m_size / (1024 * 1024) << std::setw(16)
			  << memRaw.MemRaw[i].MemoryUsage / (1024 * 1024) << " (MB)" << std::setw(16)
			  << memRaw.MemRaw[i].BOCount << std::endl;
		}
	}
    // Add the platform to the device tree
    ptDevice.add_child("platform", ptPlatform);
    dump(ptDevice, std::cout);
  }
}
