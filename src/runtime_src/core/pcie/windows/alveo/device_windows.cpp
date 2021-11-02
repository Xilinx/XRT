/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */
#define XRT_CORE_PCIE_WINDOWS_SOURCE
#define XCL_DRIVER_DLL_EXPORT
#include "device_windows.h"
#include "mgmt.h"
#include "shim.h"
#include "core/common/query_requests.h"
#include "core/common/utils.h"
#include "core/include/xrt.h"
#include "core/include/xclfeatures.h"

#include "core/common/debug_ip.h"

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <type_traits>
#include <string>
#include <map>
#include <mutex>
#include <iostream>

#pragma warning(disable : 4100 4996)

namespace {

namespace query = xrt_core::query;
using key_type = xrt_core::query::key_type;
using qtype = std::underlying_type<query::key_type>::type;

xrt_core::query::no_such_key
mgmtpf_not_supported_error(key_type key)
{
  return xrt_core::query::no_such_key
    (key, "query request (" + std::to_string(static_cast<qtype>(key)) + ") not supported for mgmtpf on windows");
}

xrt_core::query::no_such_key
userpf_not_supported_error(key_type key)
{
  return xrt_core::query::no_such_key
    (key, "query request (" + std::to_string(static_cast<qtype>(key)) + ") not supported for userpf on windows");
}

xrt_core::query::no_such_key
unexpected_query_request_key(key_type key)
{
  return xrt_core::query::no_such_key
    (key, "unexpected query request ( " + std::to_string(static_cast<qtype>(key)) + ")");
}

struct flash
{
  using result_type = std::string;

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return "spi";
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    return "spi";
  }
};

struct ready
{
  using result_type = bool;

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return true;
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    return true;
  }
};


struct firewall
{
  using result_type = boost::any;

  static xcl_firewall
  init_firewall_info(const xrt_core::device* dev)
  {
    xcl_firewall info = { 0 };
    userpf::get_firewall_info(dev->get_user_handle(), &info);
    return info;
  }

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xcl_firewall> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_firewall_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;

    switch (key) {
    case key_type::firewall_detect_level:
      return query::firewall_detect_level::result_type(info.err_detected_level);
    case key_type::firewall_status:
      return query::firewall_status::result_type(info.err_detected_status);
    case key_type::firewall_time_sec:
      return query::firewall_time_sec::result_type(info.err_detected_time);
    default:
      throw unexpected_query_request_key(key);
    }
    // No query for max_level, curr_status and curr_level
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get(device,key);
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct mig
{
  using result_type = boost::any;

  static xcl_mig_ecc
  init_mig_ecc_info(const xrt_core::device* dev)
  {
    xcl_mig_ecc info = { 0 };
    userpf::get_mig_ecc_info(dev->get_user_handle(), &info);
    return info;
  };

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xcl_mig_ecc> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_mig_ecc_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;

    switch (key) {
    case key_type::mig_ecc_enabled:
      return query::mig_ecc_enabled::result_type(info.ecc_enabled);
    case key_type::mig_ecc_status:
      return query::mig_ecc_status::result_type(info.ecc_status);
    case key_type::mig_ecc_ce_cnt:
      return query::mig_ecc_ce_cnt::result_type(info.ecc_ce_cnt);
    case key_type::mig_ecc_ue_cnt:
      return query::mig_ecc_ue_cnt::result_type(info. ecc_ue_cnt);
    case key_type::mig_ecc_ce_ffa:
      return query::mig_ecc_ce_ffa::result_type(info.ecc_ce_ffa);
    case key_type::mig_ecc_ue_ffa:
      return query::mig_ecc_ue_ffa::result_type(info.ecc_ue_ffa);
    default:
      throw unexpected_query_request_key(key);
    }
    // No query for mem_type and mem_idx
  }

  static result_type
  user(const xrt_core::device* device, key_type key, query::request::modifier, const std::string&)
  {
    return get(device,key);
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key, query::request::modifier, const std::string&)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct board
{
  using result_type = boost::any;


  static xcl_board_info
  init_board_info(const xrt_core::device* dev)
  {
    xcl_board_info info = { 0 };
    if (auto mhdl = dev->get_mgmt_handle())
      mgmtpf::get_board_info(mhdl, &info);
    else if (auto uhdl = dev->get_user_handle())
      userpf::get_board_info(uhdl, &info);
    return info;
  };

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xcl_board_info> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_board_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;

    switch (key) {
    case key_type::xmc_serial_num:
      return query::xmc_serial_num::result_type(reinterpret_cast<const char*>(info.serial_num));
    case key_type::xmc_sc_version:
      return query::xmc_sc_version::result_type(reinterpret_cast<const char*>(info.bmc_ver));
    case key_type::max_power_level:
		return query::max_power_level ::result_type(info.max_power);
    case key_type::fan_fan_presence:
      return query::fan_fan_presence::result_type(info.fan_presence == 0 ? "P" : "A");
    case key_type::xmc_board_name:
      return query::xmc_board_name::result_type(reinterpret_cast<const char*>(info.bd_name));
    case key_type::mac_addr_first:
      return std::string(info.mac_addr_first);
    case key_type::mac_contiguous_num:
      return query::mac_contiguous_num::result_type(info.mac_contiguous_num);
    case key_type::mac_addr_list:
      return std::vector<std::string>{ std::string(info.mac_addr0), std::string(info.mac_addr1), std::string(info.mac_addr2), std::string(info.mac_addr3) };
    default:
      throw unexpected_query_request_key(key);
    }
    // No query for mac_addr0, mac_addr1, mac_addr2, mac_addr3, revision, bd_name and config_mode
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get(device, key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    return get(device, key);
  }
};

struct sensor
{
  using result_type = boost::any;

  static xcl_sensor
  init_sensor_info(const xrt_core::device* dev)
  {
    xcl_sensor info = { 0 };
    userpf::get_sensor_info(dev->get_user_handle(), &info);
    return info;
  }

  static result_type
  get_info(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xcl_sensor> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_sensor_info(device));
      it = ret.first;
    }

    const xcl_sensor& info = (*it).second;
    uint64_t val = 0;

    switch (key) {
    case key_type::v12v_pex_millivolts:
      return query::v12v_pex_millivolts::result_type(info.vol_12v_pex);
    case key_type::v12v_aux_millivolts:
      return query::v12v_aux_millivolts::result_type(info.vol_12v_aux);
    case key_type::v12v_pex_milliamps:
      return query::v12v_pex_milliamps::result_type(info.cur_12v_pex);
    case key_type::v12v_aux_milliamps:
      return query::v12v_aux_milliamps::result_type(info.cur_12v_aux);
    case key_type::v3v3_pex_millivolts:
      return query::v3v3_pex_millivolts::result_type(info.vol_3v3_pex);
    case key_type::v3v3_aux_millivolts:
      return query::v3v3_aux_millivolts::result_type(info.vol_3v3_aux);
    case key_type::v3v3_aux_milliamps:
      return query::v3v3_aux_milliamps::result_type(info.cur_3v3_aux);
    case key_type::ddr_vpp_bottom_millivolts:
      return query::ddr_vpp_bottom_millivolts::result_type(info.ddr_vpp_btm);
    case key_type::ddr_vpp_top_millivolts:
      return query::ddr_vpp_top_millivolts::result_type(info.ddr_vpp_top);
    case key_type::v5v5_system_millivolts:
      return query::v5v5_system_millivolts::result_type(info.sys_5v5);
    case key_type::v1v2_vcc_top_millivolts:
      return query::v1v2_vcc_top_millivolts::result_type(info.top_1v2);
    case key_type::v1v2_vcc_bottom_millivolts:
      return query::v1v2_vcc_bottom_millivolts::result_type(info.vcc1v2_btm);
    case key_type::v1v8_millivolts:
      return query::v1v8_millivolts::result_type(info.vol_1v8);
    case key_type::v0v85_millivolts:
      return query::v0v85_millivolts::result_type(info.vol_0v85);
    case key_type::v0v9_vcc_millivolts:
      return query::v0v9_vcc_millivolts::result_type(info.mgt0v9avcc);
    case key_type::v12v_sw_millivolts:
      return query::v12v_sw_millivolts::result_type(info.vol_12v_sw);
    case key_type::mgt_vtt_millivolts:
      return query::mgt_vtt_millivolts::result_type(info.mgtavtt);
    case key_type::int_vcc_millivolts:
      return query::int_vcc_millivolts::result_type(info.vccint_vol);
    case key_type::int_vcc_milliamps:
      return query::int_vcc_milliamps::result_type(info.vccint_curr);
    case key_type::v3v3_pex_milliamps:
      return query::v3v3_pex_milliamps::result_type(info.cur_3v3_pex);
    case key_type::int_vcc_io_milliamps:
      return query::int_vcc_io_milliamps::result_type(info.cur_0v85);
    case key_type::v3v3_vcc_millivolts:
      return query::v3v3_vcc_millivolts::result_type(info.vol_3v3_vcc);
    case key_type::hbm_1v2_millivolts:
      return query::hbm_1v2_millivolts::result_type(info.vol_1v2_hbm);
    case key_type::v2v5_vpp_millivolts:
      return query::v2v5_vpp_millivolts::result_type(info.vol_2v5_vpp);
    case key_type::int_vcc_io_millivolts:
      return query::int_vcc_io_millivolts::result_type(info.vccint_bram);
    case key_type::temp_card_top_front:
      return query::temp_card_top_front::result_type(info.se98_temp0);
    case key_type::temp_card_top_rear:
      return query::temp_card_top_rear::result_type(info.se98_temp1);
    case key_type::temp_card_bottom_front:
      return query::temp_card_bottom_front::result_type(info.se98_temp2);
    case key_type::temp_fpga:
      return query::temp_fpga::result_type(info.fpga_temp);
    case key_type::fan_trigger_critical_temp:
      return query::fan_trigger_critical_temp::result_type(info.fan_temp);
    case key_type::fan_speed_rpm:
      return query::fan_speed_rpm::result_type(info.fan_rpm);
    case key_type::ddr_temp_0:
      return query::ddr_temp_0::result_type(info.dimm_temp0);
    case key_type::ddr_temp_1:
      return query::ddr_temp_1::result_type(info.dimm_temp1);
    case key_type::ddr_temp_2:
      return query::ddr_temp_2::result_type(info.dimm_temp2);
    case key_type::ddr_temp_3:
      return query::ddr_temp_3::result_type(info.dimm_temp3);
    case key_type::hbm_temp:
      return query::hbm_temp::result_type(info.hbm_temp0);
    case key_type::cage_temp_0:
      return query::cage_temp_0::result_type(info.cage_temp0);
    case key_type::cage_temp_1:
      return query::cage_temp_1::result_type(info.cage_temp1);
    case key_type::cage_temp_2:
      return query::cage_temp_2::result_type(info.cage_temp2);
    case key_type::cage_temp_3:
      return query::cage_temp_3::result_type(info.cage_temp3);
    case key_type::xmc_version:
      return std::to_string(info.version);
    case key_type::power_microwatts:
      val = static_cast<uint64_t>(info.vol_12v_pex) * static_cast<uint64_t>(info.cur_12v_pex) +
          static_cast<uint64_t>(info.vol_12v_aux) * static_cast<uint64_t>(info.cur_12v_aux) +
          static_cast<uint64_t>(info.vol_3v3_pex) * static_cast<uint64_t>(info.cur_3v3_pex);
      return val;
    case key_type::power_warning:
      return query::power_warning::result_type(info.power_warn);
    case key_type::v12_aux1_millivolts:
      return query::v12_aux1_millivolts::result_type(info.vol_12v_aux1);
    case key_type::vcc1v2_i_milliamps:
      return query::vcc1v2_i_milliamps::result_type(info.vol_vcc1v2_i);
    case key_type::v12_in_i_milliamps:
      return query::v12_in_i_milliamps::result_type(info.vol_v12_in_i);
    case key_type::v12_in_aux0_i_milliamps:
      return query::v12_in_aux0_i_milliamps::result_type(info.vol_v12_in_aux0_i);
    case key_type::v12_in_aux1_i_milliamps:
      return query::v12_in_aux1_i_milliamps::result_type(info.vol_v12_in_aux1_i);
    case key_type::vcc_aux_millivolts:
      return query::vcc_aux_millivolts::result_type(info.vol_vccaux);
    case key_type::vcc_aux_pmc_millivolts:
      return query::vcc_aux_pmc_millivolts::result_type(info.vol_vccaux_pmc);
    case key_type::vcc_ram_millivolts:
      return query::vcc_ram_millivolts::result_type(info.vol_vccram);
    case key_type::v0v9_int_vcc_vcu_millivolts:
      return query::v0v9_int_vcc_vcu_millivolts::result_type(info.vccint_vcu_0v9);
    case key_type::int_vcc_temp:
      return query::int_vcc_temp::result_type(info.vccint_temp);
    case key_type::heartbeat_count:
      return query::heartbeat_count::result_type(info.heartbeat_count);
    case key_type::heartbeat_err_code:
      return query::heartbeat_err_code::result_type(info.heartbeat_err_code);
    case key_type::heartbeat_err_time:
      return query::heartbeat_err_time::result_type(info.heartbeat_err_time);
    case key_type::heartbeat_stall:
      return query::heartbeat_stall::result_type(info.heartbeat_stall);
    default:
      throw unexpected_query_request_key(key);
    }
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get_info(device,key);
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct icap
{
  using result_type = boost::any;

  static xcl_pr_region
  init_icap_info(const xrt_core::device* dev)
  {
    xcl_pr_region info = { 0 };
    userpf::get_icap_info(dev->get_user_handle(), &info);
    return info;
  };

  static result_type
  get_info(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xcl_pr_region> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_icap_info(device));
      it = ret.first;
    }

    const xcl_pr_region& info = (*it).second;

    switch (key) {
    case key_type::clock_freqs_mhz:
      return query::clock_freqs_mhz::result_type {
        std::to_string(info.freq_0),
        std::to_string(info.freq_1),
        std::to_string(info.freq_2),
        std::to_string(info.freq_3)
      };
    case key_type::idcode:
      return query::idcode::result_type(info.idcode);
    case key_type::status_mig_calibrated:
      return query::status_mig_calibrated::result_type(info.mig_calib);
    case key_type::xclbin_uuid: {
      char uuid_str[64] = { 0 };
      uuid_unparse_lower(info.uuid, uuid_str);
      return query::xclbin_uuid::result_type(uuid_str);
    }
    default:
      throw unexpected_query_request_key(key);
    }
    // No query for freq_cntr_0, freq_cntr_1, freq_cntr_2, freq_cntr_3 and uuid
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get_info(device,key);
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct xclbin
{
  using result_type = std::vector<char>;

  static result_type
  user(const xrt_core::device* dev, key_type key)
  {
    auto uhdl = dev->get_user_handle();
    if (!uhdl)
      throw xrt_core::internal_error("xclbin query request, missing user device handle");

    if (key == key_type::mem_topology_raw) {
      size_t size_ret = 0;
      userpf::get_mem_topology(uhdl, nullptr, 0, &size_ret);
      std::vector<char> data(size_ret);
      userpf::get_mem_topology(uhdl, data.data(), size_ret, nullptr);
      return data;
    }

    if (key == key_type::ip_layout_raw) {
      size_t size_ret = 0;
      userpf::get_ip_layout(uhdl, nullptr, 0, &size_ret);
      std::vector<char> data(size_ret);
      userpf::get_ip_layout(uhdl, data.data(), size_ret, nullptr);
      return data;
    }

    if (key == key_type::debug_ip_layout_raw) {
      size_t size_ret = 0;
      userpf::get_debug_ip_layout(uhdl, nullptr, 0, &size_ret);
      std::vector<char> data(size_ret);
      userpf::debug_get_ip_layout(uhdl, data.data(), size_ret, nullptr);
      return data;
    }

    if (key == key_type::temp_by_mem_topology) {
      size_t size_ret = 0;
      userpf::get_temp_by_mem_topology(uhdl, nullptr, 0, &size_ret);
      std::vector<char> data(size_ret);
      userpf::get_temp_by_mem_topology(uhdl, data.data(), size_ret, nullptr);
      return data;
    }
    throw unexpected_query_request_key(key);
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct group_topology
{
  using result_type = std::vector<char>;

  static result_type
  user(const xrt_core::device* dev, key_type key)
  {
    auto uhdl = dev->get_user_handle();
    if (!uhdl)
      throw xrt_core::internal_error("group_topology query request, missing user device handle");

    size_t size_ret = 0;
    userpf::get_group_mem_topology(uhdl, nullptr, 0, &size_ret);
    std::vector<char> gdata(size_ret);
    userpf::get_group_mem_topology(uhdl, gdata.data(), size_ret, nullptr);
    return gdata;
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct memstat
{
  using result_type = std::vector<char>;

  static result_type
  user(const xrt_core::device* dev, key_type key)
  {
    auto uhdl = dev->get_user_handle();
    if (!uhdl)
      throw xrt_core::internal_error("memstat query request, missing user device handle");

    size_t size_ret = 0;
    bool raw = false;
    userpf::get_group_mem_topology(uhdl, nullptr, 0, &size_ret);
    std::vector<char> gdata(size_ret);
    userpf::get_group_mem_topology(uhdl, gdata.data(), size_ret, nullptr);
    if (key == key_type::memstat) {
      userpf::get_memstat(uhdl, nullptr, 0, &size_ret, raw);
      std::vector<char> data(size_ret);
      userpf::get_memstat(uhdl, data.data(), size_ret, nullptr, raw);
      return data;
    }
    throw unexpected_query_request_key(key);
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct memstat_raw
{
  using result_type = std::vector<std::string>;

  static result_type
  user(const xrt_core::device* dev, key_type key)
  {
    auto uhdl = dev->get_user_handle();
    if (!uhdl)
      throw xrt_core::internal_error("memstat query request, missing user device handle");

    size_t size_ret = 0;
    bool raw = true;
    userpf::get_group_mem_topology(uhdl, nullptr, 0, &size_ret);
    std::vector<char> gdata(size_ret);
    userpf::get_group_mem_topology(uhdl, gdata.data(), size_ret, nullptr);

    if (key == key_type::memstat_raw) {
      userpf::get_memstat(uhdl, nullptr, 0, &size_ret, raw);
	  auto op_size = size_ret * sizeof(struct drm_xocl_mm_stat);
      std::vector<char> data(op_size);
      userpf::get_memstat(uhdl, data.data(), op_size, nullptr, raw);
      auto mm_stat = reinterpret_cast<struct drm_xocl_mm_stat*>(data.data());
      std::vector<std::string> output;
      for (int i = 0; i < size_ret; i++) {
        output.push_back(boost::str(boost::format("%u %u\n") % mm_stat->memory_usage % mm_stat->bo_count));
        mm_stat++;
      }
      return output;
    }

    throw unexpected_query_request_key(key);
  }

  static result_type
  mgmt(const xrt_core::device*, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct bdf
{
  using result_type = query::pcie_bdf::result_type;

  struct bdf_type {
    uint16_t domain = 0;
    uint16_t bus = 0;
    uint16_t device = 0;
    uint16_t function = 0;
  };

  static void
  init_bdf(const xrt_core::device* dev, bdf_type* bdf)
  {
    if (auto mhdl = dev->get_mgmt_handle())
      mgmtpf::get_bdf_info(mhdl, reinterpret_cast<uint16_t*>(bdf));
    else if (auto uhdl = dev->get_user_handle())
      userpf::get_bdf_info(uhdl, reinterpret_cast<uint16_t*>(bdf));
    else
      throw xrt_core::internal_error("bdf::init_bdf - No device handle");
  }

  static result_type
  get_bdf(const xrt_core::device* device)
  {
    static std::map<const xrt_core::device*, bdf_type> bdfmap;
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
    return std::make_tuple(bdf.domain, bdf.bus, bdf.device, bdf.function);
  }

  static result_type
  user(const xrt_core::device* device, key_type)
  {
    return get_bdf(device);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type)
  {
    return get_bdf(device);
  }
};

struct info
{
  using result_type = boost::any;

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    auto init_device_info = [](const xrt_core::device* dev) {
      XOCL_DEVICE_INFORMATION info = { 0 };
      userpf::get_device_info(dev->get_user_handle(), &info);
      return info;
    };

    static std::map<const xrt_core::device*, XOCL_DEVICE_INFORMATION> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_device_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;

    switch (key) {
    case key_type::pcie_vendor:
      return static_cast<query::pcie_vendor::result_type>(info.Vendor);
    case key_type::pcie_device:
      return static_cast<query::pcie_device::result_type>(info.Device);
    case key_type::pcie_subsystem_vendor:
      return static_cast<query::pcie_subsystem_vendor::result_type>(info.SubsystemVendor);
    case key_type::pcie_subsystem_id:
      return static_cast<query::pcie_subsystem_id::result_type>(info.SubsystemDevice);
    case key_type::pcie_link_speed_max:
      return static_cast<query::pcie_link_speed_max::result_type>(info.MaximumLinkSpeed);
    case key_type::pcie_link_speed:
      return static_cast<query::pcie_link_speed::result_type>(info.LinkSpeed);
    case key_type::pcie_express_lane_width_max:
      return static_cast<query::pcie_express_lane_width_max::result_type>(info.MaximumLinkWidth);
    case key_type::pcie_express_lane_width:
      return static_cast<query::pcie_express_lane_width::result_type>(info.LinkWidth);
    default:
      throw unexpected_query_request_key(key);
    }
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    auto init_pcie_info = [](const xrt_core::device* dev) {
      XCLMGMT_IOC_DEVICE_PCI_INFO info = { 0 };
      mgmtpf::get_pcie_info(dev->get_mgmt_handle(), &info);
      return info;
    };

    static std::map<const xrt_core::device*, XCLMGMT_IOC_DEVICE_PCI_INFO> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_pcie_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;

    switch (key) {
    case key_type::pcie_vendor:
      return static_cast<query::pcie_vendor::result_type>(info.pcie_info.vendor);
    case key_type::pcie_device:
      return static_cast<query::pcie_device::result_type>(info.pcie_info.device);
    case key_type::pcie_subsystem_vendor:
      return static_cast<query::pcie_subsystem_vendor::result_type>(info.pcie_info.subsystem_vendor);
    case key_type::pcie_subsystem_id:
      return static_cast<query::pcie_subsystem_id::result_type>(info.pcie_info.subsystem_device);
    default:
      throw unexpected_query_request_key(key);
    }
  }
};

struct xmc
{
  using result_type = boost::any;

  static result_type
  user(const xrt_core::device*, key_type key)
  {
    throw userpf_not_supported_error(key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    auto init_device_info = [](const xrt_core::device* dev) {
      XCLMGMT_IOC_DEVICE_INFO info = { 0 };
      mgmtpf::get_device_info(dev->get_mgmt_handle(), &info);
      return info;
    };

    static std::map<const xrt_core::device*, XCLMGMT_IOC_DEVICE_INFO> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_device_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;

    switch (key) {
    case key_type::xmc_reg_base:
      return info.xmc_offset;
    case key_type::xmc_status:
      return query::xmc_status::result_type(1); //hardcoded
    case key_type::xmc_qspi_status:
      return std::pair<std::string, std::string>("N/A", "N/A");
    default:
      throw unexpected_query_request_key(key);
    }
  }
};

struct devinfo
{
  using result_type = boost::any;

  static result_type
  user(const xrt_core::device*, key_type key)
  {
    throw userpf_not_supported_error(key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    auto init_device_info = [](const xrt_core::device* dev) {
      XCLMGMT_DEVICE_INFO info = { 0 };
      mgmtpf::get_dev_info(dev->get_mgmt_handle(), &info);
      return info;
    };

    static std::map<const xrt_core::device*, XCLMGMT_DEVICE_INFO> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_device_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;
    switch (key) {
    case key_type::board_name:
      return static_cast<query::board_name::result_type>(info.ShellName);
    case key_type::is_mfg:
    {
      auto shell = static_cast<query::board_name::result_type>(info.ShellName);
      boost::to_upper(shell);
      return (shell.find("GOLDEN") != std::string::npos) ? true : false;
    }
    case key_type::xmc_sc_presence:
    {
      //xmc is not present in golden image
      //inverse logic of is_mfg
      auto shell = static_cast<query::board_name::result_type>(info.ShellName);
      boost::to_upper(shell);
      //sample strings: xilinx_u250_GOLDEN, xilinx_u250_gen3x16_base, xilinx_u250_xdma_201830_3
      return (shell.find("GOLDEN") != std::string::npos) ? false : true;
    }
    default:
      throw unexpected_query_request_key(key);
    }
  }
};

struct recovery
{
  using result_type = bool;
  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return false;
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    return false;
  }
};

struct uuid
{
  using result_type = boost::any;

  static result_type
  user(const xrt_core::device*, key_type key)
  {
    throw userpf_not_supported_error(key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    auto init_device_info = [](const xrt_core::device* dev) {
      XCLMGMT_IOC_UUID_INFO info = { 0 };
      mgmtpf::get_uuids(dev->get_mgmt_handle(), &info);
      return info;
    };

    static std::map<const xrt_core::device*, XCLMGMT_IOC_UUID_INFO> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_device_info(device));
      it = ret.first;
    }

    auto& info = (*it).second;
    switch (key) {
    case key_type::interface_uuids:
      return std::vector<std::string>{ std::string(info.blp_interface_uuid), std::string(info.plp_interface_uuid) };
    case key_type::logic_uuids:
      return std::vector<std::string>{ std::string(info.blp_logic_uuid), std::string(info.plp_logic_uuid) };
    default:
      throw unexpected_query_request_key(key);
    }
  }
};

struct flash_bar_offset
{
  using result_type = uint64_t;

  static result_type
  user(const xrt_core::device* device, key_type)
  {
    return 0;
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type)
  {
    auto init_addr = [](const xrt_core::device* dev) {
      uint64_t addr;
      mgmtpf::get_flash_addr(dev->get_mgmt_handle(), addr);
      return addr;
    };

    static std::map<const xrt_core::device*, uint64_t> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_addr(device));
      it = ret.first;
    }

    return (*it).second;
  }
};

struct rom
{
  using result_type = boost::any;

  static FeatureRomHeader
  init_feature_rom_header(const xrt_core::device* dev)
  {
    FeatureRomHeader hdr = {0};
    if (auto mhdl = dev->get_mgmt_handle())
      mgmtpf::get_rom_info(mhdl, &hdr);
    else if (auto uhdl = dev->get_user_handle())
      userpf::get_rom_info(uhdl, &hdr);
    else
      throw xrt_core::internal_error("No device handle");
    return hdr;
  }

  static result_type
  get_info(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, FeatureRomHeader> hdrmap;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = hdrmap.find(device);
    if (it == hdrmap.end()) {
      auto ret = hdrmap.emplace(device,init_feature_rom_header(device));
      it = ret.first;
    }

    auto& hdr= (*it).second;

    switch (key) {
    case key_type::rom_vbnv:
      return std::string(reinterpret_cast<const char*>(hdr.VBNVName));
    case key_type::rom_ddr_bank_size_gb:
      return static_cast<query::rom_ddr_bank_size_gb::result_type>(hdr.DDRChannelSize);
    case key_type::rom_ddr_bank_count_max:
      return static_cast<query::rom_ddr_bank_count_max::result_type>(hdr.DDRChannelCount);
    case key_type::rom_fpga_name:
      return std::string(reinterpret_cast<const char*>(hdr.FPGAPartName));
    case key_type::rom_time_since_epoch:
      return static_cast<query::rom_time_since_epoch::result_type>(hdr.TimeSinceEpoch);
    }

    if (device->get_user_handle())
      throw unexpected_query_request_key(key);

    switch (key) {
    case key_type::rom_uuid:
      return std::string(reinterpret_cast<const char*>(hdr.uuid),16);
    default:
      throw unexpected_query_request_key(key);
    }
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get_info(device,key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    return get_info(device,key);
  }
};

struct kds_cu_info
{
  using result_type = query::kds_cu_info::result_type;

  static XOCL_KDS_CU_INFORMATION*
    init_kds_custat(const xrt_core::device* dev)
  {
    if (auto uhdl = dev->get_user_handle()) {
      int cu_count; DWORD output_sz;
      userpf::get_kds_custat(uhdl, nullptr, 0, &cu_count);
      output_sz = sizeof(XOCL_KDS_CU_INFORMATION) + cu_count * sizeof(XOCL_KDS_CU);
      std::vector<char> hdr(output_sz);
      userpf::get_kds_custat(uhdl, hdr.data(), output_sz, nullptr);
      return reinterpret_cast<XOCL_KDS_CU_INFORMATION*>(hdr.data());
    }
    else
      throw std::runtime_error("No userpf device handle");
  }

  static result_type
  get_info(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, XOCL_KDS_CU_INFORMATION*> hdrmap;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = hdrmap.find(device);
    if (it == hdrmap.end())
      it = hdrmap.emplace(device, init_kds_custat(device)).first;

    auto& stats = (*it).second;
    result_type cuStats;
    for (unsigned int i = 0; i < stats->CuCount; i++)
      cuStats.push_back(std::make_tuple(stats->CuInfo[i].BaseAddress, stats->CuInfo[i].Usage, 0));
    return cuStats;
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get_info(device, key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    throw mgmtpf_not_supported_error(key);
  }
};

struct data_retention
{
  using result_type = uint32_t;
  using value_type = uint32_t;

  static result_type
  user_get(const xrt_core::device* device)
  {
	  return 0;
  }

  static result_type
  mgmt_get(const xrt_core::device* device)
  {
    auto init_ret = [](const xrt_core::device* dev) {
      uint32_t ret;
      mgmtpf::get_data_retention(dev->get_mgmt_handle(), &ret);
      return ret;
    };

    static std::map<const xrt_core::device*, uint32_t> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_ret(device));
      it = ret.first;
    }

    return (*it).second;
  }

  static void
  user_put(const xrt_core::device* device, value_type)
  {
    // data retention can't be set on user side, hence doesn't have driver support
    throw xrt_core::query::not_supported("device data retention query is not implemented on user windows");
  }

  static void
  mgmt_put(const xrt_core::device* device, value_type val)
  {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    mgmtpf::set_data_retention(device->get_mgmt_handle(), val);
  }

};

struct mailbox
{
  using result_type = boost::any;

  static xcl_mailbox
  init_mailbox_info(const xrt_core::device* dev)
  {
    xcl_mailbox info = { 0 };
    userpf::get_mailbox_info(dev->get_user_handle(), &info);
    return info;
  }

  static result_type
  get_info(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xcl_mailbox> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);

    auto it = info_map.find(device);
    if (it == info_map.end())
      it = info_map.emplace(device, init_mailbox_info(device)).first;

    const xcl_mailbox& info = (*it).second;
    switch (key) {
    case key_type::mailbox_metrics:
    {
      std::vector<std::string> vec;
      vec.push_back(boost::str(boost::format("raw bytes received: %d\n") % info.mbx_recv_raw_bytes));
      for (int i = 0; i < XCL_MAILBOX_REQ_MAX; i++)
         vec.push_back(boost::str(boost::format("req[%d] received: %d\n") % i % info.mbx_recv_req[i]));
      return vec;
    }
    default:
      throw unexpected_query_request_key(key);
    }
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get_info(device, key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    auto init_mailbox_info = [](const xrt_core::device* dev) {
      XCLMGMT_IOC_MAILBOX_RECV_INFO info = { 0 };
      mgmtpf::get_mailbox_info(dev->get_mgmt_handle(), &info);
      return info;
    };

    static std::map<const xrt_core::device*, XCLMGMT_IOC_MAILBOX_RECV_INFO> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end())
      it = info_map.emplace(device, init_mailbox_info(device)).first;

    auto& info = (*it).second;
    switch (key) {
    case key_type::mailbox_metrics:
    {
      std::vector<std::string> vec;
      vec.push_back((boost::format("raw bytes received: %d\n") % info.mbx_recv_raw_bytes).str());
      for (int i = 0; i < XCL_MAILBOX_REQ_MAX; i++)
        vec.push_back(boost::str(boost::format("req[%d] received: %d\n") % i % info.mbx_recv_req[i]));
      return std::vector<std::string>(vec);
    }
    default:
      throw unexpected_query_request_key(key);
    }
  }
}; //end of struct mailbox


struct aim_counter
{
  using result_type = query::aim_counter::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& arg1)
  {
    const auto dbgIpData = boost::any_cast<query::aim_counter::debug_ip_data_type>(arg1);

    return xrt_core::debug_ip::getAIMCounterResult(device, dbgIpData);
  }
};


template <typename QueryRequestType, typename Getter>
struct function0_getput : QueryRequestType
{
  static_assert(std::is_same<Getter::result_type, QueryRequestType::result_type>::value
    || std::is_same<Getter::result_type, boost::any>::value, "get type mismatch");
  static_assert(std::is_same<Getter::value_type, QueryRequestType::result_type>::value
    || std::is_same<Getter::value_type, boost::any>::value, "value type mismatch");

  boost::any
  get(const xrt_core::device* device) const
  {
    if (device->get_mgmt_handle())
      return Getter::mgmt_get(device);
    if (device->get_user_handle())
      return Getter::user_get(device);
    throw xrt_core::internal_error("No device handle");
  }

  void
  put(const xrt_core::device* device, const boost::any& any) const
  {
    auto val = boost::any_cast<typename QueryRequestType::value_type>(any);
    if (device->get_mgmt_handle())
      Getter::mgmt_put(device, val);
    else if (device->get_user_handle())
      Getter::user_put(device, val);
    else
      throw xrt_core::internal_error("No device handle");
  }
};

template <typename QueryRequestType, typename Getter>
struct function0_getter : QueryRequestType
{
  static_assert(std::is_same<Getter::result_type, QueryRequestType::result_type>::value
             || std::is_same<Getter::result_type, boost::any>::value, "type mismatch");

  boost::any
  get(const xrt_core::device* device) const
  {
    auto k = QueryRequestType::key;
    if (auto mhdl = device->get_mgmt_handle())
      return Getter::mgmt(device,k);
    else if (auto uhdl = device->get_user_handle())
      return Getter::user(device,k);
    else
      throw xrt_core::internal_error("No device handle");
  }
};

template <typename QueryRequestType, typename Getter>
struct function1_getter : QueryRequestType
{
  static_assert(std::is_same<Getter::result_type, QueryRequestType::result_type>::value
             || std::is_same<Getter::result_type, boost::any>::value, "type mismatch");

  boost::any
  get(const xrt_core::device* device, const boost::any& any) const
  {
    auto k = QueryRequestType::key;
    if (auto mhdl = device->get_mgmt_handle())
      return Getter::mgmt(device,k,any);
    else if (auto uhdl = device->get_user_handle())
      return Getter::user(device,k,any);
    else
      throw xrt_core::internal_error("No device handle");
  }
};

template <typename QueryRequestType, typename Getter>
struct function2_getter : QueryRequestType
{
  static_assert(std::is_same<Getter::result_type, QueryRequestType::result_type>::value
             || std::is_same<Getter::result_type, boost::any>::value, "type mismatch");

  boost::any
  get(const xrt_core::device* device, query::request::modifier m, const std::string& v) const
  {
    auto k = QueryRequestType::key;
    if (auto mhdl = device->get_mgmt_handle())
      return Getter::mgmt(device,k,m,v);
    else if (auto uhdl = device->get_user_handle())
      return Getter::user(device,k,m,v);
    else
      throw xrt_core::internal_error("No device handle");
  }
};


template <typename QueryRequestType, typename Getter>
struct function4_get : virtual QueryRequestType
{
  boost::any
  get(const xrt_core::device* device, const boost::any& arg1) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k, arg1);
  }
};


static std::map<xrt_core::query::key_type, std::unique_ptr<xrt_core::query::request>> query_tbl;

template <typename QueryRequestType, typename Getter>
static void
emplace_function0_getter()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_getter<QueryRequestType, Getter>>());
}

template <typename QueryRequestType, typename Getter>
static void
emplace_function1_getter()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function1_getter<QueryRequestType, Getter>>());
}

template <typename QueryRequestType, typename Getter>
static void
emplace_function2_getter()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function2_getter<QueryRequestType, Getter>>());
}

template <typename QueryRequestType, typename Getter>
static void
emplace_function0_getput()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_getput<QueryRequestType, Getter>>());
}


template <typename QueryRequestType, typename Getter>
static void
emplace_func4_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function4_get<QueryRequestType, Getter>>());
}

static void
initialize_query_table()
{
  emplace_function0_getter<query::pcie_vendor,               info>();
  emplace_function0_getter<query::pcie_device,               info>();
  emplace_function0_getter<query::pcie_subsystem_vendor,     info>();
  emplace_function0_getter<query::pcie_subsystem_id,         info>();
  emplace_function0_getter<query::pcie_link_speed_max,       info>();
  emplace_function0_getter<query::pcie_link_speed,           info>();
  emplace_function0_getter<query::pcie_express_lane_width_max, info>();
  emplace_function0_getter<query::pcie_express_lane_width,   info>();
  emplace_function0_getter<query::interface_uuids,           uuid>();
  emplace_function0_getter<query::logic_uuids,               uuid>();
  emplace_function0_getter<query::xmc_reg_base,              xmc>();
  emplace_function0_getter<query::pcie_bdf,                  bdf>();
  emplace_function0_getter<query::rom_vbnv,                  rom>();
  emplace_function0_getter<query::rom_ddr_bank_size_gb,      rom>();
  emplace_function0_getter<query::rom_ddr_bank_count_max,    rom>();
  emplace_function0_getter<query::rom_fpga_name,             rom>();
  //emplace_function0_getter<query::rom_raw,                 rom>();
  emplace_function0_getter<query::rom_uuid,                  rom>();
  emplace_function0_getter<query::rom_time_since_epoch,      rom>();
  emplace_function0_getter<query::mem_topology_raw,          xclbin>();
  emplace_function0_getter<query::ip_layout_raw,             xclbin>();
  emplace_function0_getter<query::debug_ip_layout_raw,       xclbin>();
  emplace_function0_getter<query::temp_by_mem_topology,      xclbin>();
  emplace_function0_getter<query::clock_freqs_mhz,           icap>();
  emplace_function0_getter<query::idcode,                    icap>();
  emplace_function0_getter<query::status_mig_calibrated,     icap>();
  emplace_function0_getter<query::xclbin_uuid,               icap>();
  emplace_function0_getter<query::v12v_pex_millivolts,       sensor>();
  emplace_function0_getter<query::v12v_aux_millivolts,       sensor>();
  emplace_function0_getter<query::v12v_pex_milliamps,        sensor>();
  emplace_function0_getter<query::v12v_aux_milliamps,        sensor>();
  emplace_function0_getter<query::v3v3_pex_millivolts,       sensor>();
  emplace_function0_getter<query::v3v3_aux_millivolts,       sensor>();
  emplace_function0_getter<query::v3v3_aux_milliamps,        sensor>();
  emplace_function0_getter<query::ddr_vpp_bottom_millivolts, sensor>();
  emplace_function0_getter<query::ddr_vpp_top_millivolts,    sensor>();
  emplace_function0_getter<query::v5v5_system_millivolts,    sensor>();
  emplace_function0_getter<query::v1v2_vcc_top_millivolts,   sensor>();
  emplace_function0_getter<query::v1v2_vcc_bottom_millivolts,sensor>();
  emplace_function0_getter<query::v1v8_millivolts,           sensor>();
  emplace_function0_getter<query::v0v85_millivolts,          sensor>();
  emplace_function0_getter<query::v0v9_vcc_millivolts,       sensor>();
  emplace_function0_getter<query::v12v_sw_millivolts,        sensor>();
  emplace_function0_getter<query::mgt_vtt_millivolts,        sensor>();
  emplace_function0_getter<query::int_vcc_millivolts,        sensor>();
  emplace_function0_getter<query::int_vcc_milliamps,         sensor>();
  emplace_function0_getter<query::v3v3_pex_milliamps,        sensor>();
  emplace_function0_getter<query::int_vcc_io_milliamps,      sensor>();
  emplace_function0_getter<query::v3v3_vcc_millivolts,       sensor>();
  emplace_function0_getter<query::hbm_1v2_millivolts,        sensor>();
  emplace_function0_getter<query::v2v5_vpp_millivolts,       sensor>();
  emplace_function0_getter<query::int_vcc_io_millivolts,     sensor>();
  emplace_function0_getter<query::temp_card_top_front,       sensor>();
  emplace_function0_getter<query::temp_card_top_rear,        sensor>();
  emplace_function0_getter<query::temp_card_bottom_front,    sensor>();
  emplace_function0_getter<query::temp_fpga,                 sensor>();
  emplace_function0_getter<query::fan_trigger_critical_temp, sensor>();
  emplace_function0_getter<query::fan_speed_rpm,             sensor>();
  emplace_function0_getter<query::ddr_temp_0,                sensor>();
  emplace_function0_getter<query::ddr_temp_1,                sensor>();
  emplace_function0_getter<query::ddr_temp_2,                sensor>();
  emplace_function0_getter<query::ddr_temp_3,                sensor>();
  emplace_function0_getter<query::hbm_temp,                  sensor>();
  emplace_function0_getter<query::cage_temp_0,               sensor>();
  emplace_function0_getter<query::cage_temp_1,               sensor>();
  emplace_function0_getter<query::cage_temp_2,               sensor>();
  emplace_function0_getter<query::cage_temp_3,               sensor>();
  emplace_function0_getter<query::xmc_version,               sensor>();
  emplace_function0_getter<query::power_microwatts,          sensor>();
  emplace_function0_getter<query::power_warning,             sensor>();
  emplace_function0_getter<query::v12_aux1_millivolts,       sensor>();
  emplace_function0_getter<query::vcc1v2_i_milliamps,        sensor>();
  emplace_function0_getter<query::v12_in_i_milliamps,        sensor>();
  emplace_function0_getter<query::v12_in_aux0_i_milliamps,   sensor>();
  emplace_function0_getter<query::v12_in_aux1_i_milliamps,   sensor>();
  emplace_function0_getter<query::vcc_aux_millivolts,        sensor>();
  emplace_function0_getter<query::int_vcc_temp,              sensor>();
  emplace_function0_getter<query::vcc_aux_pmc_millivolts,    sensor>();
  emplace_function0_getter<query::vcc_ram_millivolts,        sensor>();
  emplace_function0_getter<query::v0v9_int_vcc_vcu_millivolts, sensor>();
  emplace_function0_getter<query::heartbeat_count,           sensor>();
  emplace_function0_getter<query::heartbeat_err_time,        sensor>();
  emplace_function0_getter<query::heartbeat_err_code,        sensor>();
  emplace_function0_getter<query::heartbeat_stall,           sensor>();
  emplace_function0_getter<query::xmc_status,                xmc>();
  emplace_function0_getter<query::xmc_qspi_status,           xmc>();
  emplace_function0_getter<query::xmc_serial_num,            board>();
  emplace_function0_getter<query::max_power_level,           board>();
  emplace_function0_getter<query::xmc_sc_version,            board>();
  emplace_function0_getter<query::fan_fan_presence,          board>();
  emplace_function0_getter<query::xmc_board_name,            board>();
  emplace_function0_getter<query::mac_addr_first,            board>();
  emplace_function0_getter<query::mac_contiguous_num,        board>();
  emplace_function0_getter<query::mac_addr_list,             board>();
  emplace_function2_getter<query::mig_ecc_enabled,           mig>();
  emplace_function2_getter<query::mig_ecc_status,            mig>();
  emplace_function2_getter<query::mig_ecc_ce_cnt,            mig>();
  emplace_function2_getter<query::mig_ecc_ue_cnt,            mig>();
  emplace_function2_getter<query::mig_ecc_ce_ffa,            mig>();
  emplace_function2_getter<query::mig_ecc_ue_ffa,            mig>();
  emplace_function0_getter<query::firewall_detect_level,     firewall>();
  emplace_function0_getter<query::firewall_status,           firewall>();
  emplace_function0_getter<query::firewall_time_sec,         firewall>();
  emplace_function0_getter<query::f_flash_type,              flash>();
  emplace_function0_getter<query::flash_type,                flash>();
  emplace_function0_getter<query::is_mfg,                    devinfo>();
  emplace_function0_getter<query::is_ready,                  ready>();
  emplace_function0_getter<query::board_name,                devinfo>();
  emplace_function0_getter<query::flash_bar_offset,          flash_bar_offset>();
  emplace_function0_getter<query::xmc_sc_presence,           devinfo>();
  emplace_function0_getput<query::data_retention,            data_retention>();
  emplace_function0_getter<query::is_recovery,               recovery>();
  emplace_function0_getter<query::mailbox_metrics,           mailbox>();
  emplace_function0_getter<query::kds_cu_info,               kds_cu_info>();
  emplace_function0_getter<query::memstat_raw,               memstat_raw>();
  emplace_function0_getter<query::memstat,                   memstat>();
  emplace_function0_getter<query::group_topology,            group_topology>();

  emplace_func4_request<query::aim_counter,                  aim_counter>();
}

struct X { X() { initialize_query_table(); }};
static X x;

}

namespace xrt_core {

const query::request&
device_windows::
lookup_query(query::key_type query_key) const
{
  auto it = query_tbl.find(query_key);

  if (it == query_tbl.end())
    throw query::no_such_key(query_key);

  return *(it->second);
}

device_windows::
device_windows(handle_type device_handle, id_type device_id, bool user)
  : shim<device_pcie>(user ? device_handle : nullptr, device_id, user)
  , m_mgmthdl(user ? nullptr : device_handle)
{}

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

void
device_windows::
reset(const char*, const char*, const char*) const
{
  throw std::runtime_error("Reset is not supported on Windows.");
}

/* TODO: after 2020.1
 * Adding open/close stubs for compilation purposes.
 * We currently don't use these functions but we'll
 * need them when we switch over to using driver for
 * flashing
 */
int
device_windows::
open(const std::string& subdev, int flag) const
{
  return 0;
}

void
device_windows::
close(int dev_handle) const
{
}

void
device_windows::
xclmgmt_load_xclbin(const char* buffer) const {}

} // xrt_core
