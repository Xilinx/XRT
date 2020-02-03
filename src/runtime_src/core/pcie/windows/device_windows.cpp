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
#define XRT_CORE_PCIE_WINDOWS_SOURCE
#define XCL_DRIVER_DLL_EXPORT
#include "device_windows.h"
#include "mgmt.h"
#include "shim.h"
#include "common/utils.h"
#include "xrt.h"
#include "xclfeatures.h"

//#include "core/pcie/driver/windows/include/XoclUser_INTF.h"
//#include "core/pcie/driver/windows/include/XoclMgmt_INTF.h"

#include "boost/format.hpp"
#include <string>
#include <iostream>
#include <map>
#include <mutex>

#pragma warning(disable : 4100 4996)

namespace {

constexpr size_t
operator"" _k (unsigned long long value)
{
  return value * 1024;
}

using device_type = xrt_core::device_windows;
using qr_type = xrt_core::device::QueryRequest;

static void
flash_type(const device_type*, qr_type, const std::type_info&, boost::any& value)
{
  value = std::string("spi");
}

static void
xmc(const device_type*, qr_type qr, const std::type_info&, boost::any& value)
{
  if(qr == xrt_core::device::QR_XMC_STATUS)
    value = (uint64_t)1;
  if(value.empty())
    throw std::runtime_error("Invalid query value");
}

void
mfg(const device_type*, qr_type, const std::type_info&, boost::any& value)
{
  value = false;
}

void
board_name(const device_type*, qr_type, const std::type_info&, boost::any& value)
{
  value = std::string("TO-DO");
}

static void
rom(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_feature_rom_header = [](const device_type* dev) {
    FeatureRomHeader hdr = {0};
    if (auto mhdl = dev->get_mgmt_handle())
      mgmtpf::get_rom_info(mhdl, &hdr);
    else if (auto uhdl = dev->get_user_handle())
      userpf::get_rom_info(uhdl, &hdr);
    else
      throw std::runtime_error("No device handle");
    return hdr;
  };

  static std::map<const device_type*, FeatureRomHeader> hdrmap;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = hdrmap.find(device);
  if (it == hdrmap.end()) {
    auto ret = hdrmap.emplace(device,init_feature_rom_header(device));
    it = ret.first;
  }

  auto& hdr= (*it).second;

  switch (qr) {
  case qr_type::QR_ROM_VBNV:
    value = std::string(reinterpret_cast<const char*>(hdr.VBNVName));
    return;
  case qr_type::QR_ROM_DDR_BANK_SIZE:
    value = static_cast<uint64_t>(hdr.DDRChannelSize);
    return;
  case qr_type::QR_ROM_DDR_BANK_COUNT_MAX:
    value = static_cast<uint64_t>(hdr.DDRChannelCount);
    return;
  case qr_type::QR_ROM_FPGA_NAME:
    value = std::string(reinterpret_cast<const char*>(hdr.FPGAPartName));
    return;
  }

  if (device->get_user_handle())
    throw std::runtime_error("device_windows::rom() unexpected qr("
                             + std::to_string(qr)
                             + ") for userpf");

  switch (qr) {
  case qr_type::QR_ROM_UUID:
    value = std::string(reinterpret_cast<const char*>(hdr.uuid),16);
    return;
  case qr_type::QR_ROM_TIME_SINCE_EPOCH:
    value = hdr.TimeSinceEpoch;
    return;
  default:
    throw std::runtime_error("device_windows::rom() unexpected qr " + std::to_string(qr));
  }
}

static void
info_user(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_device_info = [](const device_type* dev) {
    XOCL_DEVICE_INFORMATION info = { 0 };
    userpf::get_device_info(dev->get_user_handle(), &info);
    return info;
  };

  static std::map<const device_type*, XOCL_DEVICE_INFORMATION> info_map;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = info_map.find(device);
  if (it == info_map.end()) {
    auto ret = info_map.emplace(device,init_device_info(device));
    it = ret.first;
  }

  auto& info = (*it).second;

  switch (qr) {
  case qr_type::QR_PCIE_VENDOR:
    value = info.Vendor;
    return;
  case qr_type::QR_PCIE_DEVICE:
    value = info.Device;
    return;
  case qr_type::QR_PCIE_SUBSYSTEM_VENDOR:
    value = info.SubsystemVendor;
    return;
  case qr_type::QR_PCIE_SUBSYSTEM_ID:
    value = info.SubsystemDevice;
    return;
  default:
    throw std::runtime_error("device_windows::info_user() unexpected qr " + std::to_string(qr));
  }
}

static void
info_mgmt(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_device_info = [](const device_type* dev) {
    XCLMGMT_IOC_DEVICE_INFO info = { 0 };
    mgmtpf::get_device_info(dev->get_mgmt_handle(), &info);
    return info;
  };

  static std::map<const device_type*, XCLMGMT_IOC_DEVICE_INFO> info_map;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = info_map.find(device);
  if (it == info_map.end()) {
    auto ret = info_map.emplace(device,init_device_info(device));
    it = ret.first;
  }

  auto& info = (*it).second;

  switch (qr) {
  case qr_type::QR_PCIE_VENDOR:
    value = static_cast<uint64_t>(info.pcie_info.vendor);
    return;
  case qr_type::QR_PCIE_DEVICE:
    value = static_cast<uint64_t>(info.pcie_info.device);
    return;
  case qr_type::QR_PCIE_SUBSYSTEM_VENDOR:
    value = static_cast<uint64_t>(info.pcie_info.subsystem_vendor);
    return;
  case qr_type::QR_PCIE_SUBSYSTEM_ID:
    value = static_cast<uint64_t>(info.pcie_info.subsystem_device);
    return;
  default:
    throw std::runtime_error("device_windows::info_mgmt() unexpected qr " + std::to_string(qr));
  }
}

static void
sensor_info(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_sensor_info = [](const device_type* dev) {
    xcl_sensor info = { 0 };
    userpf::get_sensor_info(dev->get_user_handle(), &info);
    return info;
  };

  static std::map<const device_type*, xcl_sensor> info_map;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = info_map.find(device);
  if (it == info_map.end()) {
    auto ret = info_map.emplace(device,init_sensor_info(device));
    it = ret.first;
  }

  const xcl_sensor& info = (*it).second;

  switch (qr) {
  case qr_type::QR_12V_PEX_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_12v_pex);
    return;
  case qr_type::QR_12V_AUX_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_12v_aux);
    return;
  case qr_type::QR_12V_PEX_MILLIAMPS:
    value = static_cast<uint64_t>(info.cur_12v_pex);
    return;
  case qr_type::QR_12V_AUX_MILLIAMPS:
    value = static_cast<uint64_t>(info.cur_12v_aux);
    return;
  case qr_type::QR_3V3_PEX_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_3v3_pex);
    return;
    case qr_type::QR_3V3_AUX_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_3v3_aux);
    return;
  case qr_type::QR_DDR_VPP_BOTTOM_MILLIVOLTS:
    value = static_cast<uint64_t>(info.ddr_vpp_btm);
    return;
  case qr_type::QR_DDR_VPP_TOP_MILLIVOLTS:
    value = static_cast<uint64_t>(info.ddr_vpp_top);
    return;
  case qr_type::QR_5V5_SYSTEM_MILLIVOLTS:
    value = static_cast<uint64_t>(info.sys_5v5);
    return;
  case qr_type::QR_1V2_VCC_TOP_MILLIVOLTS:
    value = static_cast<uint64_t>(info.top_1v2);
    return;
    case qr_type::QR_1V2_VCC_BOTTOM_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vcc1v2_btm);
    return;
  case qr_type::QR_1V8_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_1v8);
    return;
  case qr_type::QR_0V85_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_0v85);
    return;
  case qr_type::QR_0V9_VCC_MILLIVOLTS:
    value = static_cast<uint64_t>(info.mgt0v9avcc);
    return;
  case qr_type::QR_12V_SW_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_12v_sw);
    return;
    case qr_type::QR_MGT_VTT_MILLIVOLTS:
    value = static_cast<uint64_t>(info.mgtavtt);
    return;
  case qr_type::QR_INT_VCC_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vccint_vol);
    return;
  case qr_type::QR_INT_VCC_MILLIAMPS:
    value = static_cast<uint64_t>(info.vccint_curr);
    return;
  case qr_type::QR_3V3_PEX_MILLIAMPS:
    value = static_cast<uint64_t>(info.cur_3v3_pex);
    return;
  case qr_type::QR_0V85_MILLIAMPS:
    value = static_cast<uint64_t>(info.cur_0v85);
    return;
    case qr_type::QR_3V3_VCC_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_3v3_vcc);
    return;
  case qr_type::QR_HBM_1V2_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_1v2_hbm);
    return;
  case qr_type::QR_2V5_VPP_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vol_2v5_vpp);
    return;
  case qr_type::QR_INT_BRAM_VCC_MILLIVOLTS:
    value = static_cast<uint64_t>(info.vccint_bram);
    return;
  case qr_type::QR_TEMP_CARD_TOP_FRONT:
    value = static_cast<uint64_t>(info.se98_temp0);
    return;
  case qr_type::QR_TEMP_CARD_TOP_REAR:
    value = static_cast<uint64_t>(info.se98_temp1);
    return;
  case qr_type::QR_TEMP_CARD_BOTTOM_FRONT:
    value = static_cast<uint64_t>(info.se98_temp2);
    return;
  case qr_type::QR_TEMP_FPGA:
    value = static_cast<uint64_t>(info.fpga_temp);
    return;
  case qr_type::QR_FAN_TRIGGER_CRITICAL_TEMP:
    value = static_cast<uint64_t>(info.fan_temp);
    return;
  case qr_type::QR_FAN_SPEED_RPM:
    value = static_cast<uint64_t>(info.fan_rpm);
    return;
  case qr_type::QR_DDR_TEMP_0:
    value = static_cast<uint64_t>(info.dimm_temp0);
    return;
  case qr_type::QR_DDR_TEMP_1:
    value = static_cast<uint64_t>(info.dimm_temp1);
    return;
  case qr_type::QR_DDR_TEMP_2:
    value = static_cast<uint64_t>(info.dimm_temp2);
    return;
  case qr_type::QR_DDR_TEMP_3:
    value = static_cast<uint64_t>(info.dimm_temp3);
    return;
  case qr_type::QR_HBM_TEMP:
    value = static_cast<uint64_t>(info.hbm_temp0);
    return;
  case qr_type::QR_CAGE_TEMP_0:
    value = static_cast<uint64_t>(info.cage_temp0);
    return;
  case qr_type::QR_CAGE_TEMP_1:
    value = static_cast<uint64_t>(info.cage_temp1);
    return;
  case qr_type::QR_CAGE_TEMP_2:
    value = static_cast<uint64_t>(info.cage_temp2);
    return;
  case qr_type::QR_CAGE_TEMP_3:
    value = static_cast<uint64_t>(info.cage_temp3);
    return;
  case qr_type::QR_XMC_VERSION:
    value = static_cast<uint64_t>(info.version);
    return;
  default:
    throw std::runtime_error("device_windows::sensor_info() unexpected qr " + std::to_string(qr));
  }
}

static void
icap_info(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_icap_info = [](const device_type* dev) {
    xcl_hwicap info = { 0 };
    userpf::get_icap_info(dev->get_user_handle(), &info);
    return info;
  };

  static std::map<const device_type*, xcl_hwicap> info_map;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = info_map.find(device);
  if (it == info_map.end()) {
    auto ret = info_map.emplace(device,init_icap_info(device));
    it = ret.first;
  }

  const xcl_hwicap& info = (*it).second;

  switch (qr) {
  case qr_type::QR_CLOCK_FREQS:
    value = std::vector<std::string>{ std::to_string(info.freq_0), std::to_string(info.freq_1), 
                                       std::to_string(info.freq_2), std::to_string(info.freq_3) };
    return;
  case qr_type::QR_IDCODE:
    value = info.idcode;
    return;
  case qr_type::QR_STATUS_MIG_CALIBRATED:
    value = info.mig_calib;
    return;
  default:
    throw std::runtime_error("device_windows::icap() unexpected qr " + std::to_string(qr));
  }
  // No query for freq_cntr_0, freq_cntr_1, freq_cntr_2, freq_cntr_3 and uuid
}

static void
board_info(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_board_info = [](const device_type* dev) {
    xcl_board_info info = { 0 };
    userpf::get_board_info(dev->get_user_handle(), &info);
    return info;
  };

  static std::map<const device_type*, xcl_board_info> info_map;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = info_map.find(device);
  if (it == info_map.end()) {
    auto ret = info_map.emplace(device,init_board_info(device));
    it = ret.first;
  }

  const xcl_board_info& info = (*it).second;

  switch (qr) {
  case qr_type::QR_XMC_SERIAL_NUM:
    value = std::string(reinterpret_cast<const char*>(info.serial_num));
    return;
  case qr_type::QR_XMC_BMC_VERSION:
    value = std::string(reinterpret_cast<const char*>(info.bmc_ver));
    return;
  case qr_type::QR_XMC_MAX_POWER:
    value = static_cast<uint64_t>(info.max_power);
    return;
  case qr_type::QR_FAN_FAN_PRESENCE:
    value = static_cast<uint64_t>(info.fan_presence);
    return;
  default:
    throw std::runtime_error("device_windows::board_info() unexpected qr " + std::to_string(qr));
  }
  // No query for mac_addr0, mac_addr1, mac_addr2, mac_addr3, revision, bd_name and config_mode
}

static void
mig_ecc_info(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_mig_ecc_info = [](const device_type* dev) {
    xcl_mig_ecc info = { 0 };
    userpf::get_mig_ecc_info(dev->get_user_handle(), &info);
    return info;
  };

  static std::map<const device_type*, xcl_mig_ecc> info_map;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = info_map.find(device);
  if (it == info_map.end()) {
    auto ret = info_map.emplace(device,init_mig_ecc_info(device));
    it = ret.first;
  }

  const xcl_mig_ecc& info = (*it).second;

  switch (qr) {
  case qr_type::QR_MIG_ECC_ENABLED:
    value = info.ecc_enabled;
    return;
  case qr_type::QR_MIG_ECC_STATUS:
    value = info.ecc_status;
    return;
  case qr_type::QR_MIG_ECC_CE_CNT:
    value = info.ecc_ce_cnt;
    return;
  case qr_type::QR_MIG_ECC_UE_CNT:
    value = info. ecc_ue_cnt;
    return;
  case qr_type::QR_MIG_ECC_CE_FFA:
    value = info.ecc_ce_ffa;
    return;
  case qr_type::QR_MIG_ECC_UE_FFA:
    value = info.ecc_ue_ffa;
    return;
  default:
    throw std::runtime_error("device_windows::mig_ecc_info() unexpected qr " + std::to_string(qr));
  }
  // No query for mem_type and mem_idx
}

static void
firewall_info(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto init_firewall_info = [](const device_type* dev) {
    xcl_firewall info = { 0 };
    userpf::get_firewall_info(dev->get_user_handle(), &info);
    return info;
  };

  static std::map<const device_type*, xcl_firewall> info_map;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = info_map.find(device);
  if (it == info_map.end()) {
    auto ret = info_map.emplace(device,init_firewall_info(device));
    it = ret.first;
  }

  const xcl_firewall& info = (*it).second;

  switch (qr) {
  case qr_type::QR_FIREWALL_DETECT_LEVEL:
    value = info.err_detected_level;
    return;
  case qr_type::QR_FIREWALL_STATUS:
    value = info.err_detected_status;
    return;
  case qr_type::QR_FIREWALL_TIME_SEC:
    value = info.err_detected_time;
    return;
  default:
    throw std::runtime_error("device_windows::firewall_info() unexpected qr " + std::to_string(qr));
  }
  // No query for max_level, curr_status and curr_level
}

static void
info(const device_type* device, qr_type qr, const std::type_info& tinfo, boost::any& value)
{
  if (auto mhdl = device->get_mgmt_handle())
    info_mgmt(device,qr,tinfo,value);
  else if (auto uhdl = device->get_user_handle())
    info_user(device,qr,tinfo,value);
  else
    throw std::runtime_error("No device handle");
}

static void
xclbin_fcn(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  auto uhdl = device->get_user_handle();
  if (!uhdl)
    throw std::runtime_error("Query request " + std::to_string(qr) + "requires a userpf device");

  if (qr == qr_type::QR_MEM_TOPOLOGY_RAW) {
    size_t size_ret = 0;
    userpf::get_mem_topology(uhdl, nullptr, 0, &size_ret);
    std::vector<char> data(size_ret);
    userpf::get_mem_topology(uhdl, data.data(), size_ret, nullptr);
    value = std::move(data);
    return;
  }

  if (qr == qr_type::QR_IP_LAYOUT_RAW) {
    size_t size_ret = 0;
    userpf::get_ip_layout(uhdl, nullptr, 0, &size_ret);
    std::vector<char> data(size_ret);
    userpf::get_ip_layout(uhdl, data.data(), size_ret, nullptr);
    value = std::move(data);
    return;
  }

  throw std::runtime_error("device_windows::xclbin() unexpected qr " + std::to_string(qr));
}

static void
bdf_fcn(const device_type* device, qr_type qr, const std::type_info&, boost::any& value)
{
  struct bdf_type {
    uint16_t bus = 0;
    uint16_t device = 0;
    uint16_t function = 0;
  };

  auto init_bdf = [](const device_type* dev, bdf_type* bdf) {
    if (auto mhdl = dev->get_mgmt_handle())
      mgmtpf::get_bdf_info(mhdl, reinterpret_cast<uint16_t*>(bdf));
    else if (auto uhdl = dev->get_user_handle())
      userpf::get_bdf_info(uhdl, reinterpret_cast<uint16_t*>(bdf));
    else
      throw std::runtime_error("No device handle");
  };

  static std::map<const device_type*, bdf_type> bdfmap;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lk(mutex);
  auto it = bdfmap.find(device);
  if (it == bdfmap.end()) {
    bdf_type bdf;
    init_bdf(device, &bdf);
    auto ret = bdfmap.emplace(device,bdf);
    it = ret.first;
  }

  auto& bdf = (*it).second;

  switch (qr) {
  case qr_type::QR_PCIE_BDF_BUS:
    value = bdf.bus;
    return;
  case qr_type::QR_PCIE_BDF_DEVICE:
    value = bdf.device;
    return;
  case qr_type::QR_PCIE_BDF_FUNCTION:
    value = bdf.function;
    return;
  default:
    throw std::runtime_error("device_windows::bdf() unexpected qr " + std::to_string(qr));
  }
}

} // namespace

namespace xrt_core {

const device_windows::IOCTLEntry &
device_windows::
get_IOCTL_entry(QueryRequest qr) const
{
  // Initialize our lookup table
  static const std::map<QueryRequest, IOCTLEntry> QueryRequestToIOCTLTable =
  {
    { QR_PCIE_VENDOR,               { info }},
    { QR_PCIE_DEVICE,               { info }},
    { QR_PCIE_SUBSYSTEM_VENDOR,     { info }},
    { QR_PCIE_SUBSYSTEM_ID,         { info }},
    { QR_PCIE_LINK_SPEED,           { nullptr }},
    { QR_PCIE_EXPRESS_LANE_WIDTH,   { nullptr }},
    { QR_PCIE_BDF_BUS,              { bdf_fcn }},
    { QR_PCIE_BDF_DEVICE,           { bdf_fcn }},
    { QR_PCIE_BDF_FUNCTION,         { bdf_fcn }},
    { QR_DMA_THREADS_RAW,           { nullptr }},
    { QR_ROM_VBNV,                  { rom }},
    { QR_ROM_DDR_BANK_SIZE,         { rom }},
    { QR_ROM_DDR_BANK_COUNT_MAX,    { rom }},
    { QR_ROM_FPGA_NAME,             { rom }},
    { QR_ROM_RAW,                   { rom }},
    { QR_ROM_UUID,                  { rom }},
    { QR_ROM_TIME_SINCE_EPOCH,      { rom }},
    { QR_MEM_TOPOLOGY_RAW,          { xclbin_fcn }},
    { QR_IP_LAYOUT_RAW,             { xclbin_fcn }},
    { QR_XMC_VERSION,               { sensor_info }},
    { QR_XMC_SERIAL_NUM,            { board_info }},
    { QR_XMC_MAX_POWER,             { board_info }},
    { QR_XMC_BMC_VERSION,           { board_info }},
    { QR_XMC_STATUS,                { xmc }},
    { QR_XMC_REG_BASE,              { nullptr }},
    { QR_DNA_SERIAL_NUM,            { nullptr }},
    { QR_CLOCK_FREQS,               { icap_info }},
    { QR_IDCODE,                    { icap_info }},
    { QR_STATUS_MIG_CALIBRATED,     { icap_info }},
    { QR_STATUS_P2P_ENABLED,        { nullptr }},

    { QR_TEMP_CARD_TOP_FRONT,       { sensor_info }},
    { QR_TEMP_CARD_TOP_REAR,        { sensor_info }},
    { QR_TEMP_CARD_BOTTOM_FRONT,    { sensor_info }},
    { QR_TEMP_FPGA,                 { sensor_info }},
    { QR_FAN_TRIGGER_CRITICAL_TEMP, { sensor_info }},
    { QR_FAN_FAN_PRESENCE,          { board_info }},
    { QR_FAN_SPEED_RPM,             { sensor_info }},
    { QR_DDR_TEMP_0,                { sensor_info }},
    { QR_DDR_TEMP_1,                { sensor_info }},
    { QR_DDR_TEMP_2,                { sensor_info }},
    { QR_DDR_TEMP_3,                { sensor_info }},
    { QR_HBM_TEMP,                  { sensor_info }},
    { QR_CAGE_TEMP_0,               { sensor_info }},
    { QR_CAGE_TEMP_1,               { sensor_info }},
    { QR_CAGE_TEMP_2,               { sensor_info }},
    { QR_CAGE_TEMP_3,               { sensor_info }},
    { QR_12V_PEX_MILLIVOLTS,        { sensor_info }},
    { QR_12V_PEX_MILLIAMPS,         { sensor_info }},
    { QR_12V_AUX_MILLIVOLTS,        { sensor_info }},
    { QR_12V_AUX_MILLIAMPS,         { sensor_info }},
    { QR_3V3_PEX_MILLIVOLTS,        { sensor_info }},
    { QR_3V3_AUX_MILLIVOLTS,        { sensor_info }},
    { QR_DDR_VPP_BOTTOM_MILLIVOLTS, { sensor_info }},
    { QR_DDR_VPP_TOP_MILLIVOLTS,    { sensor_info }},
    { QR_5V5_SYSTEM_MILLIVOLTS,     { sensor_info }},
    { QR_1V2_VCC_TOP_MILLIVOLTS,    { sensor_info }},
    { QR_1V2_VCC_BOTTOM_MILLIVOLTS, { sensor_info }},
    { QR_1V8_MILLIVOLTS,            { sensor_info }},
    { QR_0V85_MILLIVOLTS,           { sensor_info }},
    { QR_0V9_VCC_MILLIVOLTS,        { sensor_info }},
    { QR_12V_SW_MILLIVOLTS,         { sensor_info }},
    { QR_MGT_VTT_MILLIVOLTS,        { sensor_info }},
    { QR_INT_VCC_MILLIVOLTS,        { sensor_info }},
    { QR_INT_VCC_MILLIAMPS,         { sensor_info }},
    { QR_3V3_PEX_MILLIAMPS,         { sensor_info }},
    { QR_0V85_MILLIAMPS,            { sensor_info }},
    { QR_3V3_VCC_MILLIVOLTS,        { sensor_info }},
    { QR_HBM_1V2_MILLIVOLTS,        { sensor_info }},
    { QR_2V5_VPP_MILLIVOLTS,        { sensor_info }},
    { QR_INT_BRAM_VCC_MILLIVOLTS,   { sensor_info }},

    { QR_FIREWALL_DETECT_LEVEL,     { firewall_info }},
    { QR_FIREWALL_STATUS,           { firewall_info }},
    { QR_FIREWALL_TIME_SEC,         { firewall_info }},

    { QR_POWER_MICROWATTS,          { nullptr }},

    { QR_MIG_ECC_ENABLED,           { mig_ecc_info }},
    { QR_MIG_ECC_STATUS,            { mig_ecc_info }},
    { QR_MIG_ECC_CE_CNT,            { mig_ecc_info }},
    { QR_MIG_ECC_UE_CNT,            { mig_ecc_info }},
    { QR_MIG_ECC_CE_FFA,            { mig_ecc_info }},
    { QR_MIG_ECC_UE_FFA,            { mig_ecc_info }},

    { QR_FLASH_BAR_OFFSET,          { nullptr }},
    { QR_IS_MFG,                    { mfg }},
    { QR_F_FLASH_TYPE,              { flash_type }},
    { QR_FLASH_TYPE,                { flash_type }},
    { QR_BOARD_NAME,                { board_name }}
  };
  // Find the translation entry
  std::map<QueryRequest, IOCTLEntry>::const_iterator it = QueryRequestToIOCTLTable.find(qr);

  if (it == QueryRequestToIOCTLTable.end() || !it->second.m_fcn) {
    std::string err = boost::str( boost::format("The given query request ID (%d) is not supported.") % qr);
    throw no_such_query(qr, err);
  }

  return it->second;
}

void
device_windows::
query(QueryRequest qr, const std::type_info & tinfo, boost::any& value) const
{
  // Initialize return data to being empty container.
  // Note: CentOS Boost 1.53 doesn't support the clear() method.
  boost::any anyEmpty;
  value.swap(anyEmpty);

  // Get the sysdev and entry values to call
  auto& entry = get_IOCTL_entry(qr);
  if (!entry.m_fcn)
    throw std::runtime_error("Unexpected error, exception should already have been thrown");

  entry.m_fcn(this,qr,tinfo,value);

}

device_windows::
device_windows(id_type device_id, bool user)
  : device_pcie(device_id, user)
{
  if (user)
    return;

  m_mgmthdl = mgmtpf::open(device_id);
}

device_windows::
~device_windows()
{
  if (m_mgmthdl)
    mgmtpf::close(m_mgmthdl);
}

void
device_windows::
read_dma_stats(boost::property_tree::ptree& pt) const
{
}

void
device_windows::
read(uint64_t addr, void* buf, uint64_t len) const
{
  if (!m_mgmthdl)
    throw std::runtime_error("");

  mgmtpf::read_bar(m_mgmthdl, addr, buf, len);
}

void
device_windows::
write(uint64_t addr, const void* buf, uint64_t len) const
{
  if (!m_mgmthdl)
    throw std::runtime_error("");

  mgmtpf::write_bar(m_mgmthdl, addr, buf, len);
}

} // xrt_core
