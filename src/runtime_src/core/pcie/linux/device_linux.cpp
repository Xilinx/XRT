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
#include <boost/format.hpp>

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

template <>
struct sysfs_fcn<std::string>
{
  using ValueType = std::string;

  static ValueType
  get(const pdev& dev, const char* subdev, const char* entry)
  {
    std::string err;
    ValueType value;
    dev->sysfs_get(subdev, entry, err, value);
    if (!err.empty())
      throw std::runtime_error(err);
    return value;
  }
};

template <typename VectorValueType>
struct sysfs_fcn<std::vector<VectorValueType>>
{
  //using ValueType = std::vector<std::string>;
  using ValueType = std::vector<VectorValueType>;

  static ValueType
  get(const pdev& dev, const char* subdev, const char* entry)
  {
    std::string err;
    ValueType value;
    dev->sysfs_get(subdev, entry, err, value);
    if (!err.empty())
      throw std::runtime_error(err);
    return value;
  }
};

template <typename QueryRequestType>
struct sysfs_getter : QueryRequestType
{
  const char* subdev;
  const char* entry;

  sysfs_getter(const char* s, const char* e)
    : subdev(s), entry(e)
  {}

  boost::any
  get(const xrt_core::device* device) const
  {
    return sysfs_fcn<typename QueryRequestType::result_type>
      ::get(get_pcidev(device), subdev, entry);
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
emplace_sysfs_request(const char* subdev, const char* entry)
{
  auto x = QueryRequestType::key;
  query_tbl.emplace(x, std::make_unique<sysfs_getter<QueryRequestType>>(subdev, entry));
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
  emplace_sysfs_request<query::pcie_vendor>                 ("", "vendor");
  emplace_sysfs_request<query::pcie_device>                 ("", "device");
  emplace_sysfs_request<query::pcie_subsystem_vendor>       ("", "subsystem_vendor");
  emplace_sysfs_request<query::pcie_subsystem_id>           ("", "subsystem_device");
  emplace_sysfs_request<query::pcie_link_speed>             ("", "link_speed");
  emplace_sysfs_request<query::pcie_link_speed_max>         ("", "link_speed_max");
  emplace_sysfs_request<query::pcie_express_lane_width>     ("", "link_width");
  emplace_sysfs_request<query::pcie_express_lane_width_max> ("", "link_width_max");
  emplace_sysfs_request<query::dma_threads_raw>             ("dma", "channel_stat_raw");
  emplace_sysfs_request<query::rom_vbnv>                    ("rom", "VBNV");
  emplace_sysfs_request<query::rom_ddr_bank_size_gb>        ("rom", "ddr_bank_size");
  emplace_sysfs_request<query::rom_ddr_bank_count_max>      ("rom", "ddr_bank_count_max");
  emplace_sysfs_request<query::rom_fpga_name>               ("rom", "FPGA");
  emplace_sysfs_request<query::rom_raw>                     ("rom", "raw");
  emplace_sysfs_request<query::rom_uuid>                    ("rom", "uuid");
  emplace_sysfs_request<query::rom_time_since_epoch>        ("rom", "timestamp");
  emplace_sysfs_request<query::xclbin_uuid>                 ("", "xclbinuuid");
  emplace_sysfs_request<query::mem_topology_raw>            ("icap", "mem_topology");
  emplace_sysfs_request<query::ip_layout_raw>               ("icap", "ip_layout");
  emplace_sysfs_request<query::clock_freqs_mhz>             ("icap", "clock_freqs");
  emplace_sysfs_request<query::idcode>                      ("icap", "idcode");
  emplace_sysfs_request<query::status_mig_calibrated>       ("", "mig_calibration");
  emplace_sysfs_request<query::xmc_version>                 ("xmc", "version");
  emplace_sysfs_request<query::xmc_board_name>              ("xmc", "bd_name");
  emplace_sysfs_request<query::xmc_serial_num>              ("xmc", "serial_num");
  emplace_sysfs_request<query::xmc_max_power>               ("xmc", "max_power");
  emplace_sysfs_request<query::xmc_bmc_version>             ("xmc", "bmc_ver");
  emplace_sysfs_request<query::expected_bmc_version>        ("xmc", "exp_bmc_ver");
  emplace_sysfs_request<query::xmc_status>                  ("xmc", "status");
  emplace_sysfs_request<query::xmc_reg_base>                ("xmc", "reg_base");
  emplace_sysfs_request<query::dna_serial_num>              ("dna", "dna");
  emplace_sysfs_request<query::status_p2p_enabled>          ("", "p2p_enable");
  emplace_sysfs_request<query::temp_card_top_front>         ("xmc", "xmc_se98_temp0");
  emplace_sysfs_request<query::temp_card_top_rear>          ("xmc", "xmc_se98_temp1");
  emplace_sysfs_request<query::temp_card_bottom_front>      ("xmc", "xmc_se98_temp2");
  emplace_sysfs_request<query::temp_fpga>                   ("xmc", "xmc_fpga_temp");
  emplace_sysfs_request<query::fan_trigger_critical_temp>   ("xmc", "xmc_fan_temp");
  emplace_sysfs_request<query::fan_fan_presence>            ("xmc", "fan_presence");
  emplace_sysfs_request<query::fan_speed_rpm>               ("xmc", "xmc_fan_rpm");
  emplace_sysfs_request<query::ddr_temp_0>                  ("xmc", "xmc_ddr_temp0");
  emplace_sysfs_request<query::ddr_temp_1>                  ("xmc", "xmc_ddr_temp1");
  emplace_sysfs_request<query::ddr_temp_2>                  ("xmc", "xmc_ddr_temp2");
  emplace_sysfs_request<query::ddr_temp_3>                  ("xmc", "xmc_ddr_temp3");
  emplace_sysfs_request<query::hbm_temp>                    ("xmc", "xmc_hbm_temp");
  emplace_sysfs_request<query::cage_temp_0>                 ("xmc", "xmc_cage_temp0");
  emplace_sysfs_request<query::cage_temp_1>                 ("xmc", "xmc_cage_temp1");
  emplace_sysfs_request<query::cage_temp_2>                 ("xmc", "xmc_cage_temp2");
  emplace_sysfs_request<query::cage_temp_3>                 ("xmc", "xmc_cage_temp3");
  emplace_sysfs_request<query::v12v_pex_millivolts>         ("xmc", "xmc_12v_pex_vol");
  emplace_sysfs_request<query::v12v_pex_milliamps>          ("xmc", "xmc_12v_pex_curr");
  emplace_sysfs_request<query::v12v_aux_millivolts>         ("xmc", "xmc_12v_aux_vol");
  emplace_sysfs_request<query::v12v_aux_milliamps>          ("xmc", "xmc_12v_aux_curr");
  emplace_sysfs_request<query::v3v3_pex_millivolts>         ("xmc", "xmc_3v3_pex_vol");
  emplace_sysfs_request<query::v3v3_aux_millivolts>         ("xmc", "xmc_3v3_aux_vol");
  emplace_sysfs_request<query::ddr_vpp_bottom_millivolts>   ("xmc", "xmc_ddr_vpp_btm");
  emplace_sysfs_request<query::ddr_vpp_top_millivolts>      ("xmc", "xmc_ddr_vpp_top");

  emplace_sysfs_request<query::v5v5_system_millivolts>    ("xmc", "xmc_sys_5v5");
  emplace_sysfs_request<query::v1v2_vcc_top_millivolts>   ("xmc", "xmc_1v2_top");
  emplace_sysfs_request<query::v1v2_vcc_bottom_millivolts>("xmc", "xmc_vcc1v2_btm");
  emplace_sysfs_request<query::v1v8_millivolts>           ("xmc", "xmc_1v8");
  emplace_sysfs_request<query::v0v85_millivolts>          ("xmc", "xmc_0v85");
  emplace_sysfs_request<query::v0v9_vcc_millivolts>       ("xmc", "xmc_mgt0v9avcc");
  emplace_sysfs_request<query::v12v_sw_millivolts>        ("xmc", "xmc_12v_sw");
  emplace_sysfs_request<query::mgt_vtt_millivolts>        ("xmc", "xmc_mgtavtt");
  emplace_sysfs_request<query::int_vcc_millivolts>        ("xmc", "xmc_vccint_vol");
  emplace_sysfs_request<query::int_vcc_milliamps>         ("xmc", "xmc_vccint_curr");

  emplace_sysfs_request<query::v3v3_pex_milliamps>        ("xmc", "xmc_3v3_pex_curr");
  emplace_sysfs_request<query::v0v85_milliamps>           ("xmc", "xmc_0v85_curr");
  emplace_sysfs_request<query::v3v3_vcc_millivolts>       ("xmc", "xmc_3v3_vcc_vol");
  emplace_sysfs_request<query::hbm_1v2_millivolts>        ("xmc", "xmc_hbm_1v2_vol");
  emplace_sysfs_request<query::v2v5_vpp_millivolts>       ("xmc", "xmc_vpp2v5_vol");
  emplace_sysfs_request<query::int_bram_vcc_millivolts>   ("xmc", "xmc_vccint_bram_vol");

  emplace_sysfs_request<query::firewall_detect_level>     ("firewall", "detected_level");
  emplace_sysfs_request<query::firewall_status>           ("firewall", "detected_status");
  emplace_sysfs_request<query::firewall_time_sec>         ("firewall", "detected_time");

  emplace_sysfs_request<query::power_microwatts>          ("xmc", "xmc_power");

  //emplace_sysfs_request<query::mig_ecc_enabled,         sp::_4, "ecc_enabled");
  //emplace_sysfs_request<query::mig_ecc_status,          sp::_4, "ecc_status");
  //emplace_sysfs_request<query::mig_ecc_ce_cnt,          sp::_4, "ecc_ce_cnt");
  //emplace_sysfs_request<query::mig_ecc_ue_cnt,          sp::_4, "ecc_ue_cnt");
  //emplace_sysfs_request<query::mig_ecc_ce_ffa,          sp::_4, "ecc_ce_ffa");
  //emplace_sysfs_request<query::mig_ecc_ue_ffa,          sp::_4, "ecc_ue_ffa");

  emplace_sysfs_request<query::flash_bar_offset>          ("flash", "bar_off");
  emplace_sysfs_request<query::is_mfg>                    ("", "mfg");
  emplace_sysfs_request<query::f_flash_type>              ("flash", "flash_type");
  emplace_sysfs_request<query::flash_type>                ("", "flash_type");
  emplace_sysfs_request<query::board_name>                ("", "board_name");
  emplace_sysfs_request<query::logic_uuids>               ("", "logic_uuids");
  emplace_sysfs_request<query::interface_uuids>           ("", "interface_uuids");

  emplace_func0_request<query::pcie_bdf,                  bdf>();
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

  if (it == query_tbl.end())
    throw query::no_such_key(query_key);

  return *(it->second);
}

device_linux::
device_linux(handle_type device_handle, id_type device_id, bool user)
  : shim<device_pcie>(device_handle, device_id, user)
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
    pt_dma.put( "h2c", xrt_core::utils::unit_convert(devstat.h2c[idx]) );
    pt_dma.put( "c2h", xrt_core::utils::unit_convert(devstat.c2h[idx]) );

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

void 
device_linux::
reset(const char* subdev, const char* key, const char* value) const 
{
  std::string err;
  pcidev::get_dev(get_device_id(), false)->sysfs_put(subdev, key, err, value);
  if (!err.empty())
    throw error("reset failed");
}

int 
device_linux::
open(const std::string& subdev, int flag) const
{
  return pcidev::get_dev(get_device_id(), false)->open(subdev, flag);
}

void
device_linux::
close(int dev_handle) const 
{
  pcidev::get_dev(get_device_id(), false)->close(dev_handle);
}

} // xrt_core
