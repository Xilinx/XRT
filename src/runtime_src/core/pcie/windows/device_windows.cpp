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
#include "core/common/query_requests.h"
#include "core/common/utils.h"
#include "core/include/xrt.h"
#include "core/include/xclfeatures.h"

#include <boost/format.hpp>
#include <type_traits>
#include <string>
#include <iostream>
#include <map>
#include <mutex>

#pragma warning(disable : 4100 4996)

namespace {

namespace query = xrt_core::query;
using key_type = xrt_core::query::key_type;
using qtype = std::underlying_type<query::key_type>::type;

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

struct mfg
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

struct board_name
{
  using result_type = std::string;

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return "TO-DO";
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    return "TO-DO";
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
      throw std::runtime_error("device_windows::firewall_info() unexpected qr "
                               + std::to_string(static_cast<qtype>(key)));
    }
    // No query for max_level, curr_status and curr_level
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get(device,key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    throw std::runtime_error("query request ("
                             + std::to_string(static_cast<qtype>(key))
                             + ") not supported for mgmtpf on windows");
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
      throw std::runtime_error("device_windows::mig_ecc_info() unexpected qr "
                               + std::to_string(static_cast<qtype>(key)));
    }
    // No query for mem_type and mem_idx
  }

  static result_type
  user(const xrt_core::device* device, key_type key, const boost::any&)
  {
    return get(device,key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key, const boost::any&)
  {
    throw std::runtime_error("query request ("
                             + std::to_string(static_cast<qtype>(key))
                             + ") not supported for mgmtpf on windows");
  }
};

struct board
{
  using result_type = boost::any;


  static xcl_board_info
  init_board_info(const xrt_core::device* dev)
  {
    xcl_board_info info = { 0 };
    userpf::get_board_info(dev->get_user_handle(), &info);
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
    case key_type::xmc_bmc_version:
      return query::xmc_bmc_version::result_type(reinterpret_cast<const char*>(info.bmc_ver));
    case key_type::xmc_max_power:
      return query::xmc_max_power::result_type(info.max_power);
    case key_type::fan_fan_presence:
      return query::fan_fan_presence::result_type(info.fan_presence);
    default:
      throw std::runtime_error("device_windows::board_info() unexpected qr "
                               + static_cast<qtype>(key));
    }
    // No query for mac_addr0, mac_addr1, mac_addr2, mac_addr3, revision, bd_name and config_mode
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get(device,key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    throw std::runtime_error("query request ("
                             + std::to_string(static_cast<qtype>(key))
                             + ") not supported for mgmtpf on windows");
  }
};

struct xmc
{
  using result_type = uint64_t;

  static result_type
  get(const xrt_core::device* dev, key_type key)
  {
    if(key == query::key_type::xmc_status)
      return query::xmc_status::result_type(1);
    throw std::runtime_error
      ("Invalid query request (" + std::to_string(static_cast<qtype>(key)) + ")");
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get(device,key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    return get(device,key);
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
    case key_type::v0v85_milliamps:
      return query::v0v85_milliamps::result_type(info.cur_0v85);
    case key_type::v3v3_vcc_millivolts:
      return query::v3v3_vcc_millivolts::result_type(info.vol_3v3_vcc);
    case key_type::hbm_1v2_millivolts:
      return query::hbm_1v2_millivolts::result_type(info.vol_1v2_hbm);
    case key_type::v2v5_vpp_millivolts:
      return query::v2v5_vpp_millivolts::result_type(info.vol_2v5_vpp);
    case key_type::int_bram_vcc_millivolts:
      return query::int_bram_vcc_millivolts::result_type(info.vccint_bram);
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
    default:
      throw std::runtime_error("device_windows::icap() unexpected qr("
                               + std::to_string(static_cast<qtype>(key))
                               + ") for userpf");
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
    throw std::runtime_error("query request ("
                             + std::to_string(static_cast<qtype>(key))
                             + ") not supported for mgmtpf on windows");
  }
};

struct icap
{
  using result_type = boost::any;

  static xcl_hwicap
  init_icap_info(const xrt_core::device* dev)
  {
    xcl_hwicap info = { 0 };
    userpf::get_icap_info(dev->get_user_handle(), &info);
    return info;
  };

  static result_type
  get_info(const xrt_core::device* device, key_type key)
  {
    static std::map<const xrt_core::device*, xcl_hwicap> info_map;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lk(mutex);
    auto it = info_map.find(device);
    if (it == info_map.end()) {
      auto ret = info_map.emplace(device,init_icap_info(device));
      it = ret.first;
    }

    const xcl_hwicap& info = (*it).second;

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
      throw std::runtime_error("device_windows::icap() unexpected qr("
                               + std::to_string(static_cast<qtype>(key))
                               + ") for userpf");
    }
    // No query for freq_cntr_0, freq_cntr_1, freq_cntr_2, freq_cntr_3 and uuid
  }

  static result_type
  user(const xrt_core::device* device, key_type key)
  {
    return get_info(device,key);
  }

  static result_type
  mgmt(const xrt_core::device* device, key_type key)
  {
    throw std::runtime_error("query request ("
                             + std::to_string(static_cast<qtype>(key))
                             + ") not supported for mgmtpf on windows");
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
      throw std::runtime_error("xclbin query request, missing user device handle");

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

    throw std::runtime_error("unexpected error");
  }

  static result_type
  mgmt(const xrt_core::device* dev, key_type key)
  {
    throw std::runtime_error("mgmt xclbin raw data queries are not implemented on windows");
  }
};

struct bdf
{
  using result_type = query::pcie_bdf::result_type;

  struct bdf_type {
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
      throw std::runtime_error("No device handle");
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
    return std::make_tuple(bdf.bus,bdf.device,bdf.function);
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
  using result_type = uint16_t;

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
      return info.Vendor;
    case key_type::pcie_device:
      return info.Device;
    case key_type::pcie_subsystem_vendor:
      return info.SubsystemVendor;
    case key_type::pcie_subsystem_id:
      return info.SubsystemDevice;
    default:
      throw std::runtime_error("device_windows::info_user() unexpected qr");
    }
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
    case key_type::pcie_vendor:
      return info.pcie_info.vendor;
    case key_type::pcie_device:
      return info.pcie_info.device;
    case key_type::pcie_subsystem_vendor:
      return info.pcie_info.subsystem_vendor;
    case key_type::pcie_subsystem_id:
      return info.pcie_info.subsystem_device;
    default:
      throw std::runtime_error("device_windows::info_mgmt() unexpected qr");
    }
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
      throw std::runtime_error("No device handle");
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
    }

    if (device->get_user_handle())
      throw std::runtime_error("device_windows::rom() unexpected qr("
                               + std::to_string(static_cast<qtype>(key))
                               + ") for userpf");

    switch (key) {
    case key_type::rom_uuid:
      return std::string(reinterpret_cast<const char*>(hdr.uuid),16);
    case key_type::rom_time_since_epoch:
      return static_cast<query::rom_time_since_epoch::result_type>(hdr.TimeSinceEpoch);
    default:
      throw std::runtime_error("device_windows::rom() unexpected qr "
                               + std::to_string(static_cast<qtype>(key))
                               + ") for mgmgpf");
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
      throw std::runtime_error("No device handle");
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
      throw std::runtime_error("No device handle");
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

static void
initialize_query_table()
{
  emplace_function0_getter<query::pcie_vendor,               info>();
  emplace_function0_getter<query::pcie_device,               info>();
  emplace_function0_getter<query::pcie_subsystem_vendor,     info>();
  emplace_function0_getter<query::pcie_subsystem_id,         info>();
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
  emplace_function0_getter<query::v0v85_milliamps,           sensor>();
  emplace_function0_getter<query::v3v3_vcc_millivolts,       sensor>();
  emplace_function0_getter<query::hbm_1v2_millivolts,        sensor>();
  emplace_function0_getter<query::v2v5_vpp_millivolts,       sensor>();
  emplace_function0_getter<query::int_bram_vcc_millivolts,   sensor>();
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
  emplace_function0_getter<query::xmc_status,                xmc>();
  emplace_function0_getter<query::xmc_serial_num,            board>();
  emplace_function0_getter<query::xmc_max_power,             board>();
  emplace_function0_getter<query::xmc_bmc_version,           board>();
  emplace_function0_getter<query::fan_fan_presence,          board>();
  emplace_function1_getter<query::mig_ecc_enabled,           mig>();
  emplace_function1_getter<query::mig_ecc_status,            mig>();
  emplace_function1_getter<query::mig_ecc_ce_cnt,            mig>();
  emplace_function1_getter<query::mig_ecc_ue_cnt,            mig>();
  emplace_function1_getter<query::mig_ecc_ce_ffa,            mig>();
  emplace_function1_getter<query::mig_ecc_ue_ffa,            mig>();
  emplace_function0_getter<query::firewall_detect_level,     firewall>();
  emplace_function0_getter<query::firewall_status,           firewall>();
  emplace_function0_getter<query::firewall_time_sec,         firewall>();
  emplace_function0_getter<query::f_flash_type,              flash>();
  emplace_function0_getter<query::flash_type,                flash>();
  emplace_function0_getter<query::is_mfg,                    mfg>();
  emplace_function0_getter<query::board_name,                board_name>();
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
device_windows(id_type device_id, bool user)
  : shim<device_pcie>(device_id, user)
{
  if (user)
    return;

  m_mgmthdl = mgmtpf::open(device_id);
}

device_windows::
device_windows(handle_type device_handle, id_type device_id)
  : shim<device_pcie>(device_handle, device_id)
{
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

} // xrt_core
