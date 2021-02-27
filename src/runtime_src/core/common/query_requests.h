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
#include <sstream>
#include <iomanip>
#include <stdexcept>
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
  pcie_link_speed_max,
  pcie_express_lane_width,
  pcie_express_lane_width_max,
  pcie_bdf,

  edge_vendor,

  dma_threads_raw,

  rom_vbnv,
  rom_ddr_bank_size_gb,
  rom_ddr_bank_count_max,
  rom_fpga_name,
  rom_raw,
  rom_uuid,
  rom_time_since_epoch,

  xclbin_uuid,
  group_topology,
  memstat,
  memstat_raw,
  temp_by_mem_topology,
  mem_topology_raw,
  ip_layout_raw,
  clock_freq_topology_raw,
  dma_stream,
  kds_cu_info,

  xmc_version,
  xmc_board_name,
  xmc_serial_num,
  max_power_level,
  xmc_sc_presence,
  is_sc_fixed,
  xmc_sc_version,
  expected_sc_version,
  xmc_status,
  xmc_reg_base,
  xmc_scaling_enabled,
  xmc_scaling_override,
  xmc_scaling_reset,

  m2m,
  error,
  nodma,

  dna_serial_num,
  clock_freqs_mhz,
  idcode,
  data_retention,
  sec_level,
  max_shared_host_mem_aperture_bytes,

  status_mig_calibrated,
  p2p_config,

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
  v3v3_pex_milliamps,

  v3v3_aux_millivolts,
  v3v3_aux_milliamps,

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
  int_vcc_temp,
  int_vcc_io_milliamps,
  v3v3_vcc_millivolts,
  hbm_1v2_millivolts,
  v2v5_vpp_millivolts,
  v12_aux1_millivolts,
  vcc1v2_i_milliamps,
  v12_in_i_milliamps,
  v12_in_aux0_i_milliamps,
  v12_in_aux1_i_milliamps,
  vcc_aux_millivolts,
  vcc_aux_pmc_millivolts,
  vcc_ram_millivolts,
  int_vcc_io_millivolts,
  mac_contiguous_num,
  mac_addr_first,
  mac_addr_list,
  oem_id,

  firewall_detect_level,
  firewall_status,
  firewall_time_sec,
  power_microwatts,
  host_mem_size,
  kds_numcdmas,

  mig_cache_update,
  mig_ecc_enabled,
  mig_ecc_status,
  mig_ecc_ce_cnt,
  mig_ecc_ue_cnt,
  mig_ecc_ce_ffa,
  mig_ecc_ue_ffa,

  flash_bar_offset,
  is_mfg,
  mfg_ver,
  is_recovery,
  is_ready,
  f_flash_type,
  flash_type,
  board_name,
  interface_uuids,
  logic_uuids,
  rp_program_status,
  cpu_affinity,
  shared_host_mem,

  aie_metadata,
  graph_status,
  mailbox_metrics,

  clock_timestamp,
  ert_sleep,
  ert_cq_write,
  ert_cq_read,
  ert_cu_write,
  ert_cu_read,

  noop
};

class no_such_key : public std::exception
{
  key_type m_key;
  std::string msg;

  using qtype = std::underlying_type<query::key_type>::type;
public:
  explicit
  no_such_key(key_type k)
    : m_key(k)
    , msg(boost::str(boost::format("No such query request (%d)") % static_cast<qtype>(k)))
  {}

  key_type
  get_key() const
  {
    return m_key;
  }

  const char*
  what() const noexcept
  {
    return msg.c_str();
  }
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
    return boost::str(boost::format("0x%04x") % val);
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

struct pcie_link_speed_max : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::pcie_link_speed_max;
  static const char* name() { return "link_speed_max"; }

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

struct pcie_express_lane_width_max : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::pcie_express_lane_width_max;
  static const char* name() { return "width_max"; }

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
  static const char* name() { return "bdf"; }

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

struct edge_vendor : request
{
  using result_type = uint16_t;
  static const key_type key = key_type::edge_vendor;
  static const char* name() { return "vendor"; }

  virtual boost::any
    get(const device*) const = 0;

  static std::string
    to_string(result_type val)
  {
    return boost::str(boost::format("0x%x") % val);
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

struct rom_ddr_bank_size_gb : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::rom_ddr_bank_size_gb;
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
  static const char* name() { return "uuid"; }

  virtual boost::any
  get(const device*) const = 0;

  static result_type
  to_string(const result_type& value)
  {
    return value;
  }
};

struct rom_time_since_epoch : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::rom_time_since_epoch;
  static const char* name() { return "id"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return boost::str(boost::format("0x%x") % value);
  }
};

struct interface_uuids : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::interface_uuids;
  static const char* name() { return "interface_uuids"; }

  virtual boost::any
  get(const device*) const = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }
};

struct logic_uuids : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::logic_uuids;
  static const char* name() { return "logic_uuids"; }

  virtual boost::any
  get(const device*) const = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }
};

struct xclbin_uuid : request
{
  using result_type = std::string;
  static const key_type key = key_type::xclbin_uuid;

  virtual boost::any
  get(const device*) const = 0;
};

struct group_topology : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::group_topology;

  virtual boost::any
  get(const device*) const = 0;
};

struct temp_by_mem_topology : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::temp_by_mem_topology;

  virtual boost::any
  get(const device*) const = 0;
};

struct memstat : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::memstat;

  virtual boost::any
  get(const device*) const = 0;
};

struct memstat_raw : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::memstat_raw;

  virtual boost::any
  get(const device*) const = 0;
};

struct dma_stream : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::dma_stream;

  virtual boost::any
  get(const device*) const = 0;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct mem_topology_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::mem_topology_raw;

  virtual boost::any
  get(const device*) const = 0;
};

struct aie_metadata : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_metadata;

  virtual boost::any
  get(const device*) const = 0;
};

struct graph_status : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::graph_status;

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

struct kds_cu_info : request
{
  // Returning CUs info as <base_addr, usages, status>
  using result_type = std::vector<std::tuple<uint64_t, uint32_t, uint32_t>>;
  static const key_type key = key_type::kds_cu_info;

  virtual boost::any
  get(const device*) const = 0;
};

struct clock_freq_topology_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::clock_freq_topology_raw;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_version : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_version;
  static const char* name() { return "xmc_version"; }

  virtual boost::any
  get(const device*) const = 0;

  static result_type
  to_string(const result_type& value)
  {
    return value;
  }
};

struct xmc_board_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_board_name;
  static const char* name() { return "xmc_board_name"; }

  virtual boost::any
  get(const device*) const = 0;

  static result_type
  to_string(const result_type& value)
  {
    return value;
  }
};

struct xmc_serial_num : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_serial_num;
  static const char* name() { return "serial_number"; }

  virtual boost::any
  get(const device*) const = 0;

  static result_type
  to_string(const result_type& value)
  {
    return value;
  }
};

struct max_power_level : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::max_power_level;
  static const char* name() { return "max_power_level"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct xmc_sc_presence : request
{
  using result_type = bool;
  static const key_type key = key_type::xmc_sc_presence;
  static const char* name() { return "sc_presence"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return value ? "true" : "false";
  }
};

struct is_sc_fixed : request
{
  using result_type = bool;
  static const key_type key = key_type::is_sc_fixed;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return value ? "true" : "false";
  }
};

struct xmc_sc_version : request

{
  using result_type = std::string;
  static const key_type key = key_type::xmc_sc_version;
  static const char* name() { return "sc_version"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return value;
  }
};

struct expected_sc_version : request
{
  using result_type = std::string;
  static const key_type key = key_type::expected_sc_version;
  static const char* name() { return "expected_sc_version"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return value;
  }
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

struct xmc_scaling_enabled : request
{
  using result_type = bool;       // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xmc_scaling_enabled;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct xmc_scaling_override: request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type
  static const key_type key = key_type::xmc_scaling_override;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;

};

struct xmc_scaling_reset : request
{
  using value_type = std::string;   // put value type
  static const key_type key = key_type::xmc_scaling_reset;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct m2m : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::m2m;

  virtual boost::any
  get(const device*) const = 0;

  static bool
  to_bool(const result_type& value)
  {
     // if m2m does not exist, the execption is thrown
     return value ? true : false;
  }
};

struct nodma : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::nodma;

  virtual boost::any
  get(const device*) const = 0;

  static bool
  to_bool(const result_type& value)
  {
    return (value == std::numeric_limits<uint32_t>::max())
      ? false : value;
  }
};

// Retrieve asynchronous errors from driver
struct error : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::error;

  virtual boost::any
  get(const device*) const = 0;

  // Parse sysfs line and split into error code and timestamp
  static std::pair<uint64_t, uint64_t>
  to_value(const std::string& line)
  {
    std::size_t pos = 0;
    auto code = std::stoul(line, &pos);
    auto time = std::stoul(line.substr(pos));
    return std::make_pair(code, time);
  }
};

struct dna_serial_num : request
{
  using result_type = std::string;
  static const key_type key = key_type::dna_serial_num;
  static const char* name() { return "dna"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return value;
  }
};

struct clock_freqs_mhz : request
{
  using result_type = std::vector<std::string> ;
  static const key_type key = key_type::clock_freqs_mhz;
  static const char* name() { return "clocks"; }

  virtual boost::any
  get(const device*) const = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }
};

struct idcode : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::idcode;
  static const char* name() { return "idcode"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return boost::str(boost::format("0x%x") % value);
  }
};

struct data_retention : request
{
  using result_type = uint32_t;  // get value type
  using value_type = uint32_t;   // put value type

  static const key_type key = key_type::data_retention;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;

  static bool
  to_bool(const result_type& value)
  {
    return (value == std::numeric_limits<uint32_t>::max())
      ? false : value;
  }
};

struct sec_level : request
{
  using result_type = uint16_t;   // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::sec_level;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct max_shared_host_mem_aperture_bytes : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::max_shared_host_mem_aperture_bytes;

  virtual boost::any
  get(const device*) const = 0;
};

struct status_mig_calibrated : request
{
  using result_type = bool;
  static const key_type key = key_type::status_mig_calibrated;
  static const char* name() { return "mig_calibrated"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return value ? "true" : "false";
  }
};

struct p2p_config : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::p2p_config;
  static const char* name() { return "p2p_config"; }

  virtual boost::any
  get(const device*) const = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }
};

struct temp_card_top_front : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_card_top_front;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct temp_card_top_rear : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_card_top_rear;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct temp_card_bottom_front : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_card_bottom_front;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct temp_fpga : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::temp_fpga;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct fan_trigger_critical_temp : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::fan_trigger_critical_temp;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct fan_fan_presence : request
{
  using result_type = std::string;
  static const key_type key = key_type::fan_fan_presence;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return value.compare("A") == 0 ? "true" : "false";
  }
};

struct fan_speed_rpm : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::fan_speed_rpm;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
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

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct cage_temp_0 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_0;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct cage_temp_1 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_1;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct cage_temp_2 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_2;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct cage_temp_3 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::cage_temp_3;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12v_pex_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_pex_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12v_pex_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_pex_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12v_aux_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_aux_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12v_aux_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_aux_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v3v3_pex_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_pex_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v3v3_aux_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_aux_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct ddr_vpp_bottom_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_vpp_bottom_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct ddr_vpp_top_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_vpp_top_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v5v5_system_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v5v5_system_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v1v2_vcc_top_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v1v2_vcc_top_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v1v2_vcc_bottom_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v1v2_vcc_bottom_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v1v8_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v1v8_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v0v85_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v0v85_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v0v9_vcc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v0v9_vcc_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12v_sw_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12v_sw_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct mgt_vtt_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mgt_vtt_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct int_vcc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_vcc_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct int_vcc_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_vcc_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct int_vcc_temp : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_vcc_temp;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v3v3_pex_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_pex_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v3v3_aux_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_aux_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct int_vcc_io_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_vcc_io_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v3v3_vcc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v3v3_vcc_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct hbm_1v2_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::hbm_1v2_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v2v5_vpp_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v2v5_vpp_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12_aux1_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12_aux1_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct vcc1v2_i_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::vcc1v2_i_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12_in_i_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12_in_i_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12_in_aux0_i_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12_in_aux0_i_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct v12_in_aux1_i_milliamps : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v12_in_aux1_i_milliamps;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct vcc_aux_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::vcc_aux_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct vcc_aux_pmc_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::vcc_aux_pmc_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct vcc_ram_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::vcc_ram_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct int_vcc_io_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::int_vcc_io_millivolts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct mac_contiguous_num : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mac_contiguous_num;
  static const char* name() { return "mac_contiguous_num"; }

  virtual boost::any
  get(const device*) const = 0;
};

struct mac_addr_first : request
{
  using result_type = std::string;
  static const key_type key = key_type::mac_addr_first;
  static const char* name() { return "mac_addr_first"; }

  virtual boost::any
  get(const device*) const = 0;
};

struct mac_addr_list : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::mac_addr_list;
  static const char* name() { return "mac_addr_list"; }

  virtual boost::any
  get(const device*) const = 0;
};

struct oem_id : request
{
  using result_type = std::string;
  static const key_type key = key_type::oem_id;
  static const char* name() { return "oem_id"; }

  virtual boost::any
  get(const device*) const = 0;
};

struct firewall_detect_level : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_detect_level;
  static const char* name() { return "level"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct firewall_status : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_status;
  static const char* name() { return "status"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return boost::str(boost::format("0x%x") % value);
  }
};

struct firewall_time_sec : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_time_sec;
  static const char* name() { return "time_sec"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct power_microwatts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::power_microwatts;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct host_mem_size : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::host_mem_size;
  static const char* name() { return "host_mem_size"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type val)
  {
    return std::to_string(val);
  }
};

struct kds_numcdmas : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::kds_numcdmas;
  static const char* name() { return "kds_numcdmas"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type val)
  {
    return std::to_string(val);
  }
};

struct mig_cache_update : request
{
  using result_type = std::string;
  using value_type = std::string;   // put value type
  static const key_type key = key_type::mig_cache_update;

  virtual boost::any
  get(const device*, modifier m, const std::string&) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct mig_ecc_enabled : request
{
  using result_type = bool;
  static const key_type key = key_type::mig_ecc_enabled;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct mig_ecc_status : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_status;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct mig_ecc_ce_cnt : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ce_cnt;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct mig_ecc_ue_cnt : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ue_cnt;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct mig_ecc_ce_ffa : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ce_ffa;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct mig_ecc_ue_ffa : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ue_ffa;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct is_mfg : request
{
  using result_type = bool;
  static const key_type key = key_type::is_mfg;

  virtual boost::any
  get(const device*) const = 0;
};

struct mfg_ver : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::mfg_ver;

  virtual boost::any
  get(const device*) const = 0;
};

struct is_recovery : request
{
  using result_type = bool;
  static const key_type key = key_type::is_recovery;

  virtual boost::any
  get(const device*) const = 0;
};

struct is_ready : request
{
  using result_type = bool;
  static const key_type key = key_type::is_ready;

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
  static const char* name() { return "flash_type"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return value;
  }
};

struct board_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::board_name;

  virtual boost::any
  get(const device*) const = 0;
};

struct flash_bar_offset : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::flash_bar_offset;

  virtual boost::any
  get(const device*) const = 0;
};

struct rp_program_status : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;   // put value type
  static const key_type key = key_type::rp_program_status;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;

  static bool
  to_bool(const result_type& value)
  {
    return (value != 0) ? false : true;
  }
};

struct cpu_affinity : request
{
  using result_type = std::string;
  static const key_type key = key_type::cpu_affinity;

  virtual boost::any
  get(const device*) const = 0;
};

struct shared_host_mem : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::shared_host_mem;

  virtual boost::any
  get(const device*) const = 0;
};

struct clock_timestamp : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::clock_timestamp;

  virtual boost::any
  get(const device*) const = 0;
};

struct mailbox_metrics : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::mailbox_metrics;

  virtual boost::any
  get(const device*) const = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }
};

struct ert_sleep : request
{
  using result_type = uint32_t;  // get value type
  using value_type = uint32_t;   // put value type

  static const key_type key = key_type::ert_sleep;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;

};

struct ert_cq_read : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cq_read;

  virtual boost::any
  get(const device*) const = 0;
};

struct ert_cq_write : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cq_write;

  virtual boost::any
  get(const device*) const = 0;
};

struct ert_cu_read : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cu_read;

  virtual boost::any
  get(const device*) const = 0;
};

struct ert_cu_write : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cu_write;

  virtual boost::any
  get(const device*) const = 0;
};

struct noop : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::noop;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }

};

} // query

} // xrt_core


#endif
