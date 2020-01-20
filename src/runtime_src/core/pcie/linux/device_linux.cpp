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
#include <functional>
#include "boost/format.hpp"

namespace {

using device_type = xrt_core::device_linux;
using qr_type = xrt_core::device::QueryRequest;

// A query entry is a callback function.
struct query_entry
{
  std::function<void(const device_type*, const std::type_info&, boost::any&)> m_fcn;
};

// BDF for a device
static void
bdf(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto pdev = pcidev::get_dev(device->get_device_id());

  switch (qr) {
  case qr_type::QR_PCIE_BDF_BUS:
    value = static_cast<uint16_t>(pdev->bus);
    return;
  case qr_type::QR_PCIE_BDF_DEVICE:
    value = static_cast<uint16_t>(pdev->dev);
    return;
  case qr_type::QR_PCIE_BDF_FUNCTION:
    value = static_cast<uint16_t>(pdev->func);
    return;
  default:
    throw std::runtime_error("device_linux::bdf() unexpected qr " + std::to_string(qr));
  }
}

// Sysfs accessor
static void
sysfs(const device_type* device, const std::type_info& tinfo, boost::any& value,
      const std::string& subdev, const std::string& entry)
{
  auto device_id = device->get_device_id();
  std::string errmsg;

  if (tinfo == typeid(std::string)) {
    // -- Typeid: std::string --
    value = std::string("");
    auto p_str = boost::any_cast<std::string>(&value);
    pcidev::get_dev(device_id)->sysfs_get(subdev, entry, errmsg, *p_str);

  }
  else if (tinfo == typeid(uint64_t)) {
    // -- Typeid: uint64_t --
    value = (uint64_t) -1;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(device_id)->sysfs_get(subdev, entry, errmsg, uint64Vector);
    if (!uint64Vector.empty()) {
      value = uint64Vector[0];
    }

  }
  else if (tinfo == typeid(bool)) {
    // -- Typeid: bool --
    value = (bool) 0;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(device_id)->sysfs_get(subdev, entry, errmsg, uint64Vector);
    if (!uint64Vector.empty()) {
      value = (bool) uint64Vector[0];
    }

  }
  else if (tinfo == typeid(std::vector<std::string>)) {
    // -- Typeid: std::vector<std::string>
    value = std::vector<std::string>();
    auto p_strvec = boost::any_cast<std::vector<std::string>>(&value);
    pcidev::get_dev(device_id)->sysfs_get(subdev, entry, errmsg, *p_strvec);

  }
  else {
    errmsg = boost::str( boost::format("Error: Unsupported query_device return type: '%s'") % tinfo.name());
  }

  if (!errmsg.empty()) {
    throw std::runtime_error(errmsg);
  }
}

namespace sp = std::placeholders;
static std::map<qr_type, query_entry> query_table = {
  { qr_type::QR_PCIE_VENDOR,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "vendor")}},
  { qr_type::QR_PCIE_DEVICE,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "device")}},
  { qr_type::QR_PCIE_SUBSYSTEM_VENDOR,     {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "subsystem_vendor")}},
  { qr_type::QR_PCIE_SUBSYSTEM_ID,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "subsystem_device")}},
  { qr_type::QR_PCIE_LINK_SPEED,           {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "link_speed")}},
  { qr_type::QR_PCIE_EXPRESS_LANE_WIDTH,   {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "link_width")}},
  { qr_type::QR_PCIE_BDF_BUS,              {std::bind(bdf, sp::_1, qr_type::QR_PCIE_BDF_BUS, sp::_2, sp::_3)}},
  { qr_type::QR_PCIE_BDF_DEVICE,           {std::bind(bdf, sp::_1, qr_type::QR_PCIE_BDF_DEVICE, sp::_2, sp::_3)}},
  { qr_type::QR_PCIE_BDF_FUNCTION,         {std::bind(bdf, sp::_1, qr_type::QR_PCIE_BDF_FUNCTION, sp::_2, sp::_3)}},
  { qr_type::QR_DMA_THREADS_RAW,           {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "dma", "channel_stat_raw")}},
  { qr_type::QR_ROM_VBNV,                  {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "rom", "VBNV")}},
  { qr_type::QR_ROM_DDR_BANK_SIZE,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "rom", "ddr_bank_size")}},
  { qr_type::QR_ROM_DDR_BANK_COUNT_MAX,    {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "rom", "ddr_bank_count_max")}},
  { qr_type::QR_ROM_FPGA_NAME,             {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "rom", "FPGA")}},
  { qr_type::QR_ROM_RAW,                   {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "rom", "raw")}},
  { qr_type::QR_ROM_UUID,                  {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "rom", "uuid")}},
  { qr_type::QR_XMC_VERSION,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "version")}},
  { qr_type::QR_XMC_SERIAL_NUM,            {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "serial_num")}},
  { qr_type::QR_XMC_MAX_POWER,             {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "max_power")}},
  { qr_type::QR_XMC_BMC_VERSION,           {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "bmc_ver")}},
  { qr_type::QR_XMC_STATUS,                {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "status")}},
  { qr_type::QR_XMC_REG_BASE,              {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "reg_base")}},
  { qr_type::QR_DNA_SERIAL_NUM,            {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "dna", "dna")}},
  { qr_type::QR_CLOCK_FREQS,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "icap", "clock_freqs")}},
  { qr_type::QR_IDCODE,                    {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "icap", "idcode")}},
  { qr_type::QR_STATUS_MIG_CALIBRATED,     {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "mig_calibration")}},
  { qr_type::QR_STATUS_P2P_ENABLED,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "p2p_enable")}},
  { qr_type::QR_TEMP_CARD_TOP_FRONT,       {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_se98_temp0")}},
  { qr_type::QR_TEMP_CARD_TOP_REAR,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_se98_temp1")}},
  { qr_type::QR_TEMP_CARD_BOTTOM_FRONT,    {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_se98_temp2")}},
  { qr_type::QR_TEMP_FPGA,                 {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_fpga_temp")}},
  { qr_type::QR_FAN_TRIGGER_CRITICAL_TEMP, {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_fan_temp")}},
  { qr_type::QR_FAN_FAN_PRESENCE,          {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "fan_presence")}},
  { qr_type::QR_FAN_SPEED_RPM,             {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_fan_rpm")}},
  { qr_type::QR_DDR_TEMP_0,                {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp0")}},
  { qr_type::QR_DDR_TEMP_1,                {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp1")}},
  { qr_type::QR_DDR_TEMP_2,                {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp2")}},
  { qr_type::QR_DDR_TEMP_3,                {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp3")}},
  { qr_type::QR_HBM_TEMP,                  {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_hbm_temp")}},
  { qr_type::QR_CAGE_TEMP_0,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp0")}},
  { qr_type::QR_CAGE_TEMP_1,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp1")}},
  { qr_type::QR_CAGE_TEMP_2,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp2")}},
  { qr_type::QR_CAGE_TEMP_3,               {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp3")}},
  { qr_type::QR_12V_PEX_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_pex_vol")}},
  { qr_type::QR_12V_PEX_MILLIAMPS,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_pex_curr")}},
  { qr_type::QR_12V_AUX_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_aux_vol")}},
  { qr_type::QR_12V_AUX_MILLIAMPS,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_aux_curr")}},
  { qr_type::QR_3V3_PEX_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_pex_vol")}},
  { qr_type::QR_3V3_AUX_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_aux_vol")}},
  { qr_type::QR_DDR_VPP_BOTTOM_MILLIVOLTS, {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_vpp_btm")}},
  { qr_type::QR_DDR_VPP_TOP_MILLIVOLTS,    {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_vpp_top")}},

  { qr_type::QR_5V5_SYSTEM_MILLIVOLTS,     {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_sys_5v5")}},
  { qr_type::QR_1V2_VCC_TOP_MILLIVOLTS,    {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_1v2_top")}},
  { qr_type::QR_1V2_VCC_BOTTOM_MILLIVOLTS, {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vcc1v2_btm")}},
  { qr_type::QR_1V8_MILLIVOLTS,            {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_1v8")}},
  { qr_type::QR_0V85_MILLIVOLTS,           {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_0v85")}},
  { qr_type::QR_0V9_VCC_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_mgt0v9avcc")}},
  { qr_type::QR_12V_SW_MILLIVOLTS,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_sw")}},
  { qr_type::QR_MGT_VTT_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_mgtavtt")}},
  { qr_type::QR_INT_VCC_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vccint_vol")}},
  { qr_type::QR_INT_VCC_MILLIAMPS,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vccint_curr")}},

  { qr_type::QR_3V3_PEX_MILLIAMPS,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_pex_curr")}},
  { qr_type::QR_0V85_MILLIAMPS,            {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_0v85_curr")}},
  { qr_type::QR_3V3_VCC_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_vcc_vol")}},
  { qr_type::QR_HBM_1V2_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_hbm_1v2_vol")}},
  { qr_type::QR_2V5_VPP_MILLIVOLTS,        {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vpp2v5_vol")}},
  { qr_type::QR_INT_BRAM_VCC_MILLIVOLTS,   {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vccint_bram_vol")}},

  { qr_type::QR_FIREWALL_DETECT_LEVEL,     {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "firewall", "detected_level")}},
  { qr_type::QR_FIREWALL_STATUS,           {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "firewall", "detected_status")}},
  { qr_type::QR_FIREWALL_TIME_SEC,         {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "firewall", "detected_time")}},

  { qr_type::QR_POWER_MICROWATTS,          {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "xmc", "xmc_power")}},

  { qr_type::QR_FLASH_BAR_OFFSET,          {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "flash", "bar_off")}},
  { qr_type::QR_IS_MFG,                    {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "mfg")}},
  { qr_type::QR_F_FLASH_TYPE,              {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "flash", "flash_type" )}},
  { qr_type::QR_FLASH_TYPE,                {std::bind(sysfs, sp::_1, sp::_2, sp::_3, "", "flash_type" )}}
};

const query_entry&
get_query_entry(qr_type qr)
{
  // Find the translation entry
  auto it = query_table.find(qr);

  if (it == query_table.end()) {
    std::string errMsg = boost::str( boost::format("The given query request ID (%d) is not supported.") % qr);
    throw xrt_core::no_such_query(qr, errMsg);
  }

  return it->second;
}

}

namespace xrt_core {

void
device_linux::
query(QueryRequest qr, const std::type_info& tinfo, boost::any& value) const
{
  // Initialize return data to being empty container.
  // Note: CentOS Boost 1.53 doesn't support the clear() method.
  boost::any anyEmpty;
  value.swap(anyEmpty);

  // Get the sysdev and entry values to call
  auto& entry = ::get_query_entry(qr);
  if (!entry.m_fcn)
    throw std::runtime_error("Unexpected error, exception should already have been thrown");

  entry.m_fcn(this, tinfo, value);
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

} // xrt_core
