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
#include "xrt.h"
#include "scan.h"
#include <string>
#include <iostream>
#include <map>
#include "boost/format.hpp"

namespace xrt_core {

const device_linux::SysDevEntry&
device_linux::get_sysdev_entry(QueryRequest qr) const
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
  auto it = QueryRequestToSysDevTable.find(qr);

  if (it == QueryRequestToSysDevTable.end()) {
    std::string errMsg = boost::str( boost::format("The given query request ID (%d) is not supported.") % qr);
    throw no_such_query(qr, errMsg);
  }

  return it->second;
}

void
device_linux::
query(QueryRequest qr, const std::type_info& tinfo, boost::any& value) const
{
  // Initialize return data to being empty container.
  // Note: CentOS Boost 1.53 doesn't support the clear() method.
  boost::any anyEmpty;
  value.swap(anyEmpty);

  auto device_id = get_device_id();

  // Get the sysdev and entry values to call
  auto& entry = get_sysdev_entry(qr);

  std::string errmsg;

  if (tinfo == typeid(std::string)) {
    // -- Typeid: std::string --
    value = std::string("");
    auto p_str = boost::any_cast<std::string>(&value);
    pcidev::get_dev(device_id)->sysfs_get(entry.sSubDevice, entry.sEntry, errmsg, *p_str);

  }
  else if (tinfo == typeid(uint64_t)) {
    // -- Typeid: uint64_t --
    value = (uint64_t) -1;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(device_id)->sysfs_get(entry.sSubDevice, entry.sEntry, errmsg, uint64Vector);
    if (!uint64Vector.empty()) {
      value = uint64Vector[0];
    }

  }
  else if (tinfo == typeid(bool)) {
    // -- Typeid: bool --
    value = (bool) 0;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(device_id)->sysfs_get(entry.sSubDevice, entry.sEntry, errmsg, uint64Vector);
    if (!uint64Vector.empty()) {
      value = (bool) uint64Vector[0];
    }

  }
  else if (tinfo == typeid(std::vector<std::string>)) {
    // -- Typeid: std::vector<std::string>
    value = std::vector<std::string>();
    auto p_strvec = boost::any_cast<std::vector<std::string>>(&value);
    pcidev::get_dev(device_id)->sysfs_get(entry.sSubDevice, entry.sEntry, errmsg, *p_strvec);

  }
  else {
    errmsg = boost::str( boost::format("Error: Unsupported query_device return type: '%s'") % tinfo.name());
  }

  if (!errmsg.empty()) {
    throw std::runtime_error(errmsg);
  }
}

device_linux::
device_linux(id_type device_id, bool user)
  : device_pcie(device_id, user)
{
}

void
device_linux::
read_dma_stats(boost::property_tree::ptree& pt) const
{
  auto handle = get_device_handle();

  xclDeviceUsage devstat = { 0 };
  xclGetUsageInfo(handle, &devstat);

  boost::property_tree::ptree pt_channels;
  for (unsigned int idx = 0; idx < XCL_DEVICE_USAGE_COUNT; ++idx) {
    boost::property_tree::ptree pt_dma;
    pt_dma.put( "id", std::to_string(get_device_id()));
    pt_dma.put( "h2c", unitConvert(devstat.h2c[idx]) );
    pt_dma.put( "c2h", unitConvert(devstat.c2h[idx]) );

    // Create our array of data
    pt_channels.push_back(std::make_pair("", pt_dma));
  }

  pt.add_child( "transfer_metrics.channels", pt_channels);
}

void
device_linux::
read(uint64_t offset, void* buf, uint64_t len) const
{
  if (auto err = pcidev::get_dev(get_device_id())->pcieBarRead(offset, buf, len))
    throw error(err, "read failed");
}

void
device_linux::
write(uint64_t offset, const void* buf, uint64_t len) const
{
  if (auto err = pcidev::get_dev(get_device_id())->pcieBarWrite(offset, buf, len))
    throw error(err, "write failed");
}

void
device_linux::
auto_flash(const std::string& shell, const std::string& id, bool force) const
{
  std::cout << "TO-DO: auto_flash\n";
}

void
device_linux::
reset_shell() const
{
  std::cout << "TO-DO: reset_shell\n";
}

void
device_linux::
update_shell(const std::string& flashType, const std::string& primary, const std::string& secondary) const
{
  std::cout << "TO-DO: update_shell\n";
}

void
device_linux::
update_SC(const std::string& file) const
{
  std::cout << "TO-DO: update_SC\n";
}

} // xrt_core
