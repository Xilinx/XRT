/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "xrt.h"
#include "zynq_dev.h"
#include "aie_sys_parser.h"

#include "core/common/query_requests.h"

#include <map>
#include <memory>
#include <string>

#include <unistd.h>

#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#ifdef XRT_ENABLE_AIE
#include "core/edge/include/zynq_ioctl.h"
#include "aie/aiereg.h"
#include <fcntl.h>
extern "C" {
#include <xaiengine.h>
}
#ifndef __AIESIM__
#include "xaiengine/xlnx-ai-engine.h"
#include <sys/ioctl.h>
#endif
#endif

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
    return std::make_tuple(0,0,0);
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

static xclDeviceInfo2
init_device_info(const xrt_core::device* device)
{
  xclDeviceInfo2 dinfo;
  xclGetDeviceInfo2(device->get_user_handle(), &dinfo);
  return dinfo;
}

struct dev_info
{
  static boost::any
  get(const xrt_core::device* device,key_type key)
  {
    auto edev = get_edgedev(device);
    static std::map<const xrt_core::device*, xclDeviceInfo2> infomap;
    auto it = infomap.find(device);
    if (it == infomap.end()) {
      auto ret = infomap.emplace(device,init_device_info(device));
      it = ret.first;
    }

    auto& deviceInfo = (*it).second;
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
    case key_type::rom_time_since_epoch:
      return static_cast<uint64_t>(deviceInfo.mTimeStamp);
    default:
      throw query::no_such_key(key);
    }
  }
};

struct aie_metadata {
  /* Function to read aie_metadata sysfs, and parse max rows and max columns from it. */
  static void
  read_aie_metadata(const xrt_core::device* device, uint32_t &row, uint32_t &col)
  {
    std::string err;
    std::string value;
    const static std::string AIE_TAG = "aie_metadata";
    const uint32_t major = 1;
    const uint32_t minor = 0;
    const uint32_t patch = 0;
    
    auto dev = get_edgedev(device);

    dev->sysfs_get(AIE_TAG, err, value);
    if (!err.empty())
      throw xrt_core::error(-EINVAL, err);

    std::stringstream ss(value);
    boost::property_tree::ptree pt; 
    boost::property_tree::read_json(ss, pt);

    if(pt.get<uint32_t>("schema_version.major") != major ||
       pt.get<uint32_t>("schema_version.minor") != minor ||
       pt.get<uint32_t>("schema_version.patch") != patch )
      throw xrt_core::error(-EINVAL, boost::str(boost::format("Aie Metadata major:minor:patch [%d:%d:%d] version are not matching")
                                                             % pt.get<uint32_t>("schema_version.major")
                                                             % pt.get<uint32_t>("schema_version.minor")
                                                             % pt.get<uint32_t>("schema_version.patch")));
    col = pt.get<uint32_t>("aie_metadata.driver_config.num_columns");
    row = pt.get<uint32_t>("aie_metadata.driver_config.num_rows");
  }
};

struct aie_core_info : aie_metadata
{
  using result_type = query::aie_core_info::result_type;
  static result_type
  get(const xrt_core::device* device,key_type key)
  {
    boost::property_tree::ptree ptarray;
    uint32_t max_col = 0, max_row = 0;

    read_aie_metadata(device, max_row, max_col);

    /* Loop each all aie core tiles and collect core, dma, events, errors, locks status. */ 
    for(int i=0;i<max_col;i++)
      for(int j=0; j<(max_row-1);j++)
        ptarray.push_back(std::make_pair(std::to_string(i)+"_"+std::to_string(j),
                          aie_sys_parser::get_parser()->aie_sys_read(i,(j+1)))); 

    boost::property_tree::ptree pt;
    pt.add_child("aie_core",ptarray);
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);

    std::string inifile_text = oss.str();
    return inifile_text;
  }
};

struct aie_shim_info : aie_metadata
{
  using result_type = query::aie_shim_info::result_type;
  static result_type
  get(const xrt_core::device* device,key_type key)
  {
    boost::property_tree::ptree ptarray;
    uint32_t max_col = 0, max_row = 0;

    read_aie_metadata(device, max_row, max_col);

    /* Loop all shim tiles and collect all dma, events, errors, locks status */
    for(int i=0;i<max_col;i++) {
      ptarray.push_back(std::make_pair("", aie_sys_parser::get_parser()->aie_sys_read(i,0))); 
    }

    boost::property_tree::ptree pt;
    pt.add_child("aie_shim",ptarray);
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);
    std::string inifile_text = oss.str();
    return inifile_text;
  }
};

struct kds_cu_info
{
  using result_type = query::kds_cu_info::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    auto edev = get_edgedev(device);

    std::vector<std::string> stats;
    std::string errmsg;
    edev->sysfs_get("kds_custat", errmsg, stats);
    if (!errmsg.empty())
      throw std::runtime_error(errmsg);

    result_type cuStats;
    for (auto& line : stats) {
        uint32_t base_address = 0;
        uint32_t usages = 0;
        uint32_t status = 0;
        sscanf(line.c_str(), "CU[@0x%x] : %d status : %d", &base_address, &usages, &status);
        cuStats.push_back(std::make_tuple(base_address, usages, status));
    }

    return cuStats;
  }
};

struct aie_reg_read
{
  using result_type = query::aie_reg_read::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& r, const boost::any& c, const boost::any& reg)
  {
    auto dev = get_edgedev(device);
    uint32_t val = 0;
    // Get the row value and add one since the rows actually start at 1 not zero.
    const auto row = boost::any_cast<query::aie_reg_read::row_type>(r) + 1;
    const auto col = boost::any_cast<query::aie_reg_read::col_type>(c);
    const auto v = boost::any_cast<query::aie_reg_read::reg_type>(reg);
 
#ifdef XRT_ENABLE_AIE
#ifndef __AIESIM__
  const static std::string AIE_TAG = "aie_metadata";
  const static std::string ZOCL_DEVICE = "/dev/dri/renderD128";
  const uint32_t major = 1;
  const uint32_t minor = 0;
  const uint32_t patch = 0;

  std::string err;
  std::string value;

  // Reading the aie_metadata sysfs.
  dev->sysfs_get(AIE_TAG, err, value);
  if (!err.empty())
    throw xrt_core::error(-EINVAL, err + ", The loading xclbin acceleration image doesn't use the Artificial "
                                  + "Intelligent Engines (AIE). No action will be performed.");
  std::stringstream ss(value);
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(ss, pt);

  if(pt.get<uint32_t>("schema_version.major") != major ||
     pt.get<uint32_t>("schema_version.minor") != minor ||
     pt.get<uint32_t>("schema_version.patch") != patch )
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Aie Metadata major:minor:patch [%d:%d:%d] version are not matching")
                                                             % pt.get<uint32_t>("schema_version.major")
                                                             % pt.get<uint32_t>("schema_version.minor")
                                                             % pt.get<uint32_t>("schema_version.patch")));

  int mKernelFD = open(ZOCL_DEVICE.c_str(), O_RDWR);
  if (!mKernelFD)
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Cannot open %s") % ZOCL_DEVICE));

  XAie_DevInst* devInst;         // AIE Device Instance

  XAie_SetupConfig(ConfigPtr,
    pt.get<uint8_t>("aie_metadata.driver_config.hw_gen"),
    pt.get<uint64_t>("aie_metadata.driver_config.base_address"),
    pt.get<uint8_t>("aie_metadata.driver_config.column_shift"),
    pt.get<uint8_t>("aie_metadata.driver_config.row_shift"),
    pt.get<uint8_t>("aie_metadata.driver_config.num_columns"),
    pt.get<uint8_t>("aie_metadata.driver_config.num_rows"),
    pt.get<uint8_t>("aie_metadata.driver_config.shim_row"),
    pt.get<uint8_t>("aie_metadata.driver_config.reserved_row_start"),
    pt.get<uint8_t>("aie_metadata.driver_config.reserved_num_rows"),
    pt.get<uint8_t>("aie_metadata.driver_config.aie_tile_row_start"),
    pt.get<uint8_t>("aie_metadata.driver_config.aie_tile_num_rows"));

  /* TODO get aie partition id and uid from XCLBIN or PDI, currently not supported*/
  uint32_t partition_id = 1;
  uint32_t uid = 0;
  drm_zocl_aie_fd aiefd = { partition_id, uid, 0 };
  if (ioctl(mKernelFD, DRM_IOCTL_ZOCL_AIE_FD, &aiefd))
    throw xrt_core::error(-errno, "Create AIE failed. Can not get AIE fd");

  ConfigPtr.PartProp.Handle = aiefd.fd;

  AieRC rc;
  XAie_InstDeclare(DevInst, &ConfigPtr);
  if ((rc = XAie_CfgInitialize(&DevInst, &ConfigPtr)) != XAIE_OK)
    throw xrt_core::error(-EINVAL, "Failed to initialize AIE configuration, error: " + std::to_string(rc));

  devInst = &DevInst;
  if(!devInst)
    throw xrt_core::error(-EINVAL, "Invalid aie object");

  auto max_row = pt.get<uint32_t>("aie_metadata.driver_config.num_rows");
  auto max_col = pt.get<uint32_t>("aie_metadata.driver_config.num_columns");
  if ((row <= 0) || (row >= max_row))
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Invalid row, Row should be in range [0,%u]") % (max_row-2)));

  if ((col < 0) || (col >= max_col))
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Invalid column, Column should be in range [0,%u]") % (max_col-1)));

  const std::map<std::string, uint32_t> &regmap = get_aie_register_map();
  auto it = regmap.find(v);
  if (it == regmap.end())
    throw xrt_core::error(-EINVAL, "Invalid register");

  rc = XAie_Read32(devInst, it->second + _XAie_GetTileAddr(devInst,row,col), &val);
  if(rc != XAIE_OK)
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Error reading register '%s' (0x%8x) for AIE[%u:%u]")
                                                            % v.c_str() % it->second % col % (row-1)));

#endif
#endif
    return val;
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

template <typename QueryRequestType, typename Getter>
struct function3_get : QueryRequestType
{
  boost::any
  get(const xrt_core::device* device, const boost::any& arg1, const boost::any& arg2, const boost::any& arg3) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k, arg1, arg2, arg3);
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
emplace_func0_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_get<QueryRequestType, Getter>>());
}

template <typename QueryRequestType, typename Getter>
static void
emplace_func3_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function3_get<QueryRequestType, Getter>>());
}

static void
initialize_query_table()
{
  emplace_func0_request<query::edge_vendor,             dev_info>();

  emplace_func0_request<query::rom_vbnv,                dev_info>();
  emplace_func0_request<query::rom_fpga_name,           dev_info>();
  emplace_func0_request<query::rom_ddr_bank_size_gb,    dev_info>();
  emplace_func0_request<query::rom_ddr_bank_count_max,  dev_info>();
  emplace_func0_request<query::rom_time_since_epoch,    dev_info>();

  emplace_func0_request<query::clock_freqs_mhz,         dev_info>();
  emplace_func0_request<query::aie_core_info,		aie_core_info>();
  emplace_func0_request<query::aie_shim_info,		aie_shim_info>();
  emplace_func0_request<query::kds_cu_info,             kds_cu_info>();
  emplace_func3_request<query::aie_reg_read,            aie_reg_read>();

  emplace_sysfs_get<query::xclbin_uuid>               ("xclbinid");
  emplace_sysfs_get<query::mem_topology_raw>          ("mem_topology");
  emplace_sysfs_get<query::ip_layout_raw>             ("ip_layout");
  emplace_sysfs_get<query::aie_metadata>              ("aie_metadata");
  emplace_sysfs_get<query::graph_status>              ("graph_status");
  emplace_sysfs_get<query::memstat>                   ("memstat");
  emplace_sysfs_get<query::memstat_raw>               ("memstat_raw");
  emplace_sysfs_get<query::error>                     ("errors");
  emplace_sysfs_get<query::xclbin_full>               ("xclbin_full");
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
    throw error(-ENODEV, "AIE reset not supported");
  default:
    throw error(-ENODEV, "invalid argument");
  }
}


////////////////////////////////////////////////////////////////
// Custom IP interrupt handling
////////////////////////////////////////////////////////////////
void
device_linux::
enable_ip_interrupt(xclInterruptNotifyHandle handle)
{
  int enable = 1;
  if (::write(handle, &enable, sizeof(enable)) == -1)
    throw error(errno, "enable_ip_interrupt failed POSIX write");
}

void
device_linux::
disable_ip_interrupt(xclInterruptNotifyHandle handle)
{
  int disable = 1;
  if (::write(handle, &disable, sizeof(disable)) == -1)
    throw error(errno, "disable_ip_interrupt failed POSIX write");
}

void
device_linux::
wait_ip_interrupt(xclInterruptNotifyHandle handle)
{
  int pending = 0;
  if (::read(handle, &pending, sizeof(pending)) == -1)
    throw error(errno, "wait_ip_interrupt failed POSIX read");
}

} // xrt_core
