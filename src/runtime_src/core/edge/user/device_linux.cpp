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

struct bdf 
{
  using result_type = query::pcie_bdf::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    return {0,0,0};
  }

};

struct board_name 
{
  using result_type = query::board_name::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    result_type deviceName("edge");
    std::ifstream VBNV("/etc/xocl.txt");
    if (VBNV.is_open()) {
      VBNV >> deviceName;
    }
    VBNV.close();
    return deviceName;
  }
};

struct is_ready 
{
  using result_type = query::is_ready::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    return true;
  }
};

struct devInfo
{
  static boost::any
  get(const xrt_core::device* device,key_type key)
  {
    auto edev = get_edgedev(device);
    switch (key) {
    case key_type::edge_vendor:
      return deviceInfo.mVendorId;
    case key_type::rom_vbnv:
      return std::string(deviceInfo.mName);
    case key_type::rom_ddr_bank_size_gb:
    {
      static const uint32_t BYTES_TO_GBYTES = 30;
      return (deviceInfo.mDDRSize >> BYTES_TO_GBYTES);
    }
    case key_type::rom_ddr_bank_count_max:
      return static_cast<uint64_t>(deviceInfo.mDDRBankCount);
    case key_type::clock_freqs_mhz:
    {
      std::vector<std::string> clk_freqs;
      for(int i = 0; i < sizeof(deviceInfo.mOCLFrequency)/sizeof(deviceInfo.mOCLFrequency[0]); i++)
        clk_freqs.push_back(std::to_string(deviceInfo.mOCLFrequency[i]));
      return clk_freqs;
    }
    default:
      throw query::no_such_key(key);
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
    if (!err.empty())
      throw std::runtime_error(err);
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
  get(zynq_device* dev, const char* entry)
  {
    std::string err;
    ValueType value;
    dev->sysfs_get(entry, err, value);
    if (!err.empty())
      throw std::runtime_error(err);
    return value;
  }
};

template <typename QueryRequestType>
struct sysfs_get : QueryRequestType
{
  const char* entry;

  sysfs_get(const char* e)
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
struct function0_get : QueryRequestType
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
emplace_sysfs_get(const char* entry)
{
  auto x = QueryRequestType::key;
  query_tbl.emplace(x, std::make_unique<sysfs_get<QueryRequestType>>(entry));
}

template <typename QueryRequestType, typename Getter>
static void
emplace_func0_get()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_get<QueryRequestType, Getter>>());
}

template <typename QueryRequestType, typename Getter>
static void
emplace_func0_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_get<QueryRequestType, Getter>>());
}

static void
initialize_query_table()
{
  emplace_func0_get<query::edge_vendor, devInfo>();

  emplace_func0_get<query::rom_vbnv, devInfo>();
  emplace_func0_get<query::rom_fpga_name, devInfo>();
  emplace_func0_get<query::rom_ddr_bank_size_gb, devInfo>();
  emplace_func0_get<query::rom_ddr_bank_count_max, devInfo>();

  emplace_func0_get<query::clock_freqs_mhz, devInfo>();
 
  emplace_sysfs_get<query::xclbin_uuid>               ("xclbinid");
  emplace_sysfs_get<query::mem_topology_raw>          ("mem_topology");
  emplace_sysfs_get<query::ip_layout_raw>             ("ip_layout");
  emplace_sysfs_get<query::aie_metadata>              ("aie_metadata");
  emplace_sysfs_get<query::memstat>                   ("memstat");
  emplace_sysfs_get<query::memstat_raw>               ("memstat_raw");
  emplace_sysfs_get<query::error>                     ("error");
  emplace_func0_request<query::pcie_bdf,                bdf>();
  emplace_func0_request<query::board_name,              board_name>();
  emplace_func0_request<query::is_ready,                is_ready>();
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
device_linux(handle_type device_handle, id_type device_id, bool user)
  : shim<device_edge>(device_handle, device_id, user)
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


void 
device_linux::
reset(query::reset_type key) const 
{
  switch(key.get_key()) {
  case query::reset_key::hot:
    throw error(-ENODEV, "Hot reset not supported");
  case query::reset_key::kernel:
    throw error(-ENODEV, "OCL dynamic region reset not supported");
  case query::reset_key::ert:
    throw error(-ENODEV, "ERT reset not supported");
  case query::reset_key::ecc:
    throw error(-ENODEV, "Soft Kernel reset not supported");
  case query::reset_key::aie:
    /* Commented till xrt shift to aie v2 driver */
    //if (auto ret = xclResetAIEArray(get_device_handle()))
    //  throw error(ret, "fail to reset aie array");
    break;
  default:
    throw error(-ENODEV, "invalid argument");
  }
}

} // xrt_core
