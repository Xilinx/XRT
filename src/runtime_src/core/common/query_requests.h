/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef xrt_core_common_query_requests_h
#define xrt_core_common_query_requests_h

#include "query.h"
#include <string>
#include <vector>
#include <boost/any.hpp>
#include <boost/format.hpp>

namespace xrt_core {

namespace query {

/**
 * enum class key_type - keys for specific query requests
 *
 * Use keys in this table to identify the desired query request.  Use
 * the key_type to identify the specific struct defining the query
 * request itself.  The request struct is named same as the key, so to
 * get the BDF call
 *    auto bdf = xrt_core::device_query<pcie_bdf>(device);
 * The type return the query request is the result_type defined in the
 * query request struct, e.g. pcie_bdf::result_type.
 */
enum class key_type
{
  pcie_vendor,
  pcie_device,
  pcie_subsystem_vendor,
  pcie_subsystem_id,
  pcie_link_speed,
  pcie_express_lane_width,
  pcie_bdf,

  dma_threads_raw,

  rom_vbnv,
  rom_ddr_bank_size,
  rom_ddr_bank_count_max,
  rom_fpga_name,
  rom_raw,
  rom_uuid,
  rom_time_since_epoch,

  mem_topology_raw,
  ip_layout_raw,

  xmc_version,
  xmc_serial_num,
  xmc_max_power,
  xmc_bmc_version,
  xmc_status,
  xmc_reg_base,

  dna_serial_num,
  clock_freqs,
  idcode,

  status_mig_calibrated,
  status_p2p_enabled,

  temp_card_top_front,
  temp_card_top_rear,
  temp_card_bottom_front,

  temp_fpga,

  fan_trigger_critical_temp,
  fan_fan_presence,
  fan_speed_rpm,

  ddr_temp_0,
  ddr_temp_1,
  ddr_temp_2,
  ddr_temp_3,
  hbm_temp,

  cage_temp_0,
  cage_temp_1,
  cage_temp_2,
  cage_temp_3,

  v12v_pex_millivolts,
  v12v_pex_milliamps,

  v12v_aux_millivolts,
  v12v_aux_milliamps,

  v3v3_pex_millivolts,
  v3v3_aux_millivolts,

  ddr_vpp_bottom_millivolts,
  ddr_vpp_top_millivolts,
  v5v5_system_millivolts,
  v1v2_vcc_top_millivolts,
  v1v2_vcc_bottom_millivolts,
  v1v8_millivolts,
  v0v85_millivolts,
  v0v9_vcc_millivolts,
  v12v_sw_millivolts,
  mgt_vtt_millivolts,
  int_vcc_millivolts,
  int_vcc_milliamps,
  a3v3_pex_milliamps,
  a0v85_milliamps,
  v3v3_vcc_millivolts,
  hbm_1v2_millivolts,
  v2v5_vpp_millivolts,
  int_bram_vcc_millivolts,
  firewall_detect_level,
  firewall_status,
  firewall_time_sec,
  power_microwatts,

  mig_ecc_enabled,
  mig_ecc_status,
  mig_ecc_ce_cnt,
  mig_ecc_ue_cnt,
  mig_ecc_ce_ffa,
  mig_ecc_ue_ffa,

  flash_bar_offset,
  is_mfg,
  f_flash_type,
  flash_type,
  board_name
};

struct pcie_vendor : request
{
  using result_type = uint16_t;
  static const key_type key = key_type::pcie_vendor;
  static const char* name() { return "vendor"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type val)
  {
    return boost::str(boost::format("0x%x") % val);
  }
};

struct pcie_device : request
{
  using result_type = uint16_t;
  static const key_type key = key_type::pcie_device;
  static const char* name() { return "device"; }

  virtual boost::any
  get(const device*) const = 0;


  static std::string
  to_string(result_type val)
  {
    return boost::str(boost::format("0x%x") % val);
  }
};

struct pcie_subsystem_vendor : request
{
  using result_type = uint16_t;
  static const key_type key = key_type::pcie_subsystem_vendor;
  static const char* name() { return "subsystem_vendor"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type val)
  {
    return boost::str(boost::format("0x%x") % val);
  }
};

struct pcie_subsystem_id : request
{
  using result_type = uint16_t;
  static const key_type key = key_type::pcie_subsystem_id;
  static const char* name() { return "subsystem_id"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type val)
  {
    return boost::str(boost::format("0x%x") % val);
  }
};

struct pcie_link_speed : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::pcie_link_speed;
  static const char* name() { return "link_speed"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type val)
  {
    return std::to_string(val);
  }
};

struct pcie_express_lane_width : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::pcie_express_lane_width;
  static const char* name() { return "width"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type val)
  {
    return std::to_string(val);
  }
};

struct pcie_bdf : request
{
  using result_type = std::tuple<uint16_t,uint16_t,uint16_t>;
  static const key_type key = key_type::pcie_bdf;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return boost::str
      (boost::format("%04x:%02x:%02x.%01x") % 0 % std::get<0>(value)
       % std::get<1>(value) % std::get<2>(value));
  }
};

struct dma_threads_raw : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::dma_threads_raw;
  static const char* name() { return "dma_threads_raw"; }

  virtual boost::any
  get(const device*) const = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }
};

struct rom_vbnv : request
{
  using result_type = std::string;
  static const key_type key = key_type::rom_vbnv;
  static const char* name() { return "vbnv"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return value;
  }
};

struct rom_ddr_bank_size : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::rom_ddr_bank_size;
  static const char* name() { return "ddr_size_bytes"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return boost::str(boost::format("0x%x") % (value << 30));
  }
};

struct rom_ddr_bank_count_max : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::rom_ddr_bank_count_max;
  static const char* name() { return "widdr_countdth"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct rom_fpga_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::rom_fpga_name;
  static const char* name() { return "fpga_name"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return value;
  }
};

struct rom_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::rom_raw;

  virtual boost::any
  get(const device*) const = 0;
};

struct rom_uuid : request
{
  using result_type = std::string;
  static const key_type key = key_type::rom_uuid;

  virtual boost::any
  get(const device*) const = 0;
};

struct rom_time_since_epoch : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::rom_time_since_epoch;

  virtual boost::any
  get(const device*) const = 0;
};

struct mem_topology_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::mem_topology_raw;

  virtual boost::any
  get(const device*) const = 0;
};

struct ip_layout_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::ip_layout_raw;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_version : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_version;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_serial_num : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_serial_num;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_max_power : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_max_power;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_bmc_version : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_bmc_version;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_status : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::xmc_status;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_reg_base : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::xmc_reg_base;

  virtual boost::any
  get(const device*) const = 0;
};

struct dna_serial_num : request
{
  using result_type = std::string;
  static const key_type key = key_type::dna_serial_num;

  virtual boost::any
  get(const device*) const = 0;
};

struct clock_freqs : request
{
  using result_type = std::vector<std::string> ;
  static const key_type key = key_type::clock_freqs;

  virtual boost::any
  get(const device*) const = 0;
};

struct idcode : request
{
  using result_type = std::string;
  static const key_type key = key_type::idcode;

  virtual boost::any
  get(const device*) const = 0;
};

struct status_mig_calibrated : request
{
  using result_type = bool;
  static const key_type key = key_type::status_mig_calibrated;

  virtual boost::any
  get(const device*) const = 0;
};

struct status_p2p_enabled : request
{
  using result_type = bool;
  static const key_type key = key_type::status_p2p_enabled;

  virtual boost::any
  get(const device*) const = 0;
};

struct temp_card_top_front : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_card_top_front;

  virtual boost::any
  get(const device*) const = 0;
};

struct temp_card_top_rear : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_card_top_rear;

  virtual boost::any
  get(const device*) const = 0;
};

struct temp_card_bottom_front : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_card_bottom_front;

  virtual boost::any
  get(const device*) const = 0;
};

struct temp_fpga : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_fpga;

  virtual boost::any
  get(const device*) const = 0;
};

struct fan_trigger_critical_temp : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::fan_trigger_critical_temp;

  virtual boost::any
  get(const device*) const = 0;
};

struct fan_fan_presence : request
{
  using result_type = std::string;
  static const key_type key = key_type::fan_fan_presence;

  virtual boost::any
  get(const device*) const = 0;
};

struct fan_speed_rpm : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::fan_speed_rpm;

  virtual boost::any
  get(const device*) const = 0;
};

struct ddr_temp_0 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_temp_0;

  virtual boost::any
  get(const device*) const = 0;
};

struct ddr_temp_1 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_temp_1;

  virtual boost::any
  get(const device*) const = 0;
};

struct ddr_temp_2 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_temp_2;

  virtual boost::any
  get(const device*) const = 0;
};

struct ddr_temp_3 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_temp_3;

  virtual boost::any
  get(const device*) const = 0;
};

struct hbm_temp : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::hbm_temp;

  virtual boost::any
  get(const device*) const = 0;
};

struct cage_temp_0 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_0;

  virtual boost::any
  get(const device*) const = 0;
};

struct cage_temp_1 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_1;

  virtual boost::any
  get(const device*) const = 0;
};

struct cage_temp_2 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_2;

  virtual boost::any
  get(const device*) const = 0;
};

struct cage_temp_3 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_3;

  virtual boost::any
  get(const device*) const = 0;
};

struct v12v_pex_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_pex_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v12v_pex_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_pex_milliamps;

  virtual boost::any
  get(const device*) const = 0;
};

struct v12v_aux_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_aux_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v12v_aux_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_aux_milliamps;

  virtual boost::any
  get(const device*) const = 0;
};

struct v3v3_pex_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_pex_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v3v3_aux_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_aux_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct ddr_vpp_bottom_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_vpp_bottom_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct ddr_vpp_top_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_vpp_top_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v5v5_system_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v5v5_system_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v1v2_vcc_top_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v1v2_vcc_top_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v1v2_vcc_bottom_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v1v2_vcc_bottom_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v1v8_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v1v8_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v0v85_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v0v85_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v0v9_vcc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v0v9_vcc_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v12v_sw_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_sw_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct mgt_vtt_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mgt_vtt_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct int_vcc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_vcc_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct int_vcc_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_vcc_milliamps;

  virtual boost::any
  get(const device*) const = 0;
};

struct a3v3_pex_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::a3v3_pex_milliamps;

  virtual boost::any
  get(const device*) const = 0;
};

struct a0v85_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::a0v85_milliamps;

  virtual boost::any
  get(const device*) const = 0;
};

struct v3v3_vcc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_vcc_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct hbm_1v2_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::hbm_1v2_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct v2v5_vpp_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v2v5_vpp_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct int_bram_vcc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_bram_vcc_millivolts;

  virtual boost::any
  get(const device*) const = 0;
};

struct firewall_detect_level : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_detect_level;

  virtual boost::any
  get(const device*) const = 0;
};

struct firewall_status : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_status;

  virtual boost::any
  get(const device*) const = 0;
};

struct firewall_time_sec : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_time_sec;

  virtual boost::any
  get(const device*) const = 0;
};

struct power_microwatts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::power_microwatts;

  virtual boost::any
  get(const device*) const = 0;
};

struct mig_ecc_enabled : request
{
  using result_type = bool;
  static const key_type key = key_type::mig_ecc_enabled;

  virtual boost::any
  get(const device*, const boost::any&) const = 0;
};

struct mig_ecc_status : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_status;

  virtual boost::any
  get(const device*, const boost::any&) const = 0;
};

struct mig_ecc_ce_cnt : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ce_cnt;

  virtual boost::any
  get(const device*, const boost::any&) const = 0;
};

struct mig_ecc_ue_cnt : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ue_cnt;

  virtual boost::any
  get(const device*, const boost::any&) const = 0;
};

struct mig_ecc_ce_ffa : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ce_ffa;

  virtual boost::any
  get(const device*, const boost::any&) const = 0;
};

struct mig_ecc_ue_ffa : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ue_ffa;

  virtual boost::any
  get(const device*, const boost::any&) const = 0;
};

struct flash_bar_offset : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::flash_bar_offset;

  virtual boost::any
  get(const device*) const = 0;
};

struct is_mfg : request
{
  using result_type = bool;
  static const key_type key = key_type::is_mfg;

  virtual boost::any
  get(const device*) const = 0;
};

struct f_flash_type : request
{
  using result_type = std::string;
  static const key_type key = key_type::f_flash_type;

  virtual boost::any
  get(const device*) const = 0;
};

struct flash_type : request
{
  using result_type = std::string;
  static const key_type key = key_type::flash_type;

  virtual boost::any
  get(const device*) const = 0;
};

struct board_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::board_name;

  virtual boost::any
  get(const device*) const = 0;
};

} // query

} // xrt_core


#endif
