// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
#include "device_linux.h"
#include "xrt.h"
#include "zynq_dev.h"
#include "aie_sys_parser.h"
#include "smi.h"

#include "core/common/debug_ip.h"
#include "core/common/query_requests.h"
#include "core/common/xrt_profiling.h"
#include "shim.h"
#ifdef XRT_ENABLE_AIE
#include "core/edge/user/aie/graph_object.h"
#include "core/edge/user/aie/aie_buffer_object.h"
#endif
#include "core/edge/user/aie/profile_object.h"
#include <filesystem>
#include <map>
#include <memory>
#include <poll.h>
#include <regex>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "core/edge/include/zynq_ioctl.h"
#include "aie/aiereg.h"
#include <fcntl.h>
#ifdef XRT_ENABLE_AIE 
extern "C" {
#include <xaiengine.h>
}
#include "xaiengine/xlnx-ai-engine.h"
#endif
#include <sys/ioctl.h>



namespace {

namespace query = xrt_core::query;
using key_type = query::key_type;
xclDeviceHandle handle;
xclDeviceInfo2 deviceInfo;

struct drm_fd
{
  int fd;
  drm_fd(const std::string& file_path, int flags)
  {
    fd = open(file_path.c_str(),flags);
  }
  ~drm_fd()
  {
    if(fd > 0) {
      close(fd);
    }
  }
};

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
    return std::make_tuple(0,0,0,0);
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

static xclDeviceInfo2
init_device_info(const xrt_core::device* device)
{
  xclDeviceInfo2 dinfo;
  xclGetDeviceInfo2(device->get_user_handle(), &dinfo);
  return dinfo;
}

struct dev_info
{
  static std::any
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
    case key_type::device_class:
            return xrt_core::query::device_class::type::alveo;
    default:
      throw query::no_such_key(key);
    }
  }
};

struct aie_metadata_info{
  uint32_t num_cols;
  uint32_t num_rows;
  uint32_t shim_row;
  uint32_t core_row;
  uint32_t mem_row;
  uint32_t num_mem_row;
  uint8_t hw_gen;
};

// Function to get aie max rows and cols by parsing aie_metadata sysfs node
static aie_metadata_info
get_aie_metadata_info(const xrt_core::device* device)
{
  std::string err;
  std::string value;
  static const std::string AIE_TAG = "aie_metadata";
  constexpr uint32_t major = 1;
  constexpr uint32_t minor = 0;
  constexpr uint32_t patch = 0;
  aie_metadata_info aie_meta;

  auto dev = get_edgedev(device);

  dev->sysfs_get(AIE_TAG, err, value);
  if (!err.empty())
    throw xrt_core::query::sysfs_error(err);

  std::stringstream ss(value);
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(ss, pt);

  if (pt.get<uint32_t>("schema_version.major") != major ||
      pt.get<uint32_t>("schema_version.minor") != minor ||
      pt.get<uint32_t>("schema_version.patch") != patch )
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Aie Metadata major:minor:patch [%d:%d:%d] version are not matching")
        % pt.get<uint32_t>("schema_version.major")
        % pt.get<uint32_t>("schema_version.minor")
        % pt.get<uint32_t>("schema_version.patch")));

  aie_meta.num_cols = pt.get<uint32_t>("aie_metadata.driver_config.num_columns");
  aie_meta.num_rows = pt.get<uint32_t>("aie_metadata.driver_config.num_rows");
  aie_meta.shim_row = pt.get<uint32_t>("aie_metadata.driver_config.shim_row");
  aie_meta.core_row = pt.get<uint32_t>("aie_metadata.driver_config.aie_tile_row_start");
  if (!pt.get_optional<uint32_t>("aie_metadata.driver_config.mem_tile_row_start") || 
      !pt.get_optional<uint32_t>("aie_metadata.driver_config.mem_tile_num_rows")) {
       aie_meta.mem_row = pt.get<uint32_t>("aie_metadata.driver_config.reserved_row_start");
       aie_meta.num_mem_row = pt.get<uint32_t>("aie_metadata.driver_config.reserved_num_rows");
  }
  else {
       aie_meta.mem_row = pt.get<uint32_t>("aie_metadata.driver_config.mem_tile_row_start");
       aie_meta.num_mem_row = pt.get<uint32_t>("aie_metadata.driver_config.mem_tile_num_rows");
  }
  aie_meta.hw_gen = pt.get<uint8_t>("aie_metadata.driver_config.hw_gen");
  return aie_meta;
}

struct aie_core_info_sysfs
{
  using result_type = query::aie_core_info_sysfs::result_type;
  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    boost::property_tree::ptree ptarray;
    aie_metadata_info aie_meta = get_aie_metadata_info(device);
    auto base_path = "/sys/class/aie/";
    std::regex pattern(R"(aiepart_(\d+)_(\d+))");

    for (const auto& entry : std::filesystem::directory_iterator(base_path)) {
        if (!entry.is_directory())
            continue;

        std::string dir_name = entry.path().filename().string();
        std::smatch matches;

        if (!std::regex_match(dir_name, matches, pattern))
            continue;

        auto start_col = std::stoi(matches[1].str());
        auto num_col = std::stoi(matches[2].str());
        auto aiepart = std::to_string(start_col) + "_" + std::to_string(num_col);
        const aie_sys_parser asp(aiepart);

        /* Loop each all aie core tiles and collect core, dma, events, errors, locks status. */
        for (int i = start_col; i < (start_col + num_col); ++i)
          for (int j = 0; j < (aie_meta.num_rows - 1); ++j)
            ptarray.push_back(std::make_pair(std::to_string(i) + "_" + std::to_string(j),
                              asp.aie_sys_read(i,(j + aie_meta.core_row))));
    }

    boost::property_tree::ptree pt;
    pt.add_child("aie_core",ptarray);
    pt.put("hw_gen",std::to_string(aie_meta.hw_gen));
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);
    std::string inifile_text = oss.str();
    return inifile_text;
  }
};

struct aie_shim_info_sysfs
{
  using result_type = query::aie_shim_info_sysfs::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    boost::property_tree::ptree ptarray;
    aie_metadata_info aie_meta = get_aie_metadata_info(device);
    const std::string aiepart = std::to_string(aie_meta.shim_row) + "_" + std::to_string(aie_meta.num_cols);
    const aie_sys_parser asp(aiepart);

    /* Loop all shim tiles and collect all dma, events, errors, locks status */
    for (int i=0; i < aie_meta.num_cols; ++i) {
      ptarray.push_back(std::make_pair(std::to_string(i) + "_" + std::to_string(aie_meta.shim_row),
				       asp.aie_sys_read(i, aie_meta.shim_row)));
    }

    boost::property_tree::ptree pt;
    pt.add_child("aie_shim",ptarray);
    pt.put("hw_gen",std::to_string(aie_meta.hw_gen));
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);
    std::string inifile_text = oss.str();
    return inifile_text;
  }
};

struct aie_mem_info_sysfs
{
  using result_type = query::aie_mem_info_sysfs::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    boost::property_tree::ptree ptarray;
    aie_metadata_info aie_meta = get_aie_metadata_info(device);
    const std::string aiepart = std::to_string(aie_meta.shim_row) + "_" + std::to_string(aie_meta.num_cols);
    const aie_sys_parser asp(aiepart);

    if (aie_meta.num_mem_row != 0) {
      /* Loop all mem tiles and collect all dma, events, errors, locks status */
      for (int i = 0; i < aie_meta.num_cols; ++i)
        for (int j = 0; j < (aie_meta.num_mem_row-1); ++j)
	  ptarray.push_back(std::make_pair(std::to_string(i) + "_" + std::to_string(j),
                            asp.aie_sys_read(i,(j + aie_meta.mem_row))));
    }

    boost::property_tree::ptree pt;
    pt.add_child("aie_mem",ptarray);
    pt.put("hw_gen",std::to_string(aie_meta.hw_gen));
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);
    std::string inifile_text = oss.str();
    return inifile_text;
  }
};

struct kds_cu_info
{
  using result_type = query::kds_cu_info::result_type;
  using data_type = query::kds_cu_info::data_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    auto edev = get_edgedev(device);

    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    std::vector<std::string> stats;
    std::string errmsg;

    // The kds_custat_raw is printing in formatted string of each line
    // Format: "%d,%s:%s,0x%lx,0x%x,%lu"
    // Using comma as separator.
    edev->sysfs_get("kds_custat_raw", errmsg, stats);
    if (!errmsg.empty())
      throw xrt_core::query::sysfs_error(errmsg);

    result_type cuStats;
    // stats e.g.
    // 0,0,vadd:vadd_1,0x1400000,0x4,0
    // 0,1,vadd:vadd_2,0x1500000,0x4,0
    // 0.2,mult:mult_1,0x1800000,0x4,0
    for (auto& line : stats) {
      boost::char_separator<char> sep(",");
      tokenizer tokens(line, sep);

      if (std::distance(tokens.begin(), tokens.end()) != 6)
        throw xrt_core::query::sysfs_error("CU statistic sysfs node corrupted");

      data_type data = { 0 };
      constexpr int radix = 16;
      tokenizer::iterator tok_it = tokens.begin();
      data.slot_index = std::stoi(std::string(*tok_it++));
      data.index     = std::stoi(std::string(*tok_it++));
      data.name      = std::string(*tok_it++);
      data.base_addr = std::stoull(std::string(*tok_it++), nullptr, radix);
      data.status    = std::stoul(std::string(*tok_it++), nullptr, radix);
      data.usages    = std::stoul(std::string(*tok_it++));

      cuStats.push_back(std::move(data));
    }

    return cuStats;
  }
};


struct xclbin_uuid
{

  using result_type = query::xclbin_uuid::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    std::vector<std::string> xclbin_info;
    std::string errmsg;
    auto edev = get_edgedev(device);
    edev->sysfs_get("xclbinid", errmsg, xclbin_info);
    if (!errmsg.empty())
      throw xrt_core::query::sysfs_error(errmsg);

    // xclbin_uuid e.g.
    // <slot_id> <uuid_slot_0>
    //	   0	 <uuid_slot_0>
    //	   1	 <uuid_slot_1>
    for (auto& line : xclbin_info) {
      boost::char_separator<char> sep(" ");
      tokenizer tokens(line, sep);

      if (std::distance(tokens.begin(), tokens.end()) != 2)
        throw xrt_core::query::sysfs_error("xclbinid sysfs node corrupted");

      tokenizer::iterator tok_it = tokens.begin();
      unsigned int slot_index = std::stoi(std::string(*tok_it++));
      //return the first slot uuid always for backward compatibility
      return std::string(*tok_it);
    }

    return "";
  }
};

struct xclbin_slots
{
  using result_type = query::xclbin_slots::result_type;
  using slot_info = query::xclbin_slots::slot_info;
  using slot_id = query::xclbin_slots::slot_id;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    std::vector<std::string> xclbin_info;
    std::string errmsg;
    auto edev = get_edgedev(device);
    edev->sysfs_get("xclbinid", errmsg, xclbin_info);
    if (!errmsg.empty())
      throw xrt_core::query::sysfs_error(errmsg);

    result_type xclbin_data;
    // xclbin_uuid e.g.
    // 0 <uuid_slot_0>
    // 1 <uuid_slot_1>
    for (auto& line : xclbin_info) {
      boost::char_separator<char> sep(" ");
      tokenizer tokens(line, sep);

      if (std::distance(tokens.begin(), tokens.end()) != 2)
        throw xrt_core::query::sysfs_error("xclbinid sysfs node corrupted");

      slot_info data {};
      tokenizer::iterator tok_it = tokens.begin();
      data.slot = std::stoi(std::string(*tok_it++));
      data.uuid = std::string(*tok_it++);

      xclbin_data.push_back(std::move(data));
    }

    return xclbin_data;
  }
};

struct instance
{
  using result_type = query::instance::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    std::string errmsg;
    auto edev = get_edgedev(device);

    std::string drv_exists;
    //check whether driver directory exists or not
    edev->sysfs_get("driver", errmsg, drv_exists);
    if (!errmsg.empty())
      throw xrt_core::query::sysfs_error(errmsg);

    //edge always has only one device. Return 0 if driver node is present
    return 0;
  }
};


struct aie_reg_read
{
  using result_type = query::aie_reg_read::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& r, const std::any& c, const std::any& reg)
  {
    auto dev = get_edgedev(device);
    uint32_t val = 0;
    // Get the row value and add one since the rows actually start at 1 not zero.
    const auto row = std::any_cast<query::aie_reg_read::row_type>(r) + 1;
    const auto col = std::any_cast<query::aie_reg_read::col_type>(c);
    const auto v = std::any_cast<query::aie_reg_read::reg_type>(reg);

#ifdef XRT_ENABLE_AIE
  const static std::string aie_tag = "aie_metadata";
  const std::string zocl_device = "/dev/dri/" + get_render_devname();
  const uint32_t major = 1;
  const uint32_t minor = 0;
  const uint32_t patch = 0;

  std::string err;
  std::string value;

  // Reading the aie_metadata sysfs.
  dev->sysfs_get(aie_tag, err, value);
  if (!err.empty())
    throw xrt_core::query::sysfs_error
      (err + ", The loading xclbin acceleration image doesn't use the Artificial "
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

  int mKernelFD = open(zocl_device.c_str(), O_RDWR);
  if (!mKernelFD)
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Cannot open %s") % zocl_device));

  XAie_DevInst* devInst;         // AIE Device Instance

  uint8_t mem_row_start, mem_num_rows;
  if (!pt.get_optional<uint8_t>("aie_metadata.driver_config.mem_tile_row_start") ||
      !pt.get_optional<uint8_t>("aie_metadata.driver_config.mem_tile_num_rows")) {
       mem_row_start = pt.get<uint8_t>("aie_metadata.driver_config.reserved_row_start");
       mem_num_rows = pt.get<uint8_t>("aie_metadata.driver_config.reserved_num_rows");
  }
  else {
       mem_row_start = pt.get<uint8_t>("aie_metadata.driver_config.mem_tile_row_start");
       mem_num_rows = pt.get<uint8_t>("aie_metadata.driver_config.mem_tile_num_rows");
  }

  XAie_SetupConfig(ConfigPtr,
    pt.get<uint8_t>("aie_metadata.driver_config.hw_gen"),
    pt.get<uint64_t>("aie_metadata.driver_config.base_address"),
    pt.get<uint8_t>("aie_metadata.driver_config.column_shift"),
    pt.get<uint8_t>("aie_metadata.driver_config.row_shift"),
    pt.get<uint8_t>("aie_metadata.driver_config.num_columns"),
    pt.get<uint8_t>("aie_metadata.driver_config.num_rows"),
    pt.get<uint8_t>("aie_metadata.driver_config.shim_row"),
    mem_row_start,
    mem_num_rows,
    pt.get<uint8_t>("aie_metadata.driver_config.aie_tile_row_start"),
    pt.get<uint8_t>("aie_metadata.driver_config.aie_tile_num_rows"));

  /* TODO get aie partition id and uid from XCLBIN or PDI, currently not supported*/
  uint32_t partition_id = 1;
  uint32_t uid = 0;
  drm_zocl_aie_fd aiefd = {0, partition_id, uid, 0 };
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

  rc = XAie_Read32(devInst, it->second + XAie_GetTileAddr(devInst,row,col), &val);
  if(rc != XAIE_OK)
    throw xrt_core::error(-EINVAL, boost::str(boost::format("Error reading register '%s' (0x%8x) for AIE[%u:%u]")
                                                            % v.c_str() % it->second % col % (row-1)));

#endif
    return val;
  }
};

static std::unique_ptr<drm_fd>
aie_get_drmfd(const xrt_core::device* device, const std::string& dev_path)
{
  const static std::string aie_tag = "aie_metadata";
  std::string err;
  std::string value;

  auto dev = get_edgedev(device);
  // Reading the aie_metadata sysfs.
  dev->sysfs_get(aie_tag, err, value);
  if (!err.empty())
    throw xrt_core::query::sysfs_error
    (err + ", The loading xclbin acceleration image doesn't use the Artificial "
     + "Intelligent Engines (AIE). No action will be performed.");

  return std::make_unique<drm_fd>(dev_path, O_RDWR);
}

struct aie_get_freq
{
  using result_type = query::aie_get_freq::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& partition_id)
  {
    result_type freq = 0;
#if defined(XRT_ENABLE_AIE)
    const std::string zocl_device = "/dev/dri/" + get_render_devname();
    auto fd_obj = aie_get_drmfd(device, zocl_device);
    if (fd_obj->fd < 0)
      throw xrt_core::error(-EINVAL, boost::str(boost::format("Cannot open %s") % zocl_device));

    struct drm_zocl_aie_freq_scale aie_arg;
    aie_arg.hw_ctx_id = 0;
    aie_arg.partition_id = std::any_cast<uint32_t>(partition_id);
    aie_arg.freq = 0;
    aie_arg.dir = 0;

    if (ioctl(fd_obj->fd, DRM_IOCTL_ZOCL_AIE_FREQSCALE, &aie_arg))
      throw xrt_core::error(-errno, boost::str(boost::format("Reading clock frequency from AIE partition(%d) failed") % aie_arg.partition_id));

    freq = aie_arg.freq;
#else
    throw xrt_core::error(-EINVAL, "AIE is not enabled for this device");
#endif
    return freq;
  }
};

struct aie_set_freq
{
  using result_type = query::aie_set_freq::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& partition_id, const std::any& freq)
  {
#if defined(XRT_ENABLE_AIE)
    const std::string zocl_device = "/dev/dri/" + get_render_devname();
    auto fd_obj = aie_get_drmfd(device, zocl_device);
    if (fd_obj->fd < 0)
      throw xrt_core::error(-EINVAL, boost::str(boost::format("Cannot open %s") % zocl_device));

    struct drm_zocl_aie_freq_scale aie_arg;
    aie_arg.partition_id = std::any_cast<uint32_t>(partition_id);
    aie_arg.freq = std::any_cast<uint64_t>(freq);
    aie_arg.dir = 1;

    if (ioctl(fd_obj->fd, DRM_IOCTL_ZOCL_AIE_FREQSCALE, &aie_arg))
      throw xrt_core::error(-errno, boost::str(boost::format("Setting clock frequency for AIE partition (%d) failed") % aie_arg.partition_id));

#else
    throw xrt_core::error(-EINVAL, "AIE is not enabled for this device");
#endif
    return true;
  }
};

struct aim_counter
{
  using result_type = query::aim_counter::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = std::any_cast<query::aim_counter::debug_ip_data_type>(dbg_ip_dt);

    return xrt_core::debug_ip::get_aim_counter_result(device, dbg_ip_data);
  }
};

struct am_counter
{
  using result_type = query::am_counter::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = std::any_cast<query::am_counter::debug_ip_data_type>(dbg_ip_dt);

    return xrt_core::debug_ip::get_am_counter_result(device, dbg_ip_data);
  }
};

struct xrt_smi_config
{
  using result_type = std::any;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& reqType)
  {
    if (key != key_type::xrt_smi_config)
      throw xrt_core::query::no_such_key(key, "Not implemented");

    std::string xrt_smi_config;
    const auto xrt_smi_config_type = std::any_cast<xrt_core::query::xrt_smi_config::type>(reqType);
    switch (xrt_smi_config_type) {
    case xrt_core::query::xrt_smi_config::type::options_config:
      xrt_smi_config = shim_edge::smi::get_smi_config();
      break;
    default:
      throw xrt_core::query::no_such_key(key, "Not implemented");
    }

    return xrt_smi_config;
  }
};

struct xrt_smi_lists
{
  using result_type = std::any;

  static result_type
  get(const xrt_core::device* /*device*/, key_type key)
  {
    throw xrt_core::query::no_such_key(key, "Not implemented");
  }

  static result_type
  get(const xrt_core::device* /*device*/, key_type key, const std::any& reqType)
  {
    if (key != key_type::xrt_smi_lists)
      throw xrt_core::query::no_such_key(key, "Not implemented");

    const auto xrt_smi_lists_type = std::any_cast<xrt_core::query::xrt_smi_lists::type>(reqType);
    switch (xrt_smi_lists_type) {
    case xrt_core::query::xrt_smi_lists::type::validate_tests:
      return shim_edge::smi::get_validate_tests();
    case xrt_core::query::xrt_smi_lists::type::examine_reports:
      return shim_edge::smi::get_examine_reports();
    default:
      throw xrt_core::query::no_such_key(key, "Not implemented");
    }
  }
};

struct asm_counter
{
  using result_type = query::asm_counter::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = std::any_cast<query::asm_counter::debug_ip_data_type>(dbg_ip_dt);

    return xrt_core::debug_ip::get_asm_counter_result(device, dbg_ip_data);
  }
};

struct lapc_status
{
  using result_type = query::lapc_status::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = std::any_cast<query::lapc_status::debug_ip_data_type>(dbg_ip_dt);

    return xrt_core::debug_ip::get_lapc_status(device, dbg_ip_data);
  }
};

struct spc_status
{
  using result_type = query::spc_status::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = std::any_cast<query::spc_status::debug_ip_data_type>(dbg_ip_dt);

    return xrt_core::debug_ip::get_spc_status(device, dbg_ip_data);
  }
};

struct accel_deadlock_status
{
  using result_type = query::accel_deadlock_status::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = std::any_cast<query::accel_deadlock_status::debug_ip_data_type>(dbg_ip_dt);

    return xrt_core::debug_ip::get_accel_deadlock_status(device, dbg_ip_data);
  }
};

struct dtbo_path
{
  using result_type = query::dtbo_path::result_type;
  using slot_id_type = query::dtbo_path::slot_id_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& slot_id)
  {
    std::vector<std::string> dtbo_path_vec;
    std::string errmsg;
    auto edev = get_edgedev(device);
    edev->sysfs_get("dtbo_path", errmsg, dtbo_path_vec);
    if (!errmsg.empty() || dtbo_path_vec.empty()) {
      // sysfs node is not accessible when bitstream is not loaded
      return {};
    }

    // sysfs node data eg:
    // <slot_id> <dtbo_path>
    //     0     <path 0>
    //     1     <path 1>
    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    for(auto &line : dtbo_path_vec) {
      boost::char_separator<char> sep(" ");
      tokenizer tokens(line, sep);

      if (std::distance(tokens.begin(), tokens.end()) != 2)
        throw xrt_core::query::sysfs_error("xclbinid sysfs node corrupted");

      tokenizer::iterator tok_it = tokens.begin();

      uint32_t slotId = static_cast<slot_id_type>(std::stoi(std::string(*tok_it++)));
      if(slotId == std::any_cast<slot_id_type>(slot_id))
        return std::string(*tok_it);
    }
    //if we reach here no matching slot is found
    throw xrt_core::query::sysfs_error("no matching slot is found");
  }
};

struct debug_ip_layout_path
{
  using result_type = xrt_core::query::debug_ip_layout_path::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    uint32_t size = std::any_cast<uint32_t>(param);
    std::string path;
    path.resize(size);

    // Get Debug Ip layout path
    xclGetDebugIPlayoutPath(device->get_user_handle(), const_cast<char*>(path.data()), size);
    return path;
  }
};

struct device_clock_freq_mhz {
  using result_type = xrt_core::query::device_clock_freq_mhz::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    return xclGetDeviceClockFreqMHz(device->get_user_handle());
  }
};

struct trace_buffer_info
{
  using result_type = xrt_core::query::trace_buffer_info::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    uint32_t input_samples = std::any_cast<uint32_t>(param);
    result_type buf_info;

    // Get trace buf size and trace samples
    xclGetTraceBufferInfo(device->get_user_handle(), input_samples, buf_info.samples, buf_info.buf_size);
    return buf_info;
  }
};

struct host_max_bandwidth_mbps
{
  using result_type = xrt_core::query::host_max_bandwidth_mbps::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    bool read = std::any_cast<bool>(param);

    // Get read/write host max bandwidth in MBps
    return read ? xclGetHostReadMaxBandwidthMBps(device->get_user_handle())
                : xclGetHostWriteMaxBandwidthMBps(device->get_user_handle());
  }
};

struct kernel_max_bandwidth_mbps
{
  using result_type = xrt_core::query::kernel_max_bandwidth_mbps::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    bool read = std::any_cast<bool>(param);

    // Get read/write host max bandwidth in MBps
    return read ? xclGetKernelReadMaxBandwidthMBps(device->get_user_handle())
                : xclGetKernelWriteMaxBandwidthMBps(device->get_user_handle());
  }
};

struct read_trace_data
{
  using result_type = xrt_core::query::read_trace_data::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    auto args = std::any_cast<xrt_core::query::read_trace_data::args>(param);

    result_type trace_buf;
    trace_buf.resize(args.buf_size);

    // read trace data
    xclReadTraceData(device->get_user_handle(), trace_buf.data(),
                     args.buf_size, args.samples, args.ip_base_addr, args.words_per_sample);
    return trace_buf;
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
      throw xrt_core::query::sysfs_error(err);

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
      throw xrt_core::query::sysfs_error(err);

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
      throw xrt_core::query::sysfs_error(err);

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

  std::any
  get(const xrt_core::device* device) const
  {
    return sysfs_fcn<typename QueryRequestType::result_type>
      ::get(get_edgedev(device), entry);
  }
};

template <typename QueryRequestType, typename Getter>
struct function0_get : QueryRequestType
{
  std::any
  get(const xrt_core::device* device) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k);
  }
};

template <typename QueryRequestType, typename Getter>
struct function2_get : QueryRequestType
{
  std::any
  get(const xrt_core::device* device, const std::any& arg1, const std::any& arg2) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k, arg1, arg2);
  }
};

template <typename QueryRequestType, typename Getter>
struct function3_get : QueryRequestType
{
  std::any
  get(const xrt_core::device* device, const std::any& arg1, const std::any& arg2, const std::any& arg3) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k, arg1, arg2, arg3);
  }
};

template <typename QueryRequestType, typename Getter>
struct function4_get : virtual QueryRequestType
{
  std::any
  get(const xrt_core::device* device, const std::any& arg1) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k, arg1);
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
emplace_func2_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function2_get<QueryRequestType, Getter>>());
}

template <typename QueryRequestType, typename Getter>
static void
emplace_func3_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function3_get<QueryRequestType, Getter>>());
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
  emplace_func0_request<query::edge_vendor,             dev_info>();

  emplace_func0_request<query::rom_vbnv,                dev_info>();
  emplace_func0_request<query::rom_fpga_name,           dev_info>();
  emplace_func0_request<query::rom_ddr_bank_size_gb,    dev_info>();
  emplace_func0_request<query::rom_ddr_bank_count_max,  dev_info>();
  emplace_func0_request<query::rom_time_since_epoch,    dev_info>();

  emplace_func0_request<query::clock_freqs_mhz,         dev_info>();
  emplace_func0_request<query::device_class,            dev_info>();
  emplace_func0_request<query::aie_core_info_sysfs,     aie_core_info_sysfs>();
  emplace_func0_request<query::aie_shim_info_sysfs,     aie_shim_info_sysfs>();
  emplace_func0_request<query::aie_mem_info_sysfs,      aie_mem_info_sysfs>();
  emplace_func3_request<query::aie_reg_read,            aie_reg_read>();
  emplace_func4_request<query::aie_get_freq,            aie_get_freq>();
  emplace_func2_request<query::aie_set_freq,            aie_set_freq>();

  emplace_sysfs_get<query::mem_topology_raw>          ("mem_topology");
  emplace_sysfs_get<query::group_topology>            ("mem_topology");
  emplace_sysfs_get<query::ip_layout_raw>             ("ip_layout");
  emplace_sysfs_get<query::debug_ip_layout_raw>       ("debug_ip_layout");
  emplace_sysfs_get<query::aie_metadata>              ("aie_metadata");
  emplace_sysfs_get<query::graph_status>              ("graph_status");
  emplace_sysfs_get<query::memstat>                   ("memstat");
  emplace_sysfs_get<query::memstat_raw>               ("memstat_raw");
  emplace_sysfs_get<query::error>                     ("errors");
  emplace_sysfs_get<query::xclbin_full>               ("xclbin_full");
  emplace_sysfs_get<query::host_mem_addr>             ("host_mem_addr");
  emplace_sysfs_get<query::host_mem_size>             ("host_mem_size");
  emplace_func0_request<query::pcie_bdf,                bdf>();
  emplace_func0_request<query::board_name,              board_name>();
  emplace_func0_request<query::xclbin_uuid ,            xclbin_uuid>();

  emplace_func0_request<query::kds_cu_info,             kds_cu_info>();
  emplace_func0_request<query::instance,                instance>();
  emplace_func0_request<query::xclbin_slots,            xclbin_slots>();

  emplace_func4_request<query::aim_counter,             aim_counter>();
  emplace_func4_request<query::am_counter,              am_counter>();
  emplace_func4_request<query::asm_counter,             asm_counter>();
  emplace_func4_request<query::xrt_smi_config,          xrt_smi_config>();
  emplace_func4_request<query::xrt_smi_lists,           xrt_smi_lists>();
  emplace_func4_request<query::lapc_status,             lapc_status>();
  emplace_func4_request<query::spc_status,              spc_status>();
  emplace_func4_request<query::accel_deadlock_status,   accel_deadlock_status>();
  emplace_func4_request<query::dtbo_path,               dtbo_path>();

  emplace_func4_request<query::debug_ip_layout_path,    debug_ip_layout_path>();
  emplace_func0_request<query::device_clock_freq_mhz,   device_clock_freq_mhz>();
  emplace_func4_request<query::trace_buffer_info,       trace_buffer_info>();
  emplace_func4_request<query::read_trace_data,         read_trace_data>();
  emplace_func4_request<query::host_max_bandwidth_mbps, host_max_bandwidth_mbps>();
  emplace_func4_request<query::kernel_max_bandwidth_mbps, kernel_max_bandwidth_mbps>();
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
// Custom ishim implementation
// Redefined from xrt_core::ishim for functions that are not
// universally implemented by all shims
////////////////////////////////////////////////////////////////
void
device_linux::
set_cu_read_range(cuidx_type cuidx, uint32_t start, uint32_t size)
{
  if (auto ret = xclIPSetReadRange(get_device_handle(), cuidx.index, start, size))
    throw xrt_core::error(ret, "failed to set cu read range");
}

std::unique_ptr<xrt_core::graph_handle>
device_linux::
open_graph_handle(const xrt::uuid& xclbin_id, const char* name, xrt::graph::access_mode am)
{
#ifdef XRT_ENABLE_AIE   
   return std::make_unique<zynqaie::graph_object>(
                  static_cast<ZYNQ::shim*>(get_device_handle()), xclbin_id, name, am);
#else
   throw xrt_core::error(std::errc::not_supported, __func__);;
#endif   
}

std::unique_ptr<xrt_core::profile_handle>
device_linux::
open_profile_handle()
{
#ifdef XRT_ENABLE_AIE

  auto drv = ZYNQ::shim::handleCheck(get_device_handle());

  if (not drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aie_array = drv->get_aie_array_shared();

  return std::make_unique<zynqaie::profile_object>(static_cast<ZYNQ::shim*>(get_device_handle()), aie_array);

#else
   throw xrt_core::error(std::errc::not_supported, __func__);
#endif   
}

std::unique_ptr<xrt_core::aie_buffer_handle>
device_linux::
open_aie_buffer_handle(const xrt::uuid& xclbin_id, const char* name)
{
#ifdef XRT_ENABLE_AIE
  return std::make_unique<zynqaie::aie_buffer_object>(this,xclbin_id,name);
#else
  throw xrt_core::error(std::errc::not_supported, __func__);;
#endif
}

std::unique_ptr<buffer_handle>
device_linux::
import_bo(pid_t pid, shared_handle::export_handle ehdl)
{
  if (pid == 0 || getpid() == pid)
    return xrt::shim_int::import_bo(get_device_handle(), ehdl);

  throw xrt_core::error(std::errc::not_supported, __func__);
}

void
device_linux::
get_device_info(xclDeviceInfo2 *info)
{
  if (auto ret = xclGetDeviceInfo2(get_device_handle(), info))
    throw system_error(ret, "failed to get device info");
}

std::string
device_linux::
get_sysfs_path(const std::string& subdev, const std::string& entry)
{
  constexpr size_t max_path = 256;
  std::string path_buf(max_path, '\0');

  if (auto ret = xclGetSysfsPath(get_device_handle(), subdev.c_str(), entry.c_str(), path_buf.data(), max_path))
    throw system_error(ret, "failed to get device info");

  return path_buf;
}

#ifdef XRT_ENABLE_AIE
 
void
device_linux::
open_aie_context(xrt::aie::access_mode am)
{
  auto drv = ZYNQ::shim::handleCheck(get_device_handle());

  if (int ret = drv->openAIEContext(am))
    throw xrt_core::error(ret, "Fail to open AIE context");

  drv->setAIEAccessMode(am);
}

void
device_linux::
reset_aie()
{
  auto drv = ZYNQ::shim::handleCheck(get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aie_array = drv->get_aie_array_shared();

  if (!aie_array->is_context_set())
    aie_array->open_context(this, xrt::aie::access_mode::primary);

  aie_array->reset(this, 0 /*hw_context_id*/, xrt_core::edge::aie::full_array_id);
}

void
device_linux::
wait_gmio(const char *gmioName)
{
  auto drv = ZYNQ::shim::handleCheck(get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aie_array = drv->get_aie_array_shared();

  if (!aie_array->is_context_set())
    aie_array->open_context(this, xrt::aie::access_mode::primary);

  aie_array->wait_gmio(gmioName);
}

void
device_linux::
load_axlf_meta(const axlf* buffer)
{
  if (auto ret = xclLoadXclBinMeta(get_device_handle(), buffer))
    throw system_error(ret, "failed to load xclbin");
}
#endif
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

std::cv_status
device_linux::
wait_ip_interrupt(xclInterruptNotifyHandle handle, int32_t timeout)
{
  struct pollfd pfd = {.fd=handle, .events=POLLIN};
  int32_t ret = 0;

  //Checking for only one fd; Only of one CU
  //Timeout value in milli seconds
  ret = ::poll(&pfd, 1, timeout);
  if (ret < 0)
    throw error(errno, "wait_timeout: failed POSIX poll");

  if (ret == 0) //Timeout occured
    return std::cv_status::timeout;

  if (pfd.revents & POLLIN) //Interrupt received
    return std::cv_status::no_timeout;

  throw error(-EINVAL, boost::str(boost::format("wait_timeout: POSIX poll unexpected event: %d")  % pfd.revents));
}

} // xrt_core
