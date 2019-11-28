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


#include "device_linux.h"
#include "common/utils.h"
#include "include/xrt.h"
#include "scan.h"
#include <string>
#include <iostream>
#include "boost/format.hpp"
#include <map>

namespace xrt_core {

device_core*
device_core_child_ctor()
{
  static xrt_core::device_linux dl;
  return &dl;
}

const device_linux::SysDevEntry &
device_linux::get_sysdev_entry( QueryRequest _eQueryRequest) const
{
  // Initialize our lookup table
  static const std::map<QueryRequest, SysDevEntry> QueryRequestToSysDevTable =
  {
    { QR_PCIE_VENDOR,               {"",     "vendor"}},
    { QR_PCIE_DEVICE,               {"",     "device"}},
    { QR_PCIE_SUBSYSTEM_VENDOR,     {"",     "subsystem_vendor"}},
    { QR_PCIE_SUBSYSTEM_ID,         {"",     "subsystem_device"}},
    { QR_PCIE_LINK_SPEED,           {"",     "link_speed"}},
    { QR_PCIE_EXPRESS_LANE_WIDTH,   {"",     "link_width"}},
    { QR_DMA_THREADS_RAW,           {"dma",  "channel_stat_raw"}},
    { QR_ROM_VBNV,                  {"rom",  "VBNV"}},
    { OR_ROM_DDR_BANK_SIZE,         {"rom",  "ddr_bank_size"}},
    { QR_ROM_DDR_BANK_COUNT_MAX,    {"rom",  "ddr_bank_count_max"}},
    { QR_ROM_FPGA_NAME,             {"rom",  "FPGA"}},
    { QR_XMC_VERSION,               {"xmc",  "version"}},
    { QR_XMC_SERIAL_NUM,            {"xmc",  "serial_num"}},
    { QR_XMC_MAX_POWER,             {"xmc",  "max_power"}},
    { QR_XMC_BMC_VERSION,           {"xmc",  "bmc_ver"}},
    { QR_DNA_SERIAL_NUM,            {"dna",  "dna"}},
    { QR_CLOCK_FREQS,               {"icap", "clock_freqs"}},
    { QR_IDCODE,                    {"icap", "idcode"}},
    { QR_STATUS_MIG_CALIBRATED,     {"",     "mig_calibration"}},
    { QR_STATUS_P2P_ENABLED,        {"",     "p2p_enable"}},
    { QR_TEMP_CARD_TOP_FRONT,       {"xmc",  "xmc_se98_temp0"}},
    { QR_TEMP_CARD_TOP_REAR,        {"xmc",  "xmc_se98_temp1"}},
    { QR_TEMP_CARD_BOTTOM_FRONT,    {"xmc",  "xmc_se98_temp2"}},
    { QR_TEMP_FPGA,                 {"xmc",  "xmc_fpga_temp"}},
    { QR_FAN_TRIGGER_CRITICAL_TEMP, {"xmc",  "xmc_fan_temp"}},
    { QR_FAN_FAN_PRESENCE,          {"xmc",  "fan_presence"}},
    { QR_FAN_SPEED_RPM,             {"xmc",  "xmc_fan_rpm"}},
    { QR_CAGE_TEMP_0,               {"xmc",  "xmc_cage_temp0"}},
    { QR_CAGE_TEMP_1,               {"xmc",  "xmc_cage_temp1"}},
    { QR_CAGE_TEMP_2,               {"xmc",  "xmc_cage_temp2"}},
    { QR_CAGE_TEMP_3,               {"xmc",  "xmc_cage_temp3"}},
    { QR_12V_PEX_MILLIVOLTS,        {"xmc",  "xmc_12v_pex_vol"}},
    { QR_12V_PEX_MILLIAMPS,         {"xmc",  "xmc_12v_pex_curr"}},
    { QR_12V_AUX_MILLIVOLTS,        {"xmc",  "xmc_12v_aux_vol"}},
    { QR_12V_AUX_MILLIAMPS,         {"xmc",  "xmc_12v_aux_curr"}},
    { QR_3V3_PEX_MILLIVOLTS,        {"xmc",  "xmc_3v3_pex_vol"}},
    { QR_3V3_AUX_MILLIVOLTS,        {"xmc",  "xmc_3v3_aux_vol"}},
    { QR_DDR_VPP_BOTTOM_MILLIVOLTS, {"xmc",  "xmc_ddr_vpp_btm"}},
    { QR_DDR_VPP_TOP_MILLIVOLTS,    {"xmc",  "xmc_ddr_vpp_top"}},

    { QR_5V5_SYSTEM_MILLIVOLTS,     {"xmc",  "xmc_sys_5v5"}},
    { QR_1V2_VCC_TOP_MILLIVOLTS,    {"xmc",  "xmc_1v2_top"}},
    { QR_1V2_VCC_BOTTOM_MILLIVOLTS, {"xmc",  "xmc_vcc1v2_btm"}},
    { QR_1V8_MILLIVOLTS,            {"xmc",  "xmc_1v8"}},
    { QR_0V85_MILLIVOLTS,           {"xmc",  "xmc_0v85"}},
    { QR_0V9_VCC_MILLIVOLTS,        {"xmc",  "xmc_mgt0v9avcc"}},
    { QR_12V_SW_MILLIVOLTS,         {"xmc",  "xmc_12v_sw"}},
    { QR_MGT_VTT_MILLIVOLTS,        {"xmc",  "xmc_mgtavtt"}},
    { QR_INT_VCC_MILLIVOLTS,        {"xmc",  "xmc_vccint_vol"}},
    { QR_INT_VCC_MILLIAMPS,         {"xmc",  "xmc_vccint_curr"}},

    { QR_3V3_PEX_MILLIAMPS,         {"xmc",  "xmc_3v3_pex_curr"}},
    { QR_0V85_MILLIAMPS,            {"xmc",  "xmc_0v85_curr"}},
    { QR_3V3_VCC_MILLIVOLTS,        {"xmc",  "xmc_3v3_vcc_vol"}},
    { QR_HBM_1V2_MILLIVOLTS,        {"xmc",  "xmc_hbm_1v2_vol"}},
    { QR_2V5_VPP_MILLIVOLTS,        {"xmc",  "xmc_vpp2v5_vol"}},
    { QR_INT_BRAM_VCC_MILLIVOLTS,   {"xmc",  "xmc_vccint_bram_vol"}},

    { QR_FIREWALL_DETECT_LEVEL,     {"firewall", "detected_level"}},
    { QR_FIREWALL_STATUS,           {"firewall", "detected_status"}},
    { QR_FIREWALL_TIME_SEC,         {"firewall", "detected_time"}},

    { QR_POWER_MICROWATTS,          {"xmc", "xmc_power"}}
  };
  // Find the translation entry
  auto it = QueryRequestToSysDevTable.find(_eQueryRequest);

  if (it == QueryRequestToSysDevTable.end()) {
    std::string errMsg = boost::str( boost::format("The given query request ID (%d) is not supported.") % _eQueryRequest);
    throw no_such_query(_eQueryRequest, errMsg);
  }

  return it->second;
}



void
device_linux::
query_device(uint64_t _deviceID, QueryRequest _eQueryRequest, const std::type_info & _typeInfo, boost::any &_returnValue) const
{
  // Initialize return data to being empty container.
  // Note: CentOS Boost 1.53 doesn't support the clear() method.
  boost::any anyEmpty;
  _returnValue.swap(anyEmpty);

  // Get the sysdev and entry values to call
  const SysDevEntry & entry = get_sysdev_entry(_eQueryRequest);

  std::string sErrorMsg;

  if (_typeInfo == typeid(std::string)) {
    // -- Typeid: std::string --
    _returnValue = std::string("");
    std::string *stringValue = boost::any_cast<std::string>(&_returnValue);
    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, *stringValue);

  } else if (_typeInfo == typeid(uint64_t)) {
    // -- Typeid: uint64_t --
    _returnValue = (uint64_t) -1;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, uint64Vector);
    if (!uint64Vector.empty()) {
      _returnValue = uint64Vector[0];
    }

  } else if (_typeInfo == typeid(bool)) {
    // -- Typeid: bool --
    _returnValue = (bool) 0;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, uint64Vector);
    if (!uint64Vector.empty()) {
      _returnValue = (bool) uint64Vector[0];
    }

  } else if (_typeInfo == typeid(std::vector<std::string>)) {
    // -- Typeid: std::vector<std::string>
    _returnValue = std::vector<std::string>();
    std::vector<std::string> *stringVector = boost::any_cast<std::vector<std::string>>(&_returnValue);
    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, *stringVector);

  } else {
    sErrorMsg = boost::str( boost::format("Error: Unsupported query_device return type: '%s'") % _typeInfo.name());
  }

  if (!sErrorMsg.empty()) {
    throw std::runtime_error(sErrorMsg);
  }
}


device_linux::device_linux()
{
  // Do nothing
}

device_linux::~device_linux() {
  // Do nothing
}

std::pair<uint64_t, uint64_t>
device_linux::get_total_devices() const
{
  return std::make_pair(pcidev::get_dev_total(), pcidev::get_dev_ready());
}

void
device_linux::read_device_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  _deviceID = _deviceID;
  _pt = _pt;
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

void
device_linux::
scan_devices(bool verbose, bool json) const
{
  std::cout << "TO-DO: scan_devices\n";
  verbose = verbose;
  json = json;
}

void
device_linux::
auto_flash(uint64_t _deviceID, std::string& shell, std::string& id, bool force) const
{
  std::cout << "TO-DO: auto_flash\n";
  _deviceID = _deviceID;
  shell = shell;
  id = id;
  force = force;
}

void
device_linux::
reset_shell(uint64_t _deviceID) const
{
  std::cout << "TO-DO: reset_shell\n";
  _deviceID = _deviceID;
}

void
device_linux::
update_shell(uint64_t _deviceID, std::string flashType, std::string& primary, std::string& secondary) const
{
  std::cout << "TO-DO: update_shell\n";
  _deviceID = _deviceID;
  flashType = flashType;
  primary = primary;
  secondary = secondary;
}

void
device_linux::
update_SC(uint64_t _deviceID, std::string& file) const
{
  std::cout << "TO-DO: update_SC\n";
  _deviceID = _deviceID;
  file = file;
}

} // xrt_core
