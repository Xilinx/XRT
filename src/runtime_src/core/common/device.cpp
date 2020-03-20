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

#define XRT_CORE_COMMON_SOURCE
#include "device.h"
#include "error.h"
#include "utils.h"
#include "query_requests.h"
#include "core/include/xrt.h"
#include <boost/format.hpp>
#include <string>
#include <iostream>
#include <fstream>

namespace xrt_core {

device::
device(id_type device_id)
  : m_device_id(device_id)
{
}

device::
~device()
{
  // virtual must be declared and defined
}

void
device::
register_axlf(const axlf* top)
{
  axlf_section_kind kinds[] = {EMBEDDED_METADATA, AIE_METADATA};
  for (auto kind : kinds) {
    auto hdr = xclbin::get_axlf_section(top, kind);
    if (!hdr)
      continue;
    auto section_data = reinterpret_cast<const char*>(top) + hdr->m_sectionOffset;
    std::vector<char> data{section_data, section_data + hdr->m_sectionSize};
    m_axlf_sections.emplace(std::make_pair(kind, std::move(data)));
  }
}

std::pair<const char*, size_t>
device::
get_axlf_section(axlf_section_kind section) const
{
  auto itr = m_axlf_sections.find(section);
  return itr != m_axlf_sections.end()
    ? std::make_pair((*itr).second.data(), (*itr).second.size())
    : std::make_pair(nullptr, 0);
}

std::string
device::
format_primative(const boost::any &_data)
{
  std::string sPropertyValue;

  if (_data.type() == typeid(std::string)) {
    sPropertyValue = boost::any_cast<std::string>(_data);
  }
  else if (_data.type() == typeid(uint64_t)) {
	  sPropertyValue = std::to_string(boost::any_cast<uint64_t>(_data));
  } else if (_data.type() == typeid(uint16_t)) {
	  sPropertyValue = std::to_string(boost::any_cast<uint16_t>(_data));
  } else if (_data.type() == typeid(bool)) {
    sPropertyValue = boost::any_cast<bool>(_data) ? "true" : "false";
  }
  else {
    std::string errMsg = boost::str( boost::format("Unsupported 'any' typeid: '%s'") % _data.type().name());
    throw error(errMsg);
  }

  return sPropertyValue;
}

std::string
device::format_hex(const boost::any & _data)
{
  // Can we work with this data type?
  if (_data.type() == typeid(uint64_t))
    return boost::str(boost::format("0x%x") % boost::any_cast<uint64_t>(_data));
  if (_data.type() == typeid(uint16_t))
    return boost::str(boost::format("0x%x") % boost::any_cast<uint16_t>(_data));
  if (_data.type() == typeid(uint8_t))
    return boost::str(boost::format("0x%x") % boost::any_cast<uint8_t>(_data));
  return format_primative(_data);
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

std::string
device::format_base10_shiftdown3(const boost::any &_data)
{
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  double value = (double) boost::any_cast<uint64_t>(_data);
  value /= (double) 1000.0;    // Shift down 3
  return to_string(value, 3 /*precision*/ );
}

std::string
device::format_base10_shiftdown6(const boost::any &_data)
{
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  double value = (double) boost::any_cast<uint64_t>(_data);
  value /= (double) 1000000.0;    // Shift down 3
  return to_string(value, 6 /*precision*/ );
}

std::string
device::format_hex_base2_shiftup30(const boost::any &_data)
{
  if (_data.type() == typeid(uint64_t)) {
    boost::any modifiedValue = boost::any_cast<uint64_t>(_data) << 30;
    return format_hex(modifiedValue);
  }
  if (_data.type() == typeid(uint16_t)) {
    boost::any modifiedValue = boost::any_cast<uint16_t>(_data) << 30;
    return format_hex(modifiedValue);
  }
  if (_data.type() == typeid(uint8_t)) {
    boost::any modifiedValue = boost::any_cast<uint8_t>(_data) << 30;
    return format_hex(modifiedValue);
  }
  return format_primative(_data);
}

void
device::
get_rom_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::rom_vbnv>::query_and_put(this, pt);
  ptree_updater<query::rom_ddr_bank_size>::query_and_put(this, pt);
  ptree_updater<query::rom_ddr_bank_count_max>::query_and_put(this, pt);
  ptree_updater<query::rom_fpga_name>::query_and_put(this, pt);
}


void
device::
get_xmc_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::xmc_version>::query_and_put(this, pt);
  ptree_updater<query::xmc_serial_num>::query_and_put(this, pt);
  ptree_updater<query::xmc_max_power>::query_and_put(this, pt);
  ptree_updater<query::xmc_bmc_version>::query_and_put(this, pt);
}

void
device::
get_platform_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::dna_serial_num>::query_and_put(this, pt);
  ptree_updater<query::clock_freqs>::query_and_put(this, pt);
  ptree_updater<query::idcode>::query_and_put(this, pt);
  ptree_updater<query::status_mig_calibrated>::query_and_put(this, pt);
  ptree_updater<query::status_p2p_enabled>::query_and_put(this, pt);
}

void
device::
read_thermal_pcb(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::temp_card_top_front>::query_and_put(this, pt);
  ptree_updater<query::temp_card_top_rear>::query_and_put(this, pt);
  ptree_updater<query::temp_card_bottom_front>::query_and_put(this, pt);
}

void
device::
read_thermal_fpga(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::temp_fpga>::query_and_put(this, pt);
}

void
device::
read_fan_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::fan_trigger_critical_temp>::query_and_put(this, pt);
  ptree_updater<query::fan_fan_presence>::query_and_put(this, pt);
  ptree_updater<query::fan_speed_rpm>::query_and_put(this, pt);
}

void
device::
read_thermal_cage(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::cage_temp_0>::query_and_put(this, pt);
  ptree_updater<query::cage_temp_1>::query_and_put(this, pt);
  ptree_updater<query::cage_temp_2>::query_and_put(this, pt);
  ptree_updater<query::cage_temp_3>::query_and_put(this, pt);
}

void
device::
read_electrical(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::v12v_pex_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v12v_pex_milliamps>::query_and_put(this,  pt);
  ptree_updater<query::v12v_aux_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v12v_aux_milliamps>::query_and_put(this,  pt);

  ptree_updater<query::v3v3_pex_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v3v3_aux_millivolts>::query_and_put(this, pt);
  ptree_updater<query::ddr_vpp_bottom_millivolts>::query_and_put(this, pt);
  ptree_updater<query::ddr_vpp_top_millivolts>::query_and_put(this, pt);


  ptree_updater<query::v5v5_system_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v1v2_vcc_top_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v1v2_vcc_bottom_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v1v8_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v0v85_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v0v9_vcc_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v12v_sw_millivolts>::query_and_put(this, pt);
  ptree_updater<query::mgt_vtt_millivolts>::query_and_put(this, pt);
  ptree_updater<query::int_vcc_millivolts>::query_and_put(this, pt);
  ptree_updater<query::int_vcc_milliamps>::query_and_put(this, pt);

  ptree_updater<query::v3v3_pex_milliamps>::query_and_put(this, pt);
  ptree_updater<query::v0v85_milliamps>::query_and_put(this, pt);
  ptree_updater<query::v3v3_vcc_millivolts>::query_and_put(this, pt);
  ptree_updater<query::hbm_1v2_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v2v5_vpp_millivolts>::query_and_put(this, pt);
  ptree_updater<query::int_bram_vcc_millivolts>::query_and_put(this, pt);
}

void
device::
read_power(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::power_microwatts>::query_and_put(this, pt);
}


void
device::
read_firewall(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::firewall_detect_level>::query_and_put(this, pt);
  ptree_updater<query::firewall_status>::query_and_put(this, pt);
  ptree_updater<query::firewall_time_sec>::query_and_put(this, pt);
}

} // xrt_core
