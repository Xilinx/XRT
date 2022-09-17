/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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
#include "error.h"
#include "query.h"
#include "uuid.h"

#include "core/include/xclerr_int.h"

#include <iomanip>
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <boost/any.hpp>
#include <boost/format.hpp>

struct debug_ip_data;

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

  instance,
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
  debug_ip_layout_raw,
  clock_freq_topology_raw,
  dma_stream,
  kds_cu_info,
  sdm_sensor_info,
  kds_scu_info,
  ps_kernel,
  xocl_errors,
  xclbin_full,
  ic_enable,
  ic_load_flash_address,


  xmc_version,
  xmc_board_name,
  xmc_serial_num,
  max_power_level,
  power_warning,
  xmc_sc_presence,
  is_sc_fixed,
  xmc_sc_version,
  expected_sc_version,
  xmc_status,
  xmc_reg_base,
  xmc_scaling_support,
  xmc_scaling_enabled,
  xmc_scaling_power_override,
  xmc_scaling_temp_override,
  xmc_scaling_critical_pow_threshold,
  xmc_scaling_critical_temp_threshold,
  xmc_scaling_threshold_power_limit,
  xmc_scaling_threshold_temp_limit,
  xmc_scaling_power_override_enable,
  xmc_scaling_temp_override_enable,
  xmc_scaling_reset,
  xmc_qspi_status,

  m2m,
  error,
  nodma,

  dna_serial_num,
  clock_freqs_mhz,
  aie_core_info,
  aie_shim_info,
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
  v0v9_int_vcc_vcu_millivolts,
  mac_contiguous_num,
  mac_addr_first,
  mac_addr_list,
  oem_id,

  heartbeat_count,
  heartbeat_err_code,
  heartbeat_err_time,
  heartbeat_stall,

  firewall_detect_level,
  firewall_detect_level_name,
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
  is_versal,
  is_ready,
  is_offline,
  f_flash_type,
  flash_type,
  flash_size,
  board_name,
  interface_uuids,
  logic_uuids,
  rp_program_status,
  cpu_affinity,
  shared_host_mem,
  enabled_host_mem,

  aie_metadata,
  aie_reg_read,
  graph_status,
  mailbox_metrics,

  config_mailbox_channel_disable,
  config_mailbox_channel_switch,
  config_xclbin_change,
  cache_xclbin,

  clock_timestamp,
  ert_sleep,
  ert_cq_write,
  ert_cq_read,
  ert_cu_write,
  ert_cu_read,
  ert_data_integrity,
  ert_status,

  aim_counter,
  am_counter,
  asm_counter,
  lapc_status,
  spc_status,
  accel_deadlock_status,
  xclbin_slots,
  aie_get_freq,
  aie_set_freq,
  dtbo_path,

  boot_partition,
  flush_default_only,
  program_sc,
  vmr_status,
  extended_vmr_status,

  hwmon_sdm_serial_num,
  hwmon_sdm_oem_id,
  hwmon_sdm_board_name,
  hwmon_sdm_active_msp_ver,
  hwmon_sdm_mac_addr0,
  hwmon_sdm_mac_addr1,
  hwmon_sdm_revision,
  hwmon_sdm_fan_presence,
  hwmon_sdm_mfg_date,
  hotplug_offline,

  cu_size,
  cu_read_range,

  clk_scaling_info,

  xgq_scaling_enabled,
  xgq_scaling_power_override,
  xgq_scaling_temp_override,
  noop
};

// Base class for query request exceptions.
//
// Provides granularity for calling code to catch errors specific to
// query request which are often acceptable errors because some
// devices may not support all types of query requests.
//
// Other non query exceptions signal a different kind of error which
// should maybe not be caught.
//
// The addition of the query request exception hierarchy does not
// break existing code that catches std::exception (or all errors)
// because ultimately the base query exception is-a std::exception
class exception : public std::runtime_error
{
public:
  explicit
  exception(const std::string& err)
    : std::runtime_error(err)
  {}
};

class no_such_key : public exception
{
  key_type m_key;

  using qtype = std::underlying_type<query::key_type>::type;
public:
  explicit
  no_such_key(key_type k)
    : exception(boost::str(boost::format("No such query request (%d)") % static_cast<qtype>(k)))
    , m_key(k)
  {}

  no_such_key(key_type k, const std::string& msg)
    : exception(msg)
    , m_key(k)
  {}

  key_type
  get_key() const
  {
    return m_key;
  }
};

class sysfs_error : public exception
{
public:
  explicit
  sysfs_error(const std::string& msg)
    : exception(msg)
  {}
};

class not_supported : public exception
{
public:
  explicit
  not_supported(const std::string& msg)
    : exception(msg)
  {}
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
  using result_type = std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>;
  static const key_type key = key_type::pcie_bdf;
  static const char* name() { return "bdf"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return boost::str
      (boost::format("%04x:%02x:%02x.%01x") % std::get<0>(value) %
       std::get<1>(value) % std::get<2>(value) % std::get<3>(value));
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

  // Convert string value to proper uuid string if necessary
  static std::string
  to_uuid_string(const std::string& value)
  {
    std::string str = value;
    if (str.length() < 24)  // for '-' insertion
      throw xrt_core::system_error(EINVAL, "invalid uuid: " + value);
    for (auto idx : {8, 13, 18, 23})
      if (str[idx] != '-')
        str.insert(idx,1,'-');
    if (str.length() != 36) // final uuid length must be 36 chars
      throw xrt_core::system_error(EINVAL, "invalid uuid: " + value);
    return str;
  }

  // Convert string value to proper uuid upper cased string if necessary
  XRT_CORE_COMMON_EXPORT
  static std::string
  to_uuid_upper_string(const std::string& value);

  // Convert string value to proper uuid string if necessary
  // and return xrt::uuid
  static uuid
  to_uuid(const std::string& value)
  {
    auto str = to_uuid_string(value);
    return uuid{str};
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

// dtbo_path is unique path used by libdfx library to load bitstream and device tree
// overlay(dtbo), this query reads dtbo_path from sysfs node
// Applicable only for embedded platforms
struct dtbo_path : request
{
  using result_type = std::string;
  using slot_id_type = uint32_t;

  static const key_type key = key_type::dtbo_path;

  virtual boost::any
  get(const device*, const boost::any& slot_id) const = 0;
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

struct xclbin_full : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::xclbin_full;

  virtual boost::any
  get(const device*) const = 0;
};

struct ic_enable : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::ic_enable;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct ic_load_flash_address : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::ic_load_flash_address;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct aie_metadata : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_metadata;

  virtual boost::any
  get(const device*) const = 0;
};

struct aie_reg_read : request
{
  using result_type = uint32_t;
  using row_type = uint32_t;
  using col_type = uint32_t;
  using reg_type = std::string;
  static const key_type key = key_type::aie_reg_read;

  virtual boost::any
  get(const device*, const boost::any& row, const boost::any& col, const boost::any& reg) const = 0;
};

struct aie_get_freq : request
{
  using result_type = uint64_t;
  using partition_id_type = uint32_t;
  static const key_type key = key_type::aie_get_freq;

  virtual boost::any
  get(const device*, const boost::any& partition_id) const = 0;
};

struct aie_set_freq : request
{
  using result_type = bool;
  using partition_id_type = uint32_t;
  using freq_type = uint64_t;
  static const key_type key = key_type::aie_set_freq;

  virtual boost::any
  get(const device*, const boost::any& partition_id, const boost::any& freq) const = 0;
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

struct debug_ip_layout_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::debug_ip_layout_raw;

  virtual boost::any
  get(const device*) const = 0;
};

struct sdm_sensor_info : request
{
  /**
   * enum class sdr_req_type - request ids for specific sensor query requests
   *
   * Use request ids in this table to identify the desired sensor query request.
   */
  enum class sdr_req_type
  {
    current     = 0,
    voltage     = 1,
    power       = 2,
    thermal     = 3,
    mechanical  = 4,
  };

  /*
   * struct sensor_data: used to store sensor information and
   * each sensor contains following information.
   *  label    : name
   *  input    : instantaneous value
   *  max      : maximum value
   *  average  : average value
   *  highest  : highest value (used for temperature sensors)
   *  status   : sensor status
   *  units    : sensor value units
   *  unitm    : unit modifier value used to get actual sensor value
   */
  struct sensor_data {
    std::string label;
    uint32_t input {};
    uint32_t max {};
    uint32_t average {};
    uint32_t highest {};
    std::string status;
    std::string units;
    int8_t unitm;
  };
  using result_type = std::vector<sensor_data>;
  using req_type = sdr_req_type;
  using data_type = sensor_data;
  static const key_type key = key_type::sdm_sensor_info;

  virtual boost::any
  get(const device*, const boost::any& req_type) const = 0;
};

struct kds_cu_info : request
{
  struct data {
    uint32_t slot_index;
    uint32_t index;
    std::string name;
    uint64_t base_addr;
    uint32_t status;
    uint64_t usages;
  };
  using result_type = std::vector<struct data>;
  using data_type = struct data;
  static const key_type key = key_type::kds_cu_info;

  virtual boost::any
  get(const device*) const = 0;
};

struct ps_kernel : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::ps_kernel;

  virtual boost::any
  get(const device*) const = 0;
};

struct kds_scu_info : request
{
  struct data {
    uint32_t slot_index;
    uint32_t index;
    std::string name;
    uint32_t status;
    uint64_t usages;
  };
  using result_type = std::vector<struct data>;
  using data_type = struct data;
  static const key_type key = key_type::kds_scu_info;

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


struct instance : request
{
  using result_type = int64_t;
  static const key_type key = key_type::instance;
  static const char* name() { return "instance"; }

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(const result_type& value)
  {
    return std::to_string(value);
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

struct xmc_scaling_support : request
{
  using result_type = bool;       // get value type
  static const key_type key = key_type::xmc_scaling_support;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_scaling_critical_temp_threshold : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_critical_temp_threshold;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_scaling_critical_pow_threshold : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_critical_pow_threshold;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_scaling_threshold_power_limit : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_threshold_power_limit;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_scaling_threshold_temp_limit : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_threshold_temp_limit;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_scaling_power_override_enable : request
{
  using result_type = bool;       // get value type
  static const key_type key = key_type::xmc_scaling_power_override_enable;

  virtual boost::any
  get(const device*) const = 0;
};

struct xmc_scaling_temp_override_enable : request
{
  using result_type = bool;       // get value type
  static const key_type key = key_type::xmc_scaling_temp_override_enable;

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

struct xmc_scaling_power_override: request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type
  static const key_type key = key_type::xmc_scaling_power_override;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;

};

struct xmc_scaling_temp_override: request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type
  static const key_type key = key_type::xmc_scaling_temp_override;

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

struct xmc_qspi_status : request
{
  // Returning qspi write protection status as <primary qspi, recovery qspi>
  using result_type = std::pair<std::string, std::string>;
  static const key_type key = key_type::xmc_qspi_status;

  virtual boost::any
  get(const device*) const = 0;
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

// Retrieve asynchronous xocl errors from xocl driver
struct xocl_errors : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::xocl_errors;

  virtual boost::any
  get(const device*) const = 0;

  // Parse sysfs line and from class get error code and timestamp
  XRT_CORE_COMMON_EXPORT
  static std::pair<uint64_t, uint64_t>
  to_value(const std::vector<char>& buf, xrtErrorClass ecl);

  // Parse sysfs raw data and get list of errors
  XRT_CORE_COMMON_EXPORT
  static std::vector<xclErrorLast>
  to_errors(const std::vector<char>& buf);
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

struct aie_core_info : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_core_info;

  virtual boost::any
  get(const device*) const = 0;
};

struct aie_shim_info : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_shim_info;

  virtual boost::any
  get(const device*) const = 0;
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

  enum class value_type { disabled, enabled, error, no_iomem, not_supported };

  // parse a config result and return value and msg
  XRT_CORE_COMMON_EXPORT
  static std::pair<value_type, std::string>
  parse(const result_type& config);

  // convert value_type enumerator to std::string
  XRT_CORE_COMMON_EXPORT
  static std::string
  to_string(value_type value);

  virtual boost::any
  get(const device*) const = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }

  XRT_CORE_COMMON_EXPORT
  static std::map<std::string, int64_t>
  to_map(const xrt_core::query::p2p_config::result_type& config);
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

struct v0v9_int_vcc_vcu_millivolts : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::v0v9_int_vcc_vcu_millivolts;

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

  // parse an oem_id and return value as string
  XRT_CORE_COMMON_EXPORT
  static std::string
  parse(const result_type& value);

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

struct firewall_detect_level_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::firewall_detect_level_name;
  static const char* name() { return "level_name"; }

  virtual boost::any
  get(const device*) const = 0;

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

struct power_warning : request
{
  using result_type = bool;
  static const key_type key = key_type::power_warning;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return value ? "true" : "false";
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

/*
 * struct is_versal - check if device is versal or not
 * A value of true means it is a versal device.
 * This entry is needed as some of the operations are handled
 * differently on versal devices compared to Alveo devices
 */
struct is_versal : request
{
  using result_type = bool;
  static const key_type key = key_type::is_versal;

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

// struct is_offline - check if device is offline (being reset)
//
// A value of true means means the device is currently offline and in
// process of resetting.  An application sigbus handler can catch
// SIGBUS and check if device is offline and take appropriate action.
//
// This query request is exposed through xrt::device::get_info
struct is_offline : request
{
  using result_type = bool;
  static const key_type key = key_type::is_offline;

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

struct flash_size : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::flash_size;

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

struct enabled_host_mem : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::enabled_host_mem;

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

struct config_mailbox_channel_disable : request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type

  static const key_type key = key_type::config_mailbox_channel_disable;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct config_mailbox_channel_switch : request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type

  static const key_type key = key_type::config_mailbox_channel_switch;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct config_xclbin_change : request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type

  static const key_type key = key_type::config_xclbin_change;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct cache_xclbin : request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type

  static const key_type key = key_type::cache_xclbin;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
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


struct ert_data_integrity : request
{
  using result_type = bool;
  static const key_type key = key_type::ert_data_integrity;

  virtual boost::any
  get(const device*) const = 0;

  static std::string
  to_string(result_type value)
  {
    return value ? "Pass" : "Fail";
  }
};

struct ert_status : request
{
  struct ert_status_data {
    bool        connected;
    // add more in the future
  };
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::ert_status;

  virtual boost::any
  get(const device*) const = 0;

  static ert_status_data
  to_ert_status(const result_type& strs);
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

struct heartbeat_err_time : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::heartbeat_err_time;

  virtual boost::any
  get(const device*) const = 0;
};

struct heartbeat_err_code : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::heartbeat_err_code;

  virtual boost::any
  get(const device*) const = 0;
};

struct heartbeat_count : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::heartbeat_count;

  virtual boost::any
  get(const device*) const = 0;
};

struct heartbeat_stall : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::heartbeat_stall;

  virtual boost::any
  get(const device*) const = 0;
};

struct aim_counter : request
{
  using result_type = std::vector<uint64_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::aim_counter;

  virtual boost::any
  get(const xrt_core::device* device, const boost::any& dbg_ip_data) const = 0;
};

struct am_counter : request
{
  using result_type = std::vector<uint64_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::am_counter;

  virtual boost::any
  get(const xrt_core::device* device, const boost::any& dbg_ip_data) const = 0;
};

struct asm_counter : request
{
  using result_type = std::vector<uint64_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::asm_counter;

  virtual boost::any
  get(const xrt_core::device* device, const boost::any& dbg_ip_data) const = 0;
};

struct lapc_status : request
{
  using result_type = std::vector<uint32_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::lapc_status;

  virtual boost::any
  get(const xrt_core::device* device, const boost::any& dbg_ip_data) const = 0;
};

struct spc_status : request
{
  using result_type = std::vector<uint32_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::spc_status;

  virtual boost::any
  get(const xrt_core::device* device, const boost::any& dbg_ip_data) const = 0;
};

struct accel_deadlock_status : request
{
  using result_type = uint32_t;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::accel_deadlock_status;

  virtual boost::any
  get(const xrt_core::device* device, const boost::any& dbg_ip_data) const = 0;
};

struct boot_partition : request
{
  // default: 0
  // backup : 1
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::boot_partition;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct flush_default_only : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::flush_default_only;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct program_sc : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::program_sc;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct vmr_status : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::vmr_status;

  virtual boost::any
  get(const device*) const = 0;
};

struct extended_vmr_status : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::extended_vmr_status;

  virtual boost::any
  get(const device*) const = 0;
};

// Retrieve xclbin slot information.  This is a mapping
// from xclbin uuid to the slot index created by the driver
struct xclbin_slots : request
{
  using slot_id = uint32_t;

  struct slot_info {
    slot_id slot;
    std::string uuid;
  };

  using result_type = std::vector<slot_info>;
  static const key_type key = key_type::xclbin_slots;

  // Convert raw data to associative map
  static std::map<slot_id, xrt::uuid>
  to_map(const result_type& value);

  virtual boost::any
  get(const xrt_core::device* device) const = 0;
};

struct hwmon_sdm_serial_num : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_serial_num;

  virtual boost::any
  get(const device*) const = 0;
};

struct hwmon_sdm_oem_id : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_oem_id;

  virtual boost::any
  get(const device*) const = 0;
};

struct hwmon_sdm_board_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_board_name;

  virtual boost::any
  get(const device*) const = 0;
};

struct hwmon_sdm_active_msp_ver : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_active_msp_ver;

  virtual boost::any
  get(const device*) const = 0;
};

struct hwmon_sdm_mac_addr0 : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_mac_addr0;

  virtual boost::any
  get(const device*) const = 0;
};

struct hwmon_sdm_mac_addr1 : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_mac_addr1;

  virtual boost::any
  get(const device*) const = 0;
};

struct hwmon_sdm_revision : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_revision;

  virtual boost::any
  get(const device*) const = 0;
};

struct hwmon_sdm_fan_presence : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_fan_presence;

  virtual boost::any
  get(const device*) const = 0;
};

// Retrieve board MFG date from xocl hwmon_sdm driver
struct hwmon_sdm_mfg_date : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_mfg_date;

  virtual boost::any
  get(const device*) const = 0;
};

struct hotplug_offline : request
{
  using result_type = bool;
  static const key_type key = key_type::hotplug_offline;

  virtual boost::any
  get(const device*) const = 0;
};

struct cu_size : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::cu_size;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;
};

struct cu_read_range : request
{
  struct range_data {
    uint32_t start;
    uint32_t end;
  };
  using result_type = std::string;
  static const key_type key = key_type::cu_read_range;

  virtual boost::any
  get(const device*, modifier, const std::string&) const = 0;

  static range_data
  to_range(const std::string& str);
};

struct clk_scaling_info : request
{
  struct data {
    bool support;
    bool enable;
    bool pwr_scaling_ovrd_enable;
    bool temp_scaling_ovrd_enable;
    uint8_t temp_shutdown_limit;
    uint8_t temp_scaling_limit;
    uint8_t temp_scaling_ovrd_limit;
    uint16_t pwr_shutdown_limit;
    uint16_t pwr_scaling_limit;
    uint16_t pwr_scaling_ovrd_limit;
  };
  using result_type = std::vector<struct data>;
  using data_type = struct data;
  static const key_type key = key_type::clk_scaling_info;

  virtual boost::any
  get(const device*) const = 0;
};

struct xgq_scaling_enabled : request
{
  using result_type = bool; // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xgq_scaling_enabled;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct xgq_scaling_power_override : request
{
  using result_type = std::string; // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xgq_scaling_power_override;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

struct xgq_scaling_temp_override : request
{
  using result_type = std::string; // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xgq_scaling_temp_override;

  virtual boost::any
  get(const device*) const = 0;

  virtual void
  put(const device*, const boost::any&) const = 0;
};

} // query

} // xrt_core


#endif
