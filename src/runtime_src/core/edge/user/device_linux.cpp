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


#include "device_linux.h"
#include "core/common/query_requests.h"

#include "xrt.h"
#include "zynq_dev.h"

#include <string>
#include <memory>
#include <iostream>
#include <map>
#include <boost/format.hpp>

namespace {

namespace query = xrt_core::query;
using key_type = query::key_type;
xclDeviceHandle handle;
xclDeviceInfo2 deviceInfo;

static std::map<query::key_type, std::unique_ptr<query::request>> query_tbl;

static zynq_device*
get_edgedev(const xrt_core::device* device)
{
  return zynq_device::get_dev();
}

struct devInfo
{
   static boost::any
    get(const xrt_core::device* device,key_type key)
  {
    auto edev = get_edgedev(device);
    switch (key)
    {
    case key_type::edge_device:
      return deviceInfo.mDeviceId;
    case key_type::edge_vendor:
      return deviceInfo.mVendorId;
    case key_type::edge_subsystem_vendor:
      return deviceInfo.mSubsystemVendorId;
    case key_type::edge_subsystem_id:
      return deviceInfo.mSubsystemId;
    case key_type::rom_vbnv:
      return std::string(deviceInfo.mName);
    case key_type::rom_ddr_bank_size:
      return deviceInfo.mDDRSize;
    case key_type::rom_ddr_bank_count_max:
      return deviceInfo.mDDRBankCount;
    /*returning dummy value for registered pcie_bdf query request*/
    case key_type::pcie_bdf:
      {
        uint16_t ret = std::numeric_limits<uint16_t>::max();
        return std::make_tuple(ret, ret, ret);
      }
    default:
      return std::string("NA");
    }
  }
};

// Specialize for other value types.
template <typename ValueType>
struct sysfs_fcn
{
  static ValueType
    get(zynq_device* dev, const char* entry)
  {
    std::string err;
    ValueType value;
    dev->sysfs_get(entry, err, value, static_cast<ValueType>(-1));
    if (!err.empty()){
      if(typeid(value) == typeid(bool))
        return false;
      return (static_cast<ValueType>(0));
    }
    return value;
  }
};

template <>
struct sysfs_fcn<std::string>
{
  static std::string
    get(zynq_device* dev, const char* entry)
  {
    std::string err;
    std::string value;
    dev->sysfs_get(entry, err, value);
    if (!err.empty())
      return std::string("NA");
    return value;
  }
};

template <typename VectorValueType>
struct sysfs_fcn<std::vector<VectorValueType>>
{
  //using ValueType = std::vector<std::string>;
  using ValueType = std::vector<VectorValueType>;

  static ValueType
    get(zynq_device* dev, const char* entry)
  {
    std::string err;
    ValueType value;
    dev->sysfs_get(entry, err, value);

    return value;
  }
};

template <typename QueryRequestType>
struct sysfs_getter : QueryRequestType
{
  const char* entry;

  sysfs_getter(const char* e)
    : entry(e)
  {}

  boost::any
    get(const xrt_core::device* device) const
  {
      return sysfs_fcn<typename QueryRequestType::result_type>
        ::get(get_edgedev(device), entry);
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

template <typename QueryRequestType>
static void
emplace_sysfs_request(const char* entry)
{
  auto x = QueryRequestType::key;
  query_tbl.emplace(x, std::make_unique<sysfs_getter<QueryRequestType>>(entry));
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
  emplace_func0_request<query::edge_vendor, devInfo>();
  emplace_func0_request<query::edge_device, devInfo>();
  emplace_func0_request<query::edge_subsystem_vendor, devInfo>();
  emplace_func0_request<query::edge_subsystem_id, devInfo>();
  /*pcie_bdf query request is emplaced as there is call from sub-cmd scan*/
  emplace_func0_request<query::pcie_bdf, devInfo>();

  emplace_sysfs_request<query::dma_threads_raw>           ("channel_stat_raw");
 
  emplace_func0_request<query::rom_vbnv, devInfo>();
  emplace_func0_request<query::rom_fpga_name, devInfo>();
  emplace_func0_request<query::rom_ddr_bank_size, devInfo>();
  emplace_func0_request<query::rom_ddr_bank_count_max, devInfo>();
 
  emplace_sysfs_request<query::rom_raw>                   ("raw");
  emplace_sysfs_request<query::rom_uuid>                  ("uuid");
  emplace_sysfs_request<query::rom_time_since_epoch>      ("timestamp");
  emplace_sysfs_request<query::xclbin_uuid>               ("xclbinid");
  emplace_sysfs_request<query::mem_topology_raw>          ("mem_topology");
  emplace_sysfs_request<query::ip_layout_raw>             ("ip_layout");
  emplace_sysfs_request<query::clock_freqs>               ("clock_freqs");
  emplace_sysfs_request<query::idcode>                    ("idcode");
  emplace_sysfs_request<query::status_mig_calibrated>     ("mig_calibration");
  emplace_sysfs_request<query::xmc_version>               ("version");
  emplace_sysfs_request<query::xmc_serial_num>            ("serial_num");
  emplace_sysfs_request<query::xmc_max_power>             ("max_power");
  emplace_sysfs_request<query::xmc_bmc_version>           ("bmc_ver");
  emplace_sysfs_request<query::xmc_status>                ("status");
  emplace_sysfs_request<query::xmc_reg_base>              ("reg_base");
  emplace_sysfs_request<query::dna_serial_num>            ("dna");
  emplace_sysfs_request<query::status_p2p_enabled>        ("p2p_enable");
  emplace_sysfs_request<query::temp_card_top_front>       ("xmc_se98_temp0");
  emplace_sysfs_request<query::temp_card_top_rear>        ("xmc_se98_temp1");
  emplace_sysfs_request<query::temp_card_bottom_front>    ("xmc_se98_temp2");
  emplace_sysfs_request<query::temp_fpga>                 ("xmc_fpga_temp");
  emplace_sysfs_request<query::fan_trigger_critical_temp> ("xmc_fan_temp");
  emplace_sysfs_request<query::fan_fan_presence>          ("fan_presence");
  emplace_sysfs_request<query::fan_speed_rpm>             ("xmc_fan_rpm");
  emplace_sysfs_request<query::ddr_temp_0>                ("xmc_ddr_temp0");
  emplace_sysfs_request<query::ddr_temp_1>                ("xmc_ddr_temp1");
  emplace_sysfs_request<query::ddr_temp_2>                ("xmc_ddr_temp2");
  emplace_sysfs_request<query::ddr_temp_3>                ("xmc_ddr_temp3");
  emplace_sysfs_request<query::hbm_temp>                  ("xmc_hbm_temp");
  emplace_sysfs_request<query::cage_temp_0>               ("xmc_cage_temp0");
  emplace_sysfs_request<query::cage_temp_1>               ("xmc_cage_temp1");
  emplace_sysfs_request<query::cage_temp_2>               ("xmc_cage_temp2");
  emplace_sysfs_request<query::cage_temp_3>               ("xmc_cage_temp3");
  emplace_sysfs_request<query::v12v_pex_millivolts>       ("xmc_12v_pex_vol");
  emplace_sysfs_request<query::v12v_pex_milliamps>        ("xmc_12v_pex_curr");
  emplace_sysfs_request<query::v12v_aux_millivolts>       ("xmc_12v_aux_vol");
  emplace_sysfs_request<query::v12v_aux_milliamps>        ("xmc_12v_aux_curr");
  emplace_sysfs_request<query::v3v3_pex_millivolts>       ("xmc_3v3_pex_vol");
  emplace_sysfs_request<query::v3v3_aux_millivolts>       ("xmc_3v3_aux_vol");
  emplace_sysfs_request<query::ddr_vpp_bottom_millivolts> ("xmc_ddr_vpp_btm");
  emplace_sysfs_request<query::ddr_vpp_top_millivolts>    ("xmc_ddr_vpp_top");

  emplace_sysfs_request<query::v5v5_system_millivolts>    ("xmc_sys_5v5");
  emplace_sysfs_request<query::v1v2_vcc_top_millivolts>   ("xmc_1v2_top");
  emplace_sysfs_request<query::v1v2_vcc_bottom_millivolts>("xmc_vcc1v2_btm");
  emplace_sysfs_request<query::v1v8_millivolts>           ("xmc_1v8");
  emplace_sysfs_request<query::v0v85_millivolts>          ("xmc_0v85");
  emplace_sysfs_request<query::v0v9_vcc_millivolts>       ("xmc_mgt0v9avcc");
  emplace_sysfs_request<query::v12v_sw_millivolts>        ("xmc_12v_sw");
  emplace_sysfs_request<query::mgt_vtt_millivolts>        ("xmc_mgtavtt");
  emplace_sysfs_request<query::int_vcc_millivolts>        ("xmc_vccint_vol");
  emplace_sysfs_request<query::int_vcc_milliamps>         ("xmc_vccint_curr");

  emplace_sysfs_request<query::v3v3_pex_milliamps>        ("xmc_3v3_pex_curr");
  emplace_sysfs_request<query::v0v85_milliamps>           ("xmc_0v85_curr");
  emplace_sysfs_request<query::v3v3_vcc_millivolts>       ("xmc_3v3_vcc_vol");
  emplace_sysfs_request<query::hbm_1v2_millivolts>        ("xmc_hbm_1v2_vol");
  emplace_sysfs_request<query::v2v5_vpp_millivolts>       ("xmc_vpp2v5_vol");
  emplace_sysfs_request<query::int_bram_vcc_millivolts>   ("xmc_vccint_bram_vol");

  emplace_sysfs_request<query::firewall_detect_level>     ("detected_level");
  emplace_sysfs_request<query::firewall_status>           ("detected_status");
  emplace_sysfs_request<query::firewall_time_sec>         ("detected_time");

  emplace_sysfs_request<query::power_microwatts>          ("xmc_power");
}

struct X { X() { initialize_query_table(); } };
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
device_linux(id_type device_id, bool user)
  : shim<device_edge>(device_id, user)
{
  handle = get_device_handle();
  xclGetDeviceInfo2(handle, &deviceInfo);
}

device_linux::
device_linux(handle_type device_handle, id_type device_id)
  : shim<device_edge>(device_handle, device_id)
{
}

void
device_linux::
read_dma_stats(boost::property_tree::ptree& pt) const
{
}

void
device_linux::
read(uint64_t offset, void* buf, uint64_t len) const
{

  throw error(-ENODEV, "read failed");
}

void
device_linux::
write(uint64_t offset, const void* buf, uint64_t len) const
{
  throw error(-ENODEV, "write failed");
}

} // xrt_core
