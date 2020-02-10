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
#include "core/common/query_requests.h"

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

// Sysfs accessor
static void
sysfs(const device_type* device, const std::type_info& tinfo, boost::any& value,
      const std::string& subdev, const std::string& entry)
{
  auto device_id = device->get_device_id();
  auto is_user = device->is_userpf();
  std::string errmsg;
  // ignore the errmsg from sysfs_get. Some nodes do not exist but we don't
  // want to throw an error. this function handles invalid queries nicely
  std::string ignore_err;

  if (tinfo == typeid(std::string)) {
    // -- Typeid: std::string --
    value = std::string("");
    auto p_str = boost::any_cast<std::string>(&value);
    pcidev::get_dev(device_id, is_user)->sysfs_get(subdev, entry, ignore_err, *p_str);

  }
  else if (tinfo == typeid(uint64_t)) {
    // -- Typeid: uint64_t --
    value = (uint64_t) -1;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(device_id, is_user)->sysfs_get(subdev, entry, ignore_err, uint64Vector);
    if (!uint64Vector.empty()) {
      value = uint64Vector[0];
    }

  }
  else if (tinfo == typeid(bool)) {
    // -- Typeid: bool --
    value = (bool) 0;
    std::vector<uint64_t> uint64Vector;
    pcidev::get_dev(device_id, is_user)->sysfs_get(subdev, entry, ignore_err, uint64Vector);
    if (!uint64Vector.empty()) {
      value = (bool) uint64Vector[0];
    }

  }
  else if (tinfo == typeid(std::vector<std::string>)) {
    // -- Typeid: std::vector<std::string>
    value = std::vector<std::string>();
    auto p_strvec = boost::any_cast<std::vector<std::string>>(&value);
    pcidev::get_dev(device_id, is_user)->sysfs_get(subdev, entry, ignore_err, *p_strvec);

  }
  else {
    errmsg = boost::str( boost::format("Error: Unsupported query_device return type: '%s'") % tinfo.name());
  }

  if (!errmsg.empty()) {
    throw std::runtime_error(errmsg);
  }
}

//sysfs mgmt wrapper
static void
sysfs_mgmt(const device_type* device, const std::type_info& tinfo, boost::any& value,
      const std::string& subdev, const std::string& entry)
{
  sysfs(device, tinfo, value, subdev, entry);
}

//sysfs mgmt wrapper
static void
sysfs_user(const device_type* device, const std::type_info& tinfo, boost::any& value,
      const std::string& subdev, const std::string& entry)
{
  sysfs(device, tinfo, value, subdev, entry);
}

namespace sp = std::placeholders;
static std::map<qr_type, query_entry> query_table = {
  { qr_type::QR_DMA_THREADS_RAW,           {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "dma", "channel_stat_raw")}},
  { qr_type::QR_ROM_VBNV,                  {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "rom", "VBNV")}},
  { qr_type::QR_ROM_DDR_BANK_SIZE,         {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "rom", "ddr_bank_size")}},
  { qr_type::QR_ROM_DDR_BANK_COUNT_MAX,    {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "rom", "ddr_bank_count_max")}},
  { qr_type::QR_ROM_FPGA_NAME,             {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "rom", "FPGA")}},
  { qr_type::QR_ROM_RAW,                   {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "rom", "raw")}},
  { qr_type::QR_ROM_UUID,                  {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "rom", "uuid")}},
  { qr_type::QR_ROM_TIME_SINCE_EPOCH,      {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "rom", "timestamp")}},
  { qr_type::QR_XMC_VERSION,               {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "version")}},
  { qr_type::QR_XMC_SERIAL_NUM,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "serial_num")}},
  { qr_type::QR_XMC_MAX_POWER,             {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "max_power")}},
  { qr_type::QR_XMC_BMC_VERSION,           {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "bmc_ver")}},
  { qr_type::QR_XMC_STATUS,                {std::bind(sysfs_mgmt, sp::_1, sp::_2, sp::_3, "xmc", "status")}},
  { qr_type::QR_XMC_REG_BASE,              {std::bind(sysfs_mgmt, sp::_1, sp::_2, sp::_3, "xmc", "reg_base")}},
  { qr_type::QR_DNA_SERIAL_NUM,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "dna", "dna")}},
  { qr_type::QR_CLOCK_FREQS,               {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "icap", "clock_freqs")}},
  { qr_type::QR_IDCODE,                    {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "icap", "idcode")}},
  { qr_type::QR_STATUS_MIG_CALIBRATED,     {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "", "mig_calibration")}},
  { qr_type::QR_STATUS_P2P_ENABLED,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "", "p2p_enable")}},
  { qr_type::QR_TEMP_CARD_TOP_FRONT,       {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_se98_temp0")}},
  { qr_type::QR_TEMP_CARD_TOP_REAR,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_se98_temp1")}},
  { qr_type::QR_TEMP_CARD_BOTTOM_FRONT,    {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_se98_temp2")}},
  { qr_type::QR_TEMP_FPGA,                 {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_fpga_temp")}},
  { qr_type::QR_FAN_TRIGGER_CRITICAL_TEMP, {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_fan_temp")}},
  { qr_type::QR_FAN_FAN_PRESENCE,          {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "fan_presence")}},
  { qr_type::QR_FAN_SPEED_RPM,             {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_fan_rpm")}},
  { qr_type::QR_DDR_TEMP_0,                {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp0")}},
  { qr_type::QR_DDR_TEMP_1,                {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp1")}},
  { qr_type::QR_DDR_TEMP_2,                {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp2")}},
  { qr_type::QR_DDR_TEMP_3,                {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_temp3")}},
  { qr_type::QR_HBM_TEMP,                  {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_hbm_temp")}},
  { qr_type::QR_CAGE_TEMP_0,               {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp0")}},
  { qr_type::QR_CAGE_TEMP_1,               {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp1")}},
  { qr_type::QR_CAGE_TEMP_2,               {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp2")}},
  { qr_type::QR_CAGE_TEMP_3,               {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_cage_temp3")}},
  { qr_type::QR_12V_PEX_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_pex_vol")}},
  { qr_type::QR_12V_PEX_MILLIAMPS,         {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_pex_curr")}},
  { qr_type::QR_12V_AUX_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_aux_vol")}},
  { qr_type::QR_12V_AUX_MILLIAMPS,         {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_aux_curr")}},
  { qr_type::QR_3V3_PEX_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_pex_vol")}},
  { qr_type::QR_3V3_AUX_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_aux_vol")}},
  { qr_type::QR_DDR_VPP_BOTTOM_MILLIVOLTS, {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_vpp_btm")}},
  { qr_type::QR_DDR_VPP_TOP_MILLIVOLTS,    {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_ddr_vpp_top")}},

  { qr_type::QR_5V5_SYSTEM_MILLIVOLTS,     {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_sys_5v5")}},
  { qr_type::QR_1V2_VCC_TOP_MILLIVOLTS,    {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_1v2_top")}},
  { qr_type::QR_1V2_VCC_BOTTOM_MILLIVOLTS, {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vcc1v2_btm")}},
  { qr_type::QR_1V8_MILLIVOLTS,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_1v8")}},
  { qr_type::QR_0V85_MILLIVOLTS,           {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_0v85")}},
  { qr_type::QR_0V9_VCC_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_mgt0v9avcc")}},
  { qr_type::QR_12V_SW_MILLIVOLTS,         {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_12v_sw")}},
  { qr_type::QR_MGT_VTT_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_mgtavtt")}},
  { qr_type::QR_INT_VCC_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vccint_vol")}},
  { qr_type::QR_INT_VCC_MILLIAMPS,         {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vccint_curr")}},

  { qr_type::QR_3V3_PEX_MILLIAMPS,         {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_pex_curr")}},
  { qr_type::QR_0V85_MILLIAMPS,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_0v85_curr")}},
  { qr_type::QR_3V3_VCC_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_3v3_vcc_vol")}},
  { qr_type::QR_HBM_1V2_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_hbm_1v2_vol")}},
  { qr_type::QR_2V5_VPP_MILLIVOLTS,        {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vpp2v5_vol")}},
  { qr_type::QR_INT_BRAM_VCC_MILLIVOLTS,   {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_vccint_bram_vol")}},

  { qr_type::QR_FIREWALL_DETECT_LEVEL,     {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "firewall", "detected_level")}},
  { qr_type::QR_FIREWALL_STATUS,           {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "firewall", "detected_status")}},
  { qr_type::QR_FIREWALL_TIME_SEC,         {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "firewall", "detected_time")}},

  { qr_type::QR_POWER_MICROWATTS,          {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, "xmc", "xmc_power")}},

  // { qr_type::QR_MIG_ECC_ENABLED,           {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, sp::_4, "ecc_enabled")}},
  // { qr_type::QR_MIG_ECC_STATUS,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, sp::_4, "ecc_status")}},
  // { qr_type::QR_MIG_ECC_CE_CNT,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, sp::_4, "ecc_ce_cnt")}},
  // { qr_type::QR_MIG_ECC_UE_CNT,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, sp::_4, "ecc_ue_cnt")}},
  // { qr_type::QR_MIG_ECC_CE_FFA,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, sp::_4, "ecc_ce_ffa")}},
  // { qr_type::QR_MIG_ECC_UE_FFA,            {std::bind(sysfs_user, sp::_1, sp::_2, sp::_3, sp::_4, "ecc_ue_ffa")}},

  { qr_type::QR_FLASH_BAR_OFFSET,          {std::bind(sysfs_mgmt, sp::_1, sp::_2, sp::_3, "flash", "bar_off")}},
  { qr_type::QR_IS_MFG,                    {std::bind(sysfs_mgmt, sp::_1, sp::_2, sp::_3, "", "mfg")}},
  { qr_type::QR_F_FLASH_TYPE,              {std::bind(sysfs_mgmt, sp::_1, sp::_2, sp::_3, "flash", "flash_type" )}},
  { qr_type::QR_FLASH_TYPE,                {std::bind(sysfs_mgmt, sp::_1, sp::_2, sp::_3, "", "flash_type" )}},
  { qr_type::QR_BOARD_NAME,                {std::bind(sysfs_mgmt, sp::_1, sp::_2, sp::_3, "", "board_name" )}}
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

namespace {

namespace query = xrt_core::query;
using pdev = std::shared_ptr<pcidev::pci_device>;
using key_type = query::key_type;

inline pdev
get_pcidev(const xrt_core::device* device)
{
  return pcidev::get_dev(device->get_device_id(), device->is_userpf());
}

struct bdf
{
  using result_type = query::pcie_bdf::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    auto pdev = get_pcidev(device);
    return std::make_tuple(pdev->bus,pdev->dev,pdev->func);
  }
};


// Specialize for other value types.
template <typename ValueType>
struct sysfs_fcn
{
  static ValueType
  get(const pdev& dev, const char* subdev, const char* entry)
  {
    std::string err;
    ValueType value;
    dev->sysfs_get(subdev, entry, err, value, static_cast<ValueType>(-1));
    if (!err.empty())
      throw std::runtime_error(err);
    return value;
  }
};

template <typename QueryRequestType>
struct sysfs_getter : QueryRequestType
{
  const char* entry;
  const char* subdev;

  sysfs_getter(const char* e, const char* s)
    : entry(e), subdev(s)
  {}

  boost::any
  get(const xrt_core::device* device) const
  {
    return sysfs_fcn<typename QueryRequestType::result_type>
      ::get(get_pcidev(device), entry, subdev);
  }
};

template <typename QueryRequestType, typename Getter>
struct function0_getter : QueryRequestType
{
  boost::any
  get(const xrt_core::device* device) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k);
  }
};

static std::map<xrt_core::query::key_type, std::unique_ptr<query::request>> query_tbl;

template <typename QueryRequestType>
static void
emplace_sysfs_request(const char* entry, const char* subdev)
{
  auto x = QueryRequestType::key;
  query_tbl.emplace(x, std::make_unique<sysfs_getter<QueryRequestType>>(entry, subdev));
}

template <typename QueryRequestType, typename Getter>
static void
emplace_func0_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_getter<QueryRequestType, Getter>>());
}

static void
initialize_query_table()
{
  emplace_sysfs_request<query::pcie_vendor>              ("", "vendor");
  emplace_sysfs_request<query::pcie_device>              ("", "device");
  emplace_sysfs_request<query::pcie_subsystem_vendor>    ("", "subsystem_vendor");
  emplace_sysfs_request<query::pcie_subsystem_id>        ("", "subsystem_device");
  emplace_sysfs_request<query::pcie_link_speed>          ("", "link_speed");
  emplace_sysfs_request<query::pcie_express_lane_width>  ("", "link_width");
  emplace_func0_request<query::pcie_bdf,                 bdf>();
}

struct X { X() { initialize_query_table(); }};
static X x;

}

namespace xrt_core {

const query::request&
device_linux::
lookup_query(query::key_type query_key) const
{
  auto it = query_tbl.find(query_key);

  if (it == query_tbl.end()) {
    using qtype = std::underlying_type<query::key_type>::type;
    std::string err = boost::str( boost::format("The given query request ID (%d) is not supported on Linux.")
                                  % static_cast<qtype>(query_key));
    throw std::runtime_error(err);
  }

  return *(it->second);
}

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
  if (auto err = pcidev::get_dev(get_device_id(), false)->pcieBarRead(offset, buf, len))
    throw error(err, "read failed");
}

void
device_linux::
write(uint64_t offset, const void* buf, uint64_t len) const
{
  if (auto err = pcidev::get_dev(get_device_id(), false)->pcieBarWrite(offset, buf, len))
    throw error(err, "write failed");
}

} // xrt_core
