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


#include "common/core_device.h"
#include "scan.h"
#include <string>
#include <iostream>

void 
xrt_core::device::get_device_pcie_info(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  std::string errorMsg;
  std::string valueString;
  uint64_t valueUInt64;

  // Key: vendor
  pcidev::get_dev(_deviceID)->sysfs_get( "", "vendor", errorMsg, valueString);
  _pt.put("vendor", valueString);

  // Key: device
  pcidev::get_dev(_deviceID)->sysfs_get( "", "device", errorMsg, valueString );
  _pt.put("device", valueString);

  // Key: subsystem_vendor
  pcidev::get_dev(_deviceID)->sysfs_get( "", "subsystem_vendor", errorMsg, valueString );
  _pt.put("subsystem_vendor", valueString);

  // Key: subsystem_id
  pcidev::get_dev(_deviceID)->sysfs_get( "", "subsystem_device", errorMsg, valueString );
  _pt.put("subsystem_id", valueString);

  // Key: link_speed
  pcidev::get_dev(_deviceID)->sysfs_get<uint64_t>( "", "link_speed", errorMsg, valueUInt64, 0 );
  _pt.put( "link_speed", std::to_string(valueUInt64).c_str());

  // Key:: width
  pcidev::get_dev(_deviceID)->sysfs_get<uint64_t>( "", "width", errorMsg, valueUInt64, 0 );
  _pt.put( "width", std::to_string(valueUInt64).c_str() );

  // Key:: dma_threads
  {
    std::vector<std::string> dmaThreads;
    pcidev::get_dev(_deviceID)->sysfs_get( "dma", "channel_stat_raw", errorMsg, dmaThreads); 
    _pt.put("dma_thread_count", std::to_string(dmaThreads.size()).c_str());
  }
}


void 
xrt_core::device::get_devices(boost::property_tree::ptree &_pt)
{
  size_t cardsFound = pcidev::get_dev_total();

  boost::property_tree::ptree ptDevices;
  for (unsigned int deviceID = 0; deviceID < cardsFound; ++deviceID) {
    boost::property_tree::ptree ptDevice;
    std::string valueString;

    // Key: device_id
    ptDevice.put("device_id", std::to_string(deviceID).c_str());

    // Key: pcie 
    boost::property_tree::ptree ptPcie;
    get_device_pcie_info(deviceID, ptPcie);
    ptDevice.add_child("pcie", ptPcie);

    // Create our array of data
    ptDevices.push_back(std::make_pair("", ptDevice)); 
  }

  _pt.add_child("devices", ptDevices);
}


void 
xrt_core::device::get_device_rom_info(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  std::string errorMsg;
  std::string valueString;
  uint64_t valueUInt64;

  // Key: vbnv
  pcidev::get_dev(_deviceID)->sysfs_get("rom", "VBNV", errorMsg, valueString);
  _pt.put("vbnv", valueString.c_str());

  // Key: ddr_size
  {
    pcidev::get_dev(_deviceID)->sysfs_get<uint64_t>("rom", "ddr_bank_size", errorMsg, valueUInt64, 0);
    valueUInt64 = valueUInt64 << 30; // Convert from GBytes to bytes
    std::ostringstream buf;
    buf << std::hex << "0x" << uintptr_t(valueUInt64);
    _pt.put("ddr_size_bytes", buf.str().c_str());
  }

  // Key: ddr_count
  pcidev::get_dev(_deviceID)->sysfs_get<uint64_t>( "rom", "ddr_bank_count_max", errorMsg, valueUInt64, 0);
  _pt.put("ddr_count", std::to_string(valueUInt64).c_str());

  // Key: fpga_name
  pcidev::get_dev(_deviceID)->sysfs_get( "rom", "FPGA", errorMsg, valueString);
  _pt.put( "fpga_name", valueString.c_str());
}


void 
xrt_core::device::get_device_xmc_info(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  std::string errorMsg;
  std::string valueString;

  // Key: xmc_version
  pcidev::get_dev(_deviceID)->sysfs_get( "xmc", "version", errorMsg, valueString);
  _pt.put("xmc_version", valueString.c_str() );

  // Key: serial_number
  pcidev::get_dev(_deviceID)->sysfs_get( "xmc", "serial_num", errorMsg, valueString);
  _pt.put("serial_number", valueString.c_str());

  // Key: max_power
  pcidev::get_dev(_deviceID)->sysfs_get( "xmc", "max_power", errorMsg, valueString);
  _pt.put("max_power", valueString.c_str());

  // Key: bmc_version
  pcidev::get_dev(_deviceID)->sysfs_get( "xmc", "bmc_ver", errorMsg, valueString);
  _pt.put("satellite_controller_version", valueString.c_str());
}

void 
xrt_core::device::get_device_platform_info(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  std::string errorMsg;
  std::string valueString;
  bool valueBool;
  uint64_t valueUInt64;

  // Key: dna
  pcidev::get_dev(_deviceID)->sysfs_get( "dna", "dna", errorMsg, valueString);
  _pt.put("dns", valueString.c_str());

  // Key: clocks
  {
    std::vector<std::string> clockFreqs;
    pcidev::get_dev(_deviceID)->sysfs_get( "icap", "clock_freqs", errorMsg, clockFreqs); 
    boost::property_tree::ptree ptClocks;
    for (unsigned int clockID = 0; clockID < clockFreqs.size(); ++clockID) {
      boost::property_tree::ptree ptClock;
      ptClock.put("clock_id", std::to_string(clockID).c_str());
      ptClock.put("freq_mhz", clockFreqs[clockID].c_str());

      // Create our array of data
      ptClocks.push_back(std::make_pair("", ptClock)); 
    }
    _pt.add_child("clocks", ptClocks);
  }

  // Key: idcode
  pcidev::get_dev(_deviceID)->sysfs_get( "icap", "idcode", errorMsg, valueString);
  _pt.put("idcode", valueString.c_str());

  // Key: mid_calibrated
  pcidev::get_dev(_deviceID)->sysfs_get<bool>( "", "mig_calibration", errorMsg, valueBool, false);
  _pt.put("mig_calibrate", valueBool ? "true" : "false" );

  // Key:: p2p_enabled
  pcidev::get_dev(_deviceID)->sysfs_get<uint64_t>("", "p2p_enable", errorMsg, valueUInt64, 0);
  _pt.put("p2p_enabled", valueUInt64 ? "true" : "false" );
}
