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
#include "boost/format.hpp"
#include <string>
#include <iostream>
#include <map>
#include <mutex>

#pragma warning(disable : 4100 4996)

namespace {

using device_type = xrt_core::device_windows;
using qr_type = xrt_core::device::QueryRequest;

static void
not_implemented(const device_type*, qr_type qr, const std::type_info&, boost::any& value)
{
  throw xrt_core::no_such_query(qr, "not implemented");
}

static void
flash_type(const device_type*, qr_type, const std::type_info&, boost::any& value)
{
  value = "SPI";
}

static void
xmc(const device_type*, qr_type qr, const std::type_info&, boost::any& value)
{
  if(qr == xrt_core::device::QR_XMC_STATUS)
    value = 1;
  if(value.empty())
    throw std::runtime_error("Invalid query value");
}

void
mfg(const device_type*, qr_type, const std::type_info&, boost::any& value)
{
  value = false;
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
  std::mutex mutex;
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
    value = hdr.DDRChannelSize;
    return;
  case qr_type::QR_ROM_DDR_BANK_COUNT_MAX:
    value = hdr.DDRChannelCount;
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

} // namespace

namespace xrt_core {

const device_windows::IOCTLEntry &
device_windows::
get_IOCTL_entry(QueryRequest qr) const
{
  // Initialize our lookup table
  static const std::map<QueryRequest, IOCTLEntry> QueryRequestToIOCTLTable =
  {
    { QR_PCIE_VENDOR,               { not_implemented }},
    { QR_PCIE_DEVICE,               { not_implemented }},
    { QR_PCIE_SUBSYSTEM_VENDOR,     { nullptr }},
    { QR_PCIE_SUBSYSTEM_ID,         { nullptr }},
    { QR_PCIE_LINK_SPEED,           { nullptr }},
    { QR_PCIE_EXPRESS_LANE_WIDTH,   { nullptr }},
    { QR_DMA_THREADS_RAW,           { nullptr }},
    { QR_ROM_VBNV,                  { rom }},
    { QR_ROM_DDR_BANK_SIZE,         { rom }},
    { QR_ROM_DDR_BANK_COUNT_MAX,    { rom }},
    { QR_ROM_FPGA_NAME,             { rom }},
    { QR_ROM_RAW,                   { rom }},
    { QR_ROM_UUID,                  { rom }},
    { QR_ROM_TIME_SINCE_EPOCH,      { rom }},
    { QR_XMC_VERSION,               { nullptr }},
    { QR_XMC_SERIAL_NUM,            { nullptr }},
    { QR_XMC_MAX_POWER,             { nullptr }},
    { QR_XMC_BMC_VERSION,           { nullptr }},
    { QR_XMC_STATUS,                { xmc }},
    { QR_XMC_REG_BASE,              { nullptr }},
    { QR_DNA_SERIAL_NUM,            { nullptr }},
    { QR_CLOCK_FREQS,               { nullptr }},
    { QR_IDCODE,                    { nullptr }},
    { QR_STATUS_MIG_CALIBRATED,     { nullptr }},
    { QR_STATUS_P2P_ENABLED,        { nullptr }},
    { QR_TEMP_CARD_TOP_FRONT,       { nullptr }},
    { QR_TEMP_CARD_TOP_REAR,        { nullptr }},
    { QR_TEMP_CARD_BOTTOM_FRONT,    { nullptr }},
    { QR_TEMP_FPGA,                 { nullptr }},
    { QR_FAN_TRIGGER_CRITICAL_TEMP, { nullptr }},
    { QR_FAN_FAN_PRESENCE,          { nullptr }},
    { QR_FAN_SPEED_RPM,             { nullptr }},
    { QR_CAGE_TEMP_0,               { nullptr }},
    { QR_CAGE_TEMP_1,               { nullptr }},
    { QR_CAGE_TEMP_2,               { nullptr }},
    { QR_CAGE_TEMP_3,               { nullptr }},
    { QR_12V_PEX_MILLIVOLTS,        { nullptr }},
    { QR_12V_PEX_MILLIAMPS,         { nullptr }},
    { QR_12V_AUX_MILLIVOLTS,        { nullptr }},
    { QR_12V_AUX_MILLIAMPS,         { nullptr }},
    { QR_3V3_PEX_MILLIVOLTS,        { nullptr }},
    { QR_3V3_AUX_MILLIVOLTS,        { nullptr }},
    { QR_DDR_VPP_BOTTOM_MILLIVOLTS, { nullptr }},
    { QR_DDR_VPP_TOP_MILLIVOLTS,    { nullptr }},

    { QR_5V5_SYSTEM_MILLIVOLTS,     { nullptr }},
    { QR_1V2_VCC_TOP_MILLIVOLTS,    { nullptr }},
    { QR_1V2_VCC_BOTTOM_MILLIVOLTS, { nullptr }},
    { QR_1V8_MILLIVOLTS,            { nullptr }},
    { QR_0V85_MILLIVOLTS,           { nullptr }},
    { QR_0V9_VCC_MILLIVOLTS,        { nullptr }},
    { QR_12V_SW_MILLIVOLTS,         { nullptr }},
    { QR_MGT_VTT_MILLIVOLTS,        { nullptr }},
    { QR_INT_VCC_MILLIVOLTS,        { nullptr }},
    { QR_INT_VCC_MILLIAMPS,         { nullptr }},

    { QR_3V3_PEX_MILLIAMPS,         { nullptr }},
    { QR_0V85_MILLIAMPS,            { nullptr }},
    { QR_3V3_VCC_MILLIVOLTS,        { nullptr }},
    { QR_HBM_1V2_MILLIVOLTS,        { nullptr }},
    { QR_2V5_VPP_MILLIVOLTS,        { nullptr }},
    { QR_INT_BRAM_VCC_MILLIVOLTS,   { nullptr }},

    { QR_FIREWALL_DETECT_LEVEL,     { nullptr }},
    { QR_FIREWALL_STATUS,           { nullptr }},
    { QR_FIREWALL_TIME_SEC,         { nullptr }},

    { QR_POWER_MICROWATTS,          { nullptr }},

    { QR_FLASH_BAR_OFFSET,          { nullptr }},
    { QR_IS_MFG,                    { mfg }},
    { QR_F_FLASH_TYPE,              { flash_type }},
    { QR_FLASH_TYPE,                { flash_type }},
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

//  std::string sErrorMsg;
  // Reference linux code:
//  if (_typeInfo == typeid(std::string)) {
//    // -- Typeid: std::string --
//    _returnValue = std::string("");
//    std::string *stringValue = boost::any_cast<std::string>(&_returnValue);
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, *stringValue);
//
//  } else if (_typeInfo == typeid(uint64_t)) {
//    // -- Typeid: uint64_t --
//    _returnValue = (uint64_t) -1;
//    std::vector<uint64_t> uint64Vector;
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, uint64Vector);
//    if (!uint64Vector.empty()) {
//      _returnValue = uint64Vector[0];
//    }
//
//  } else if (_typeInfo == typeid(bool)) {
//    // -- Typeid: bool --
//    _returnValue = (bool) 0;
//    std::vector<uint64_t> uint64Vector;
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, uint64Vector);
//    if (!uint64Vector.empty()) {
//      _returnValue = (bool) uint64Vector[0];
//    }
//
//  } else if (_typeInfo == typeid(std::vector<std::string>)) {
//    // -- Typeid: std::vector<std::string>
//    _returnValue = std::vector<std::string>();
//    std::vector<std::string> *stringVector = boost::any_cast<std::vector<std::string>>(&_returnValue);
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, *stringVector);
//
//  } else {
//  }
//
//  sErrorMsg = boost::str( boost::format("Error: Unsupported query_device return type: '%s'") % _typeInfo.name());
//
//  if (!sErrorMsg.empty()) {
//    throw std::runtime_error(sErrorMsg);
//  }
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
