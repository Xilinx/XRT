// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved

#ifndef xrt_core_common_query_requests_h
#define xrt_core_common_query_requests_h
#include "info_aie2.h"
#include "error.h"
#include "query.h"
#include "uuid.h"

#include "core/common/shim/hwctx_handle.h"
#include "core/include/xclerr_int.h"

#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
  pcie_id,

  instance,
  edge_vendor,
  device_class,
  xrt_smi_config,
  xrt_smi_lists,
  xclbin_name,
  sequence_name,
  elf_name,
  mobilenet,

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
  xrt_resource_raw,
  dma_stream,
  device_status,
  kds_cu_info,
  sdm_sensor_info,
  kds_scu_info,
  ps_kernel,
  hw_context_info,
  hw_context_memory_info,
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

  aie_core_info_sysfs,
  aie_shim_info_sysfs,
  aie_mem_info_sysfs,

  total_cols,
  aie_status_version,
  aie_tiles_stats,
  aie_tiles_status_info,
  aie_partition_info,

  misc_telemetry,
  aie_telemetry,
  opcode_telemetry,
  rtos_telemetry,
  stream_buffer_telemetry,


  firmware_version,

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

  dimm_temp_0,
  dimm_temp_1,
  dimm_temp_2,
  dimm_temp_3,

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
  host_mem_addr,
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
  hwmon_sdm_target_msp_ver,
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
  performance_mode,
  preemption,
  frame_boundary_preemption,
  debug_ip_layout_path,
  debug_ip_layout,
  num_live_processes,
  device_clock_freq_mhz,
  trace_buffer_info,
  host_max_bandwidth_mbps,
  kernel_max_bandwidth_mbps,
  sub_device_path,
  read_trace_data,
  noop,

  xocl_errors_ex,
  xocl_ex_error_code2string
};

struct pcie_vendor : request
{
  using result_type = uint16_t;
  static const key_type key = key_type::pcie_vendor;
  static const char* name() { return "vendor"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;


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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(const result_type& value)
  {
    return boost::str
      (boost::format("%04x:%02x:%02x.%01x") % std::get<0>(value) %
       std::get<1>(value) % std::get<2>(value) % std::get<3>(value));
  }
};

/**
 *  Useful for identifying devices that utilize revision numbers. Prefer this request over pcie_device.
 */
struct pcie_id : request
{
  struct data {
    uint16_t device_id;
    uint8_t revision_id;
  };

  using result_type = data;
  static const key_type key = key_type::pcie_id;
  static const char* name() { return "pcie_id"; }

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  device_to_string(const result_type& value)
  {
    return boost::str(boost::format("%04x") % value.device_id);
  }

  static std::string
  revision_to_string(const result_type& value)
  {
    // The cast is required. This is a boost bug. https://github.com/boostorg/format/issues/60
    return boost::str(boost::format("%02x") % static_cast<uint16_t>(value.revision_id));
  }
};

struct edge_vendor : request
{
  using result_type = uint16_t;
  static const key_type key = key_type::edge_vendor;
  static const char* name() { return "vendor"; }

  virtual std::any
    get(const device*) const override = 0;

  static std::string
    to_string(result_type val)
  {
    return boost::str(boost::format("0x%x") % val);
  }
};

/**
 * Used to retieve the configuration required for the 
 * current device assuming a valid instance "type" is passed. The shim
 * decides the appropriate path and name to return, absolving XRT of
 * needing to know where to look.
 * This structure can be extended to provide other configurations supporting xrt-smi
 */

struct xrt_smi_config : request 
{
  enum class type {
    options_config
  };

  static std::string
  enum_to_str(const type& type)
  {
    switch (type) {
      case type::options_config:
        return "options_config";
    }
    return "unknown";
  }
  using result_type = std::string;
  static const key_type key = key_type::xrt_smi_config;
  static const char* name() { return "xrt_smi_config"; }

  virtual std::any
  get(const device*, const std::any& req_type) const override = 0;
};

/* Used to retrieve the list of validate tests and examine reports along with 
   their description and visibility tags. This returns the same list which is
   used by help printing to maintain concurrency between what is printed and 
   what is run by xrt-smi. This can be extended to other list assuming the 
   structure is kept the same as validate_tests and examine reports
*/
struct xrt_smi_lists : request
{
  enum class type {
    validate_tests,
    examine_reports,
    configure_option_options,
  };
  using result_type = std::vector<std::tuple<std::string, std::string, std::string>>;
  static const key_type key = key_type::xrt_smi_lists;
  static const char* name() { return "xrt_smi_lists"; }

  virtual std::any
  get(const device*, const std::any& req_type) const override = 0;

  // formatting of individual items for the vector
  static std::string
  to_string(const std::string& value)
  {
    return value;
  }
};

/**
 * Used to retrieve the path to an xclbin file required for the
 * current device assuming a valid xclbin "type" is passed. The shim
 * decides the appropriate path and name to return, absolving XRT of
 * needing to know where to look.
 */
struct xclbin_name : request
{
  enum class type {
    validate,
    gemm,
    validate_elf,
    gemm_elf,
    mobilenet_elf,
    preemption_4x4,
    preemption_4x8  
  };

  static std::string
  enum_to_str(const type& type)
  {
    switch (type) {
      case type::validate:
        return "validate";
      case type::gemm:
        return "gemm";
      case type::validate_elf:
        return "validate_elf";
      case type::gemm_elf:
        return "gemm_elf";
      case type::preemption_4x4:
        return "preemption_4x4";
      case type::preemption_4x8:
        return "preemption_4x8";
      case type::mobilenet_elf:
        return "mobilenet_elf";
    }
    return "unknown";
  }

  using result_type = std::string;
  static const key_type key = key_type::xclbin_name;
  static const char* name() { return "xclbin_name"; }

  virtual std::any
  get(const device*, const std::any& req_type) const override = 0;
};

/**
 * Used to retrieve the path to the dpu sequence file required for the
 * current device assuming a valid sequence "type" is passed. The shim
 * decides the appropriate path and name to return, absolving XRT of
 * needing to know where to look.
 */
struct sequence_name : request
{
  enum class type {
    df_bandwidth,
    tct_one_column,
    tct_all_column,
    gemm_int8
  };

  static std::string
  enum_to_str(const type& type)
  {
    switch (type) {
      case type::df_bandwidth:
        return "df_bandwidth";
      case type::tct_one_column:
        return "tct_one_column";
      case type::tct_all_column:
        return "tct_all_column";
      case type::gemm_int8:
        return "gemm_int8";
    }
    return "unknown";
  }

  using result_type = std::string;
  static const key_type key = key_type::sequence_name;
  static const char* name() { return "sequence_name"; }

  virtual std::any
  get(const device*, const std::any& req_type) const override = 0;
};

/**
 * Used to retrieve the path to the elf file required for the
 * current device assuming a valid elf "type" is passed. The shim
 * decides the appropriate path and name to return, absolving XRT of
 * needing to know where to look.
 */
struct elf_name : request
{
  enum class type {
    df_bandwidth, 
    tct_one_column, 
    tct_all_column, 
    aie_reconfig_overhead,
    gemm_int8, 
    nop,
    preemption_noop_4x4,
    preemption_noop_4x8,
    preemption_memtile_4x4,
    preemption_memtile_4x8, 
    mobilenet
  };

  static std::string
  enum_to_str(const type& type)
  {
    switch (type) {
      case type::df_bandwidth:
        return "df_bandwidth";
      case type::tct_one_column:
        return "tct_one_column";
      case type::tct_all_column:
        return "tct_all_column";
      case type::aie_reconfig_overhead:
        return "aie_reconfig_overhead";
      case type::gemm_int8:
        return "gemm_int8";
      case type::nop:
        return "nop";
      case type::preemption_noop_4x4:
        return "preemption_noop_4x4";
      case type::preemption_noop_4x8:
        return "preemption_noop_4x8";
      case type::preemption_memtile_4x4:
        return "preemption_memtile_4x4";
      case type::preemption_memtile_4x8:
        return "preemption_memtile_4x8";
      case type::mobilenet:
        return "mobilenet";
    }
    return "unknown";
  }

  using result_type = std::string;
  static const key_type key = key_type::elf_name;
  static const char* name() { return "elf_name"; }

  virtual std::any
  get(const device*, const std::any& req_type) const override = 0;
};

struct mobilenet : request 
{
  enum class type {
    mobilenet_ifm,
    mobilenet_param,
    buffer_sizes
  };

  static std::string
  enum_to_str(const type& type)
  {
    switch (type) {
      case type::mobilenet_ifm:
        return "mobilenet_ifm";
      case type::mobilenet_param:
        return "mobilenet_param";
      case type::buffer_sizes:
        return "buffer_sizes";
    }
    return "unknown";
  }

  using result_type = std::string;
  static const key_type key = key_type::mobilenet;
  static const char* name() { return "mobilenet"; }

  virtual std::any
  get(const device*, const std::any& req_type) const override = 0;
};

struct device_class : request
{
  enum class type {
    alveo,
    ryzen
  };

  static std::string
  enum_to_str(const type& type)
  {
    switch (type) {
      case type::alveo:
        return "Alveo";
      case type::ryzen:
        return "Ryzen";
    }
    return "unknown";
  }

  using result_type = type;
  static const key_type key = key_type::device_class;
  static const char* name() { return "device_class"; }

  virtual std::any
  get(const device*) const override = 0;
};

struct dma_threads_raw : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::dma_threads_raw;
  static const char* name() { return "dma_threads_raw"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct rom_uuid : request
{
  using result_type = std::string;
  static const key_type key = key_type::rom_uuid;
  static const char* name() { return "uuid"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

// dtbo_path is unique path used by libdfx library to load bitstream and device tree
// overlay(dtbo), this query reads dtbo_path from sysfs node
// Applicable only for embedded platforms
struct dtbo_path : request
{
  using result_type = std::string;
  using slot_id_type = uint32_t;

  static const key_type key = key_type::dtbo_path;

  virtual std::any
  get(const device*, const std::any& slot_id) const override = 0;
};

struct group_topology : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::group_topology;

  virtual std::any
  get(const device*) const override = 0;
};

struct temp_by_mem_topology : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::temp_by_mem_topology;

  virtual std::any
  get(const device*) const override = 0;
};

struct memstat : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::memstat;

  virtual std::any
  get(const device*) const override = 0;
};

struct memstat_raw : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::memstat_raw;

  virtual std::any
  get(const device*) const override = 0;
};

struct dma_stream : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::dma_stream;

  virtual std::any
  get(const device*) const override = 0;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct mem_topology_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::mem_topology_raw;

  virtual std::any
  get(const device*) const override = 0;
};

struct xclbin_full : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::xclbin_full;

  virtual std::any
  get(const device*) const override = 0;
};

struct ic_enable : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::ic_enable;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct ic_load_flash_address : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::ic_load_flash_address;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct aie_metadata : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_metadata;

  virtual std::any
  get(const device*) const override = 0;
};

struct aie_reg_read : request
{
  using result_type = uint32_t;
  using row_type = uint32_t;
  using col_type = uint32_t;
  using reg_type = std::string;
  static const key_type key = key_type::aie_reg_read;

  virtual std::any
  get(const device*, const std::any& row, const std::any& col, const std::any& reg) const override = 0;
};

struct aie_get_freq : request
{
  using result_type = uint64_t;
  using partition_id_type = uint32_t;
  static const key_type key = key_type::aie_get_freq;

  virtual std::any
  get(const device*, const std::any& partition_id) const override = 0;
};

struct aie_set_freq : request
{
  using result_type = bool;
  using partition_id_type = uint32_t;
  using freq_type = uint64_t;
  static const key_type key = key_type::aie_set_freq;

  virtual std::any
  get(const device*, const std::any& partition_id, const std::any& freq) const override = 0;
};

struct graph_status : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::graph_status;

  virtual std::any
  get(const device*) const override = 0;
};

struct ip_layout_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::ip_layout_raw;

  virtual std::any
  get(const device*) const override = 0;
};

struct debug_ip_layout_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::debug_ip_layout_raw;

  virtual std::any
  get(const device*) const override = 0;
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
    int8_t unitm {};
  };
  using result_type = std::vector<sensor_data>;
  using req_type = sdr_req_type;
  using data_type = sensor_data;
  static const key_type key = key_type::sdm_sensor_info;

  virtual std::any
  get(const device*, const std::any& req_type) const override = 0;
};

/**
 * Extract the status of the device
 * This states whether or not a device is stuck due to an xclbin issue
 */
struct device_status : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::device_status;

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  parse_status(const result_type status)
  {
    switch (status) {
      case 0:
        return "HEALTHY";
      case 1:
        return "HANG";
      case 2:
        return "UNKNOWN";
      default:
        throw xrt_core::system_error(EINVAL, "Invalid device status: " + std::to_string(status));
    }
  }
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
  using result_type = std::vector<data>;
  using data_type = struct data;
  static const key_type key = key_type::kds_cu_info;

  virtual std::any
  get(const device*) const override = 0;
};

struct ps_kernel : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::ps_kernel;

  virtual std::any
  get(const device*) const override = 0;
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

  virtual std::any
  get(const device*) const override = 0;
};

/**
 * Return all hardware contexts within a device
 */
struct hw_context_info : request
{
  struct metadata {
    std::string id;
    std::string xclbin_uuid;
  };

  /**
   * A structure to represent a single hardware context on any device type. This
   * structure must contain all data that makes up a hardware context across
   * all device types.
   * 
   * The only field that must be populated is the xclbin uuid.
   * All other fields can be populated as required by the appropriate device.
   * As new compute types are created they must be accounted for here
   * 
   * For example:
   *  Alveo -> populate only the PL compute units
   *  Versal -> populate PL and PS compute units
   */
  struct data {
    struct metadata metadata;
    kds_cu_info::result_type pl_compute_units;
    kds_scu_info::result_type ps_compute_units;
  };

  using result_type = std::vector<struct data>;
  using data_type = struct data;
  static const key_type key = key_type::hw_context_info;

  virtual std::any
  get(const device*) const override = 0;
};

/**
 * Return all hardware contexts' memory info within a device
 */
struct hw_context_memory_info : request
{
  /**
   * A structure to represent a single hardware context's memory contents on
   * any device type. This structure contains all data that makes up a 
   * hardware context memory structure across all device types.
   */
  struct data {
    hw_context_info::metadata metadata;
    mem_topology_raw::result_type topology;
    group_topology::result_type grp_topology;
    memstat_raw::result_type statistics;
    temp_by_mem_topology::result_type temperature;
  };

  using result_type = std::vector<struct data>;
  using data_type = struct data;
  static const key_type key = key_type::hw_context_memory_info;

  virtual std::any
  get(const device*) const override = 0;
};

struct clock_freq_topology_raw : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::clock_freq_topology_raw;

  virtual std::any
  get(const device*) const override = 0;
};

/**
 * Return some of the resouces within a NPU device
 */
struct xrt_resource_raw : request
{
  /**
   * enum class resource_type - represents the different types of resources
   */
  enum class resource_type
  {
    npu_clk_max,   // Max H-Clocks, query returns uint64 value
    npu_tops_max,  // Max TOPs, query returns double value
    npu_task_max,  // Max Tasks, query returns uint64 value
    npu_tops_curr, // Current TOPs, query returns double value
    npu_task_curr  // Current Tasks, query returns uint64 value
  };

  /**
   * The buffer that holds the resource query data
   */
  struct xrt_resource_query
  {
    resource_type type;
    union
    {
      uint64_t data_uint64; // holds the value represented as uint64
      double data_double;   // holds the value represented as double
    };
  };

  using result_type = std::vector<xrt_resource_query>; // get value type
  static const key_type key = key_type::xrt_resource_raw;

  static std::string
  get_name(xrt_core::query::xrt_resource_raw::resource_type type)
  {
    switch (type)
    {
    case resource_type::npu_clk_max:
      return "Max Supported H-Clocks";
    case resource_type::npu_tops_max:
      return "Max Supported TOPs";
    case resource_type::npu_task_max:
      return "Max Supported Tasks";
    case resource_type::npu_tops_curr:
      return "Current TOPs";
    case resource_type::npu_task_curr:
      return "Current Tasks";
    default:
      throw xrt_core::internal_error("enum value does not exists");
    }
  }

  virtual std::any
  get(const device *) const override = 0;
};

struct xmc_version : request
{
  using result_type = std::string;
  static const key_type key = key_type::xmc_version;
  static const char* name() { return "xmc_version"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_reg_base : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::xmc_reg_base;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_support : request
{
  using result_type = bool;       // get value type
  static const key_type key = key_type::xmc_scaling_support;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_critical_temp_threshold : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_critical_temp_threshold;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_critical_pow_threshold : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_critical_pow_threshold;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_threshold_power_limit : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_threshold_power_limit;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_threshold_temp_limit : request
{
  using result_type = std::string;       // get value type
  static const key_type key = key_type::xmc_scaling_threshold_temp_limit;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_power_override_enable : request
{
  using result_type = bool;       // get value type
  static const key_type key = key_type::xmc_scaling_power_override_enable;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_temp_override_enable : request
{
  using result_type = bool;       // get value type
  static const key_type key = key_type::xmc_scaling_temp_override_enable;

  virtual std::any
  get(const device*) const override = 0;
};

struct xmc_scaling_enabled : request
{
  using result_type = bool;       // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xmc_scaling_enabled;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct xmc_scaling_power_override: request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type
  static const key_type key = key_type::xmc_scaling_power_override;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

};

struct xmc_scaling_temp_override: request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type
  static const key_type key = key_type::xmc_scaling_temp_override;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

};

struct xmc_scaling_reset : request
{
  using value_type = std::string;   // put value type
  static const key_type key = key_type::xmc_scaling_reset;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct xmc_qspi_status : request
{
  // Returning qspi write protection status as <primary qspi, recovery qspi>
  using result_type = std::pair<std::string, std::string>;
  static const key_type key = key_type::xmc_qspi_status;

  virtual std::any
  get(const device*) const override = 0;
};

struct m2m : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::m2m;

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

// Retrieve support for extended asynchronous xocl errors from xocl driver
struct xocl_errors_ex : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::xocl_errors_ex;

  virtual std::any
    get(const device*) const override = 0;

  static bool
    to_bool(const result_type& value)
  {
    return (value == std::numeric_limits<uint32_t>::max())
      ? false : value;
  }
};

// Retrieve extended asynchronous xocl errors string corresponding to the error code from xocl driver
struct xocl_ex_error_code2string : request
{
  using result_type = std::string;  // get value type
  static const key_type key = key_type::xocl_ex_error_code2string;

  virtual std::any
    get(const device*) const override = 0;

  static std::string
    to_string(const std::string& errstr)
  {
    return std::string(errstr);
  }
};

// Retrieve asynchronous xocl errors from xocl driver
struct xocl_errors : request
{
  using result_type = std::vector<char>;
  static const key_type key = key_type::xocl_errors;

  virtual std::any
  get(const device*) const override = 0;

  // Parse sysfs line and from class get error code and timestamp
  XRT_CORE_COMMON_EXPORT
  static std::pair<uint64_t, uint64_t>
  to_value(const std::vector<char>& buf, xrtErrorClass ecl);

  // Parse buffer, get error code and timestamp
  XRT_CORE_COMMON_EXPORT
  static std::tuple<uint64_t, uint64_t, uint64_t>
  to_ex_value(const std::vector<char>& buf, xrtErrorClass ecl);

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

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(const result_type& value)
  {
    return value;
  }
};

// Used to retrive aie core tile status information from sysfs
struct aie_core_info_sysfs : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_core_info_sysfs;

  virtual std::any
  get(const device*) const override = 0;
};

// Used to retrive aie shim tile status information from sysfs
struct aie_shim_info_sysfs : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_shim_info_sysfs;

  virtual std::any
  get(const device*) const override = 0;
};

// Used to retrive aie mem tile status information from sysfs
struct aie_mem_info_sysfs : request
{
  using result_type = std::string;
  static const key_type key = key_type::aie_mem_info_sysfs;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve total number of columns on device
struct total_cols : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::total_cols;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrive aie status version
// We use binary parser for parsing info from driver
// This version is used as handshake b/w userspace and driver
struct aie_status_version : request
{
  struct aie_version {
    uint16_t major;
    uint16_t minor;
  };

  using result_type = aie_version;
  static const key_type key = key_type::aie_status_version;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrive device specific Aie tiles(core, mem, shim) tiles info
// like total num of rows, cols, and num of core, mem, shim rows
// num of dma channels, locks, events
struct aie_tiles_stats : request
{
  using result_type = aie2::aie_tiles_info;
  static const key_type key = key_type::aie_tiles_stats;

  virtual std::any
  get(const device*) const override = 0;
};

// Used to retrive aie tiles status info
struct aie_tiles_status_info : request
{
  struct parameters
  {
    uint32_t col_size; // The size of a status buffer for a column
    uint16_t max_num_cols; // The maxmimum number of columns supported on the device
  };

  struct result
  {
    std::vector<char> buf;
    /**
     * A bitmap where the bit position indicates a column index.
     * A one indicates to an active column.
     * Ex. 00001100 Indicates columns 3 and 4 are active.
     */
    uint32_t cols_filled = 0;
  };

  using result_type = result;
  static const key_type key = key_type::aie_tiles_status_info;

  virtual std::any
  get(const device* device, const std::any& param) const override = 0;
};

// Retrieves the aie partition info.
// An aie partition consists of a starting column and a number of columns.
// The information is returned via hardware contexts as the driver keeps
// track of the data this way.
// Hardware contexts may share the same aie partition.
struct aie_partition_info : request
{
  struct qos_info {
    uint64_t    gops;           // Giga operations per second
    uint64_t    egops;          // Effective giga operations per second
    uint64_t    fps;            // Frames per second
    uint64_t    dma_bandwidth;  // DMA bandwidth
    uint64_t    latency;        // Frame response latency
    uint64_t    frame_exec_time;// Frame execution time
    uint64_t    priority;       // Request priority
  };
  
  struct data
  {
    hw_context_info::metadata metadata;
    uint64_t    start_col = 0;
    uint64_t    num_cols = 0;
    int         pid = -1;
    bool        is_suspended = false;
    uint64_t    instruction_mem = 0;
    uint64_t    command_submissions = 0;
    uint64_t    command_completions = 0;
    uint64_t    migrations = 0;
    uint64_t    preemptions = 0;
    uint64_t    errors = 0;
    uint64_t    pasid = 0;
    qos_info    qos {};
    uint64_t    suspensions;    // Suspensions by context switching and idle detection
  };

  using result_type = std::vector<struct data>;
  static const key_type key = key_type::aie_partition_info;

  virtual std::any
  get(const device* device) const override = 0;

  static std::string
  parse_priority_status(const uint64_t prio_status)
  {
    switch(prio_status) {
      case 256: //0x100
        return "Realtime";
      case 384: //0x180
        return "High";
      case 512: //0x200
        return "Normal";
      case 640: //0x280
        return "Low";
      default:
        return "N/A";
    }
  }
};

// Retrieves the AIE telemetry info for the device
// While the AIE status is for live information. This is historical information
// of the AIE column operation.
// This query is available for Ryzen devices
struct aie_telemetry : request
{
  struct data {
    uint64_t deep_sleep_count;
  };

  using result_type = std::vector<data>;
  static const key_type key = key_type::aie_telemetry;

  virtual std::any
  get(const device* device) const override = 0;
};

// Retrieves the miscellaneous telemetry info for the device
// Various bits of information are not tied to anything in AIE devices.
// This is how to get them!
// This query is available for Ryzen devices
struct misc_telemetry : request
{
  struct data {
    uint64_t l1_interrupts;
  };

  using result_type = data;
  static const key_type key = key_type::misc_telemetry;

  virtual std::any
  get(const device* device) const override = 0;
};

// Retrieves the opcode telemetry info for the device
// Opcodes are the commands that are sent to the device such as EXEC_BUF or SYNC_BO
// This query is available for Ryzen devices
struct opcode_telemetry : request
{
  struct data {
    uint64_t count;
  };

  using result_type = std::vector<data>;
  static const key_type key = key_type::opcode_telemetry;

  virtual std::any
  get(const device* device) const override = 0;
};

// Retrieves the rtos telemetry info for the device
// Returns historical data about how the rtos tasks operate
// This query is available for Ryzen devices
struct rtos_telemetry : request
{
  struct dtlb_data {
    uint64_t misses;
  };

  struct preempt_data {
    uint64_t slot_index;
    uint64_t preemption_flag_set;
    uint64_t preemption_flag_unset;
    uint64_t preemption_checkpoint_event;
    uint64_t preemption_frame_boundary_events;
  };

  struct data {
    uint64_t user_task = 0;
    uint64_t context_starts = 0;
    uint64_t schedules = 0;
    uint64_t syscalls = 0;
    uint64_t dma_access = 0;
    uint64_t resource_acquisition = 0;
    std::vector<dtlb_data> dtlbs;
    preempt_data preemption_data {};
  };

  using result_type = std::vector<data>;
  static const key_type key = key_type::rtos_telemetry;

  virtual std::any
  get(const device* device) const override = 0;
};

// Retrieve the stream buffer telemetry from the device
// Returns historical data about how the stream buffers operate
// Applicable to Ryzen devices
struct stream_buffer_telemetry : request
{
  struct data {
    uint64_t tokens;
  };

  using result_type = std::vector<data>;
  static const key_type key = key_type::stream_buffer_telemetry;

  virtual std::any
  get(const device* device) const override = 0;
};

// Retrieves the firmware version of the device.
struct firmware_version : request
{
  struct data
  {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t build;
  };

  using result_type = data;
  static const key_type key = key_type::firmware_version;

  virtual std::any
  get(const device* device) const override = 0;
};

struct clock_freqs_mhz : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::clock_freqs_mhz;
  static const char* name() { return "clocks"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct max_shared_host_mem_aperture_bytes : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::max_shared_host_mem_aperture_bytes;

  virtual std::any
  get(const device*) const override = 0;
};

struct status_mig_calibrated : request
{
  using result_type = bool;
  static const key_type key = key_type::status_mig_calibrated;
  static const char* name() { return "mig_calibrated"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(const result_type& value)
  {
    return value.compare("A") == 0 ? "true" : "false";
  }
};

struct fan_speed_rpm : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::fan_speed_rpm;

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct ddr_temp_1 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_temp_1;

  virtual std::any
  get(const device*) const override = 0;
};

struct ddr_temp_2 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_temp_2;

  virtual std::any
  get(const device*) const override = 0;
};

struct ddr_temp_3 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ddr_temp_3;

  virtual std::any
  get(const device*) const override = 0;
};

struct hbm_temp : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::hbm_temp;

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct dimm_temp_0 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::dimm_temp_0;

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct dimm_temp_1 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::dimm_temp_1;

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct dimm_temp_2 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::dimm_temp_2;

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(result_type value)
  {
    return std::to_string(value);
  }
};

struct dimm_temp_3 : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::dimm_temp_3;

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct mac_addr_first : request
{
  using result_type = std::string;
  static const key_type key = key_type::mac_addr_first;
  static const char* name() { return "mac_addr_first"; }

  virtual std::any
  get(const device*) const override = 0;
};

struct mac_addr_list : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::mac_addr_list;
  static const char* name() { return "mac_addr_list"; }

  virtual std::any
  get(const device*) const override = 0;
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

  virtual std::any
  get(const device*) const override = 0;
};

struct firewall_detect_level : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_detect_level;
  static const char* name() { return "level"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

};

struct firewall_status : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::firewall_status;
  static const char* name() { return "status"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(result_type value)
  {
    return value ? "true" : "false";
  }
};

struct host_mem_addr : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::host_mem_addr;
  static const char* name() { return "host_mem_addr"; }

  virtual std::any
  get(const device*) const override = 0;

  static std::string
  to_string(result_type val)
  {
    return std::to_string(val);
  }
};

struct host_mem_size : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::host_mem_size;
  static const char* name() { return "host_mem_size"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*, modifier m, const std::string&) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct mig_ecc_enabled : request
{
  using result_type = bool;
  static const key_type key = key_type::mig_ecc_enabled;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct mig_ecc_status : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_status;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct mig_ecc_ce_cnt : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ce_cnt;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct mig_ecc_ue_cnt : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ue_cnt;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct mig_ecc_ce_ffa : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ce_ffa;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct mig_ecc_ue_ffa : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::mig_ecc_ue_ffa;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct is_mfg : request
{
  using result_type = bool;
  static const key_type key = key_type::is_mfg;

  virtual std::any
  get(const device*) const override = 0;
};

struct mfg_ver : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::mfg_ver;

  virtual std::any
  get(const device*) const override = 0;
};

struct is_recovery : request
{
  using result_type = bool;
  static const key_type key = key_type::is_recovery;

  virtual std::any
  get(const device*) const override = 0;
};


// struct is_versal - check if device is versal or not
// A value of true means it is a versal device.
// This entry is needed as some of the operations are handled
// differently on versal devices compared to Alveo devices
struct is_versal : request
{
  using result_type = bool;
  static const key_type key = key_type::is_versal;

  virtual std::any
  get(const device*) const override = 0;
};

// struct is_ready - A boolean stating
// if the specified device is ready for
// XRT operations such as program or reset
struct is_ready : request
{
  using result_type = bool;
  static const key_type key = key_type::is_ready;

  virtual std::any
  get(const device*) const override = 0;
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

  virtual std::any
  get(const device*) const override = 0;
};

struct f_flash_type : request
{
  using result_type = std::string;
  static const key_type key = key_type::f_flash_type;

  virtual std::any
  get(const device*) const override = 0;
};

struct flash_type : request
{
  using result_type = std::string;
  static const key_type key = key_type::flash_type;
  static const char* name() { return "flash_type"; }

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct board_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::board_name;

  virtual std::any
  get(const device*) const override = 0;
};

struct flash_bar_offset : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::flash_bar_offset;

  virtual std::any
  get(const device*) const override = 0;
};

struct rp_program_status : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;   // put value type
  static const key_type key = key_type::rp_program_status;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct shared_host_mem : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::shared_host_mem;

  virtual std::any
  get(const device*) const override = 0;
};

struct enabled_host_mem : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::enabled_host_mem;

  virtual std::any
  get(const device*) const override = 0;
};

struct clock_timestamp : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::clock_timestamp;

  virtual std::any
  get(const device*) const override = 0;
};

struct mailbox_metrics : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::mailbox_metrics;

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct config_mailbox_channel_switch : request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type

  static const key_type key = key_type::config_mailbox_channel_switch;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct config_xclbin_change : request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type

  static const key_type key = key_type::config_xclbin_change;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct cache_xclbin : request
{
  using result_type = std::string;  // get value type
  using value_type = std::string;   // put value type

  static const key_type key = key_type::cache_xclbin;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct ert_sleep : request
{
  using result_type = uint32_t;  // get value type
  using value_type = uint32_t;   // put value type

  static const key_type key = key_type::ert_sleep;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

};

struct ert_cq_read : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cq_read;

  virtual std::any
  get(const device*) const override = 0;
};

struct ert_cq_write : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cq_write;

  virtual std::any
  get(const device*) const override = 0;
};

struct ert_cu_read : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cu_read;

  virtual std::any
  get(const device*) const override = 0;
};

struct ert_cu_write : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::ert_cu_write;

  virtual std::any
  get(const device*) const override = 0;
};


struct ert_data_integrity : request
{
  using result_type = bool;
  static const key_type key = key_type::ert_data_integrity;

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;

  static ert_status_data
  to_ert_status(const result_type& strs);
};

struct noop : request
{
  using result_type = uint64_t;
  static const key_type key = key_type::noop;

  virtual std::any
  get(const device*) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct heartbeat_err_code : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::heartbeat_err_code;

  virtual std::any
  get(const device*) const override = 0;
};

struct heartbeat_count : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::heartbeat_count;

  virtual std::any
  get(const device*) const override = 0;
};

struct heartbeat_stall : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::heartbeat_stall;

  virtual std::any
  get(const device*) const override = 0;
};

struct aim_counter : request
{
  using result_type = std::vector<uint64_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::aim_counter;

  virtual std::any
  get(const xrt_core::device* device, const std::any& dbg_ip_data) const override = 0;
};

struct am_counter : request
{
  using result_type = std::vector<uint64_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::am_counter;

  virtual std::any
  get(const xrt_core::device* device, const std::any& dbg_ip_data) const override = 0;
};

struct asm_counter : request
{
  using result_type = std::vector<uint64_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::asm_counter;

  virtual std::any
  get(const xrt_core::device* device, const std::any& dbg_ip_data) const override = 0;
};

struct lapc_status : request
{
  using result_type = std::vector<uint32_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::lapc_status;

  virtual std::any
  get(const xrt_core::device* device, const std::any& dbg_ip_data) const override = 0;
};

struct spc_status : request
{
  using result_type = std::vector<uint32_t>;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::spc_status;

  virtual std::any
  get(const xrt_core::device* device, const std::any& dbg_ip_data) const override = 0;
};

struct accel_deadlock_status : request
{
  using result_type = uint32_t;
  using debug_ip_data_type = debug_ip_data*;
  static const key_type key = key_type::accel_deadlock_status;

  virtual std::any
  get(const xrt_core::device* device, const std::any& dbg_ip_data) const override = 0;
};

struct boot_partition : request
{
  // default: 0
  // backup : 1
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::boot_partition;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct flush_default_only : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::flush_default_only;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct program_sc : request
{
  using result_type = uint32_t;
  using value_type = uint32_t;
  static const key_type key = key_type::program_sc;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};


// Returns the status the vmr subdevice. This
// includes boot information and other data.
// In the user partition only the boot information
// is returned.
struct vmr_status : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::vmr_status;

  virtual std::any
  get(const device*) const override = 0;
};

struct extended_vmr_status : request
{
  using result_type = std::vector<std::string>;
  static const key_type key = key_type::extended_vmr_status;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve xclbin slot information.  This is a mapping
// from xclbin uuid to the slot index created by the driver
struct xclbin_slots : request
{
  using slot_id = hwctx_handle::slot_id;

  struct slot_info {
    slot_id slot;
    std::string uuid;
  };

  using result_type = std::vector<slot_info>;
  static const key_type key = key_type::xclbin_slots;

  // Convert raw data to associative map
  static std::map<slot_id, xrt::uuid>
  to_map(const result_type& value);

  virtual std::any
  get(const xrt_core::device* device) const override = 0;
};

// Retrieve Board Serial number from xocl hwmon_sdm driver
struct hwmon_sdm_serial_num : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_serial_num;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve OEM ID data from xocl hwmon_sdm driver
struct hwmon_sdm_oem_id : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_oem_id;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve Board name from xocl hwmon_sdm driver
struct hwmon_sdm_board_name : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_board_name;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve active SC version from xocl hwmon_sdm driver
struct hwmon_sdm_active_msp_ver : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_active_msp_ver;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve expected SC version from xocl hwmon_sdm driver
struct hwmon_sdm_target_msp_ver : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_target_msp_ver;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve MAC ADDR0 from xocl hwmon_sdm driver
struct hwmon_sdm_mac_addr0 : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_mac_addr0;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve MAC ADDR1 from xocl hwmon_sdm driver
struct hwmon_sdm_mac_addr1 : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_mac_addr1;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve Revision data from xocl hwmon_sdm driver
struct hwmon_sdm_revision : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_revision;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve FAN presence status from xocl hwmon_sdm driver
struct hwmon_sdm_fan_presence : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_fan_presence;

  virtual std::any
  get(const device*) const override = 0;
};

// Retrieve board MFG date from xocl hwmon_sdm driver
struct hwmon_sdm_mfg_date : request
{
  using result_type = std::string;
  static const key_type key = key_type::hwmon_sdm_mfg_date;

  virtual std::any
  get(const device*) const override = 0;
};

struct hotplug_offline : request
{
  using result_type = bool;
  static const key_type key = key_type::hotplug_offline;

  virtual std::any
  get(const device*) const override = 0;
};

struct cu_size : request
{
  using result_type = uint32_t;
  static const key_type key = key_type::cu_size;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;
};

struct cu_read_range : request
{
  struct range_data {
    uint32_t start;
    uint32_t end;
  };
  using result_type = std::string;
  static const key_type key = key_type::cu_read_range;

  virtual std::any
  get(const device*, modifier, const std::string&) const override = 0;

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

  virtual std::any
  get(const device*) const override = 0;
};

struct xgq_scaling_enabled : request
{
  using result_type = bool; // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xgq_scaling_enabled;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct xgq_scaling_power_override : request
{
  using result_type = std::string; // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xgq_scaling_power_override;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct xgq_scaling_temp_override : request
{
  using result_type = std::string; // get value type
  using value_type = std::string; // put value type
  static const key_type key = key_type::xgq_scaling_temp_override;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;
};

struct performance_mode : request
{
  // Get and set power mode of device
  enum class power_type
  {
    basic = 0, // deafult
    powersaver,
    balanced,
    performance,
    turbo
  };
  using result_type = uint32_t;  // get value type
  using value_type = power_type;   // put value type

  static const key_type key = key_type::performance_mode;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

  static std::string
  parse_status(const result_type status)
  {
    switch(status) {
      case 0:
        return "Default";
      case 1:
        return "Powersaver";
      case 2:
        return "Balanced";
      case 3:
        return "Performance";
      case 4:
        return "Turbo";
      default:
        throw xrt_core::system_error(EINVAL, "Invalid performance status: " + std::to_string(status));
    }
  }
};

/*
 * this request force enables or disables layer boundary pre-emption globally
 * 1: enable; 0: disable
*/
struct preemption : request
{
  using result_type = uint32_t;  // get value type
  using value_type = uint32_t;   // put value type

  static const key_type key = key_type::preemption;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

};

/*
 * this request force enables or disables frame boundary pre-emption globally
 * 1: enable; 0: disable
*/
struct frame_boundary_preemption : request
{
  using result_type = uint32_t;  // get value type
  using value_type = uint32_t;   // put value type

  static const key_type key = key_type::frame_boundary_preemption;

  virtual std::any
  get(const device*) const override = 0;

  virtual void
  put(const device*, const std::any&) const override = 0;

};

struct debug_ip_layout_path : request
{
  // Get debug ip layout path
  // Used by xdp code
  using result_type = std::string; // get value type
  static const key_type key = key_type::debug_ip_layout_path;

  virtual std::any
  get(const device*, const std::any&) const override = 0;
};

struct debug_ip_layout : request
{
  // Get debug ip layout
  using result_type = std::vector<char>; // get value type
  static const key_type key = key_type::debug_ip_layout;

  virtual std::any
  get(const device*) const override = 0;
};

struct num_live_processes : request
{
  // Get the count of number of live processes
  using result_type = uint32_t; // get value type
  static const key_type key = key_type::num_live_processes;

  virtual std::any
  get(const device*) const override = 0;
};

struct device_clock_freq_mhz : request
{
  // Get device clock frequency in MHz
  using result_type = double; // get value type
  static const key_type key = key_type::device_clock_freq_mhz;

  virtual std::any
  get(const device*) const override = 0;
};

struct trace_buffer_info : request
{
  struct info {
    uint32_t samples;
    uint32_t buf_size;
  };
  // Get trace buffer info
  using result_type = info; // get value type
  static const key_type key = key_type::trace_buffer_info;

  virtual std::any
  get(const device*, const std::any&) const override = 0;
};

struct host_max_bandwidth_mbps : request
{
  // Get Max host bandwidth MBps
  using result_type = double; // get value type
  static const key_type key = key_type::host_max_bandwidth_mbps;

  virtual std::any
  get(const device*, const std::any&) const override = 0;
};

struct kernel_max_bandwidth_mbps : request
{
  // Get Max host bandwidth MBps
  using result_type = double; // get value type
  static const key_type key = key_type::kernel_max_bandwidth_mbps;

  virtual std::any
  get(const device*, const std::any&) const override = 0;
};

struct sub_device_path : request
{
  struct args {
    std::string subdev;
    uint32_t index;
  };
  // Get sub device sysfs path
  using result_type = std::string; // get value type
  static const key_type key = key_type::sub_device_path;

  virtual std::any
  get(const device*, const std::any&) const override = 0;
};

struct read_trace_data : request
{
  struct args {
    uint32_t  buf_size;
    uint32_t  samples;
    uint64_t  ip_base_addr;
    uint32_t& words_per_sample;
  };
  // Get sub device sysfs path
  using result_type = std::vector<uint32_t>; // get value type
  static const key_type key = key_type::read_trace_data;

  virtual std::any
  get(const device*, const std::any&) const override = 0;
};
} // query

} // xrt_core


#endif
