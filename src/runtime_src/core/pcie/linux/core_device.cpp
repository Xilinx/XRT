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
#include "common/utils.h"
#include "include/xrt.h"
#include "scan.h"
#include <string>
#include <iostream>

void 
xrt_core::device::get_device_pcie_info(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  std::string errorMsg;
  std::string valueString;
  uint64_t valueUInt64 = 0;

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
  uint64_t valueUInt64 = 0;

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
  bool valueBool = false;
  uint64_t valueUInt64 = 0;

  // Key: dna
  pcidev::get_dev(_deviceID)->sysfs_get( "dna", "dna", errorMsg, valueString);
  _pt.put("dna", valueString.c_str());

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

void 
xrt_core::device::read_device_thermal_pcb(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  uint32_t valueUInt32 = 0;

  // Key: top_front
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_se98_temp0", valueUInt32 );
  _pt.put("top_front", std::to_string(valueUInt32).c_str());

  // Key: top_rear
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_se98_temp1", valueUInt32 );
  _pt.put("top_rear", std::to_string(valueUInt32).c_str());

  // Key: top_rear
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_se98_temp2", valueUInt32 );
  _pt.put("btm_front", std::to_string(valueUInt32).c_str());
}

void 
xrt_core::device::read_device_thermal_fpga(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  uint32_t valueUInt32 = 0;

  // Key: temp_C
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_fpga_temp", valueUInt32 );
  _pt.put("temp_C", std::to_string(valueUInt32).c_str());
}

void 
xrt_core::device::read_device_fan_info(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  std::string errorMsg;
  std::string valueString;
  uint32_t valueUInt32 = 0;

  // Key: tcrit_temp
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_fan_temp",  valueUInt32 );
  _pt.put( "tcrit_temp", std::to_string(valueUInt32).c_str());

  // Key: fan_presence
  pcidev::get_dev(_deviceID)->sysfs_get( "xmc", "fan_presence", errorMsg, valueString );
  _pt.put( "fan_presence", valueString );

  // Key: fan_speed_rpm
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_fan_rpm",  valueUInt32 );
  _pt.put( "fan_speed_rpm", std::to_string(valueUInt32).c_str());
}

void 
xrt_core::device::read_device_thermal_cage(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  uint32_t valueUInt32 = 0;

  // Key: temp0
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_cage_temp0", valueUInt32);
  _pt.put( "temp0", std::to_string(valueUInt32).c_str());

  // Key: temp1
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_cage_temp1", valueUInt32);
  _pt.put( "temp1", std::to_string(valueUInt32).c_str());

  // Key: temp2
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_cage_temp2", valueUInt32);
  _pt.put( "temp2", std::to_string(valueUInt32).c_str());

  // Key: temp3
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_cage_temp3", valueUInt32);
  _pt.put( "temp3", std::to_string(valueUInt32).c_str());
}


template <typename T>
std::string 
static to_string(const T _value, const int _precision = 6)
{
  std::ostringstream sBuffer;
  sBuffer.precision(_precision);
  sBuffer << std::fixed << _value;
  return sBuffer.str();
}

void 
xrt_core::device::read_device_electrical(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  uint32_t valueUInt32 = 0;

  // Key: 12v_pex.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_12v_pex_vol",    valueUInt32);
  _pt.put( "12v_pex.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 12v_pex.current
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_12v_pex_curr",   valueUInt32);
  _pt.put( "12v_pex.current", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 12v_aux.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_12v_aux_vol",    valueUInt32);
  _pt.put( "12v_aux.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 12v_aux.current
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_12v_aux_curr",   valueUInt32);
  _pt.put( "12v_aux.current", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 3v3_pex.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_3v3_pex_vol",    valueUInt32);
  _pt.put( "3v3_pex.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 3v3_aux.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_3v3_aux_vol",    valueUInt32); 
  _pt.put( "3v3_aux.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: ddr_vpp_bottom.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_ddr_vpp_btm",    valueUInt32);
  _pt.put( "ddr_vpp_bottom.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: ddr_vpp_top.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_ddr_vpp_top",    valueUInt32);
  _pt.put( "ddr_vpp_top.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: sys_5v5.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_sys_5v5",        valueUInt32);
  _pt.put( "sys_5v5.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 1v2_top.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_1v2_top",        valueUInt32);
  _pt.put( "1v2_top.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 1v2_btm.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_vcc1v2_btm",     valueUInt32);
  _pt.put( "1v2_btm.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 1v8.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_1v8",            valueUInt32);
  _pt.put( "1v8.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 0v85.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_0v85",           valueUInt32);
  _pt.put( "0v85.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: mgt_0v9.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_mgt0v9avcc",     valueUInt32);
  _pt.put( "mgt_0v9.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 12v_sw.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_12v_sw",         valueUInt32);
  _pt.put( "12v_sw.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: mgt_vtt.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_mgtavtt",        valueUInt32);
  _pt.put( "mgt_vtt.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: vccint.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor( "xmc", "xmc_vccint_vol",     valueUInt32);
  _pt.put( "vccint.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: vccint.current
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_vccint_curr",     valueUInt32);
  _pt.put( "vccint.current", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 3v3_pex.current
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_3v3_pex_curr",    valueUInt32);
  _pt.put( "3v3_pex.current", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: 0v85.current
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_0v85_curr",       valueUInt32);
  _pt.put( "0v85.current", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: vcc3v3.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_3v3_vcc_vol",     valueUInt32);
  _pt.put( "vcc3v3.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: hbm_1v2.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_hbm_1v2_vol",     valueUInt32);
  _pt.put( "hbm_1v2.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: vpp2v5.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_vpp2v5_vol",      valueUInt32);
  _pt.put( "vpp2v5.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());

  // Key: vccint_bram.voltage
  pcidev::get_dev(_deviceID)->sysfs_get_sensor("xmc", "xmc_vccint_bram_vol", valueUInt32);
  _pt.put( "vccint_bram.voltage", ::to_string((float)valueUInt32 / 1000.0, 3).c_str());
}

void 
xrt_core::device::read_device_power(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  // Key: power_watts
  unsigned long long valueLongLong = 0;
  std::string errMsg;

  pcidev::get_dev(_deviceID)->sysfs_get<unsigned long long>( "xmc", "xmc_power",  errMsg, valueLongLong, 0);

  float power = -1;

  if (errMsg.empty()) {
    power = (float) valueLongLong / 1000000;
  }

  _pt.put( "power_watts", ::to_string(power, 6).c_str());
}


void 
xrt_core::device::read_device_firewall(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  std::string errorMsg;
  uint32_t valueUInt32 = 0;
  unsigned long long valueLongLong = 0;

  // Key: level
  pcidev::get_dev(_deviceID)->sysfs_get<unsigned int>( "firewall", "detected_level", errorMsg, valueUInt32, 0 );
  _pt.put( "level", std::to_string(valueUInt32).c_str());

  // Key: status & status_raw
  {
    pcidev::get_dev(_deviceID)->sysfs_get<unsigned int>( "firewall", "detected_status", errorMsg, valueUInt32, 0 ); 
    _pt.put( "status", parseFirewallStatus(valueUInt32) );
    std::ostringstream buf;
    buf << std::hex << "0x" << uintptr_t(valueUInt32);
    _pt.put("status_bits", buf.str().c_str());
  }

  // Key: time
  pcidev::get_dev(_deviceID)->sysfs_get<unsigned long long>( "firewall", "detected_time", errorMsg, valueLongLong, 0 ); 
  _pt.put( "time", std::to_string(valueLongLong).c_str());
}

void 
xrt_core::device::read_device_pcie_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt)
// TODO: Update code to handle error messages 
{
  xclDeviceHandle handle = xclOpen(_deviceID, nullptr, XCL_QUIET);

  if (!handle) {
    // Unable to get a handle
    return;
  }

  xclDeviceUsage devstat = { 0 };
  xclGetUsageInfo(handle, &devstat);

  // Clean up after ourselves
  xclClose(handle);

  boost::property_tree::ptree ptChannels;
  for (unsigned index = 0; index < XCL_DEVICE_USAGE_COUNT; ++index) {
      boost::property_tree::ptree ptDMA;
      ptDMA.put( "id", std::to_string(index).c_str());
      ptDMA.put( "h2c", unitConvert(devstat.h2c[index]) );
      ptDMA.put( "c2h", unitConvert(devstat.c2h[index]) );

      // Create our array of data
      ptChannels.push_back(std::make_pair("", ptDMA)); 
  }

  _pt.add_child( "transfer_metrics.channels", ptChannels);
}

