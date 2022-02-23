/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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

#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "core/common/utils.h"
#include "core/include/xcl_app_debug.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"

#include "xrt.h"
#include "scan.h"

#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <poll.h>
#include <sys/syscall.h>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>

namespace {

namespace query = xrt_core::query;
using pdev = std::shared_ptr<pcidev::pci_device>;
using key_type = query::key_type;
const static int LEGACY_COUNT = 4;

static int
get_render_value(const std::string dir)
{
  static const std::string render_name = "renderD"; 
  int instance_num = INVALID_ID;
  std::string sub;

  boost::filesystem::path render_dirs(dir);
  if (!boost::filesystem::is_directory(render_dirs))
    return instance_num;
 
  boost::filesystem::recursive_directory_iterator end_iter;
    for(boost::filesystem::recursive_directory_iterator iter(render_dirs); iter != end_iter; ++iter) {
      if (iter->path().filename().string().compare(0,render_name.size(),render_name)== 0) {   
        sub = iter->path().filename().string().substr(render_name.size());
        instance_num = std::stoi(sub);
        break;
      }
    }
    return instance_num;
}

inline pdev
get_pcidev(const xrt_core::device* device)
{
  return pcidev::get_dev(device->get_device_id(), device->is_userpf());
}


static std::vector<uint64_t> 
get_counter_status_from_sysfs(const std::string &mon_name_address, 
                              const std::string &sysfs_file_name, 
                              size_t size, 
                              const xrt_core::device* device)
{
  auto pdev = get_pcidev(device);

  /* Get full path to "name" sysfs file. 
   * Then use that path to form full path to "counters"/"status" sysfs file 
   * which contains counter/status data for the monitor
   */
  std::string name_path = pdev->get_sysfs_path(mon_name_address, "name");

  std::size_t pos = name_path.find_last_of('/');
  if (std::string::npos == pos) {
    std::string msg = "Invalid path for name sysfs node for " + mon_name_address;
    throw xrt_core::query::sysfs_error(msg);
  }

  std::string path = name_path.substr(0, pos+1);
  path += sysfs_file_name;

  std::vector<uint64_t> val_buf(size);

  std::ifstream ifs(path);

  try {
    // Enable exception on reading error
    ifs.exceptions(std::ifstream::failbit);
    for (std::size_t idx = 0; idx < size; ++idx)
      ifs >> val_buf[idx];
  } catch (const std::ios_base::failure& fail) {
    std::string msg = "Incomplete counter data read from " + path + " due to " + fail.what()
                         + ".\n Using 0 as default value in results.";
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
  }

  return val_buf;
}


struct bdf
{
  using result_type = query::pcie_bdf::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    auto pdev = get_pcidev(device);
    return std::make_tuple(pdev->domain, pdev->bus, pdev->dev, pdev->func);
  }
};

struct kds_cu_info
{
  using result_type = query::kds_cu_info::result_type;
  using data_type = query::kds_cu_info::data_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    auto pdev = get_pcidev(device);

    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    std::vector<std::string> stats;
    std::string errmsg;

    // The kds_custat_raw is printing in formatted string of each line
    // Format: "%d,%s:%s,0x%lx,0x%x,%lu"
    // Using comma as separator.
    pdev->sysfs_get("", "kds_custat_raw", errmsg, stats);
    if (!errmsg.empty())
      throw xrt_core::query::sysfs_error(errmsg);

    result_type cuStats;
    for (auto& line : stats) {
      boost::char_separator<char> sep(",");
      tokenizer tokens(line, sep);

      /* TODO : For backward compartability changing the following logic
       * as the first column should represent the slot index */
      // stats e.g.
      // Slot index present
      //   0,0,vadd:vadd_1,0x1400000,0x4,0
      // Without Slot index
      //   0,vadd:vadd_1,0x1400000,0x4,0
      if ((std::distance(tokens.begin(), tokens.end()) != 5) &&
	(std::distance(tokens.begin(), tokens.end()) != 6))
        throw xrt_core::query::sysfs_error("CU statistic sysfs node corrupted");

      data_type data = { 0 };
      const int radix = 16;
      tokenizer::iterator tok_it = tokens.begin();
      if (std::distance(tokens.begin(), tokens.end()) == 6)
        data.slot_index =std::stoi(std::string(*tok_it++));

      data.index     = std::stoi(std::string(*tok_it++));
      data.name      = std::string(*tok_it++);
      data.base_addr = std::stoull(std::string(*tok_it++), nullptr, radix);
      data.status    = std::stoul(std::string(*tok_it++), nullptr, radix);
      data.usages    = std::stoul(std::string(*tok_it++));

      cuStats.push_back(data);
    }

    return cuStats;
  }
};

struct instance 
{
  using result_type = query::instance::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    static const std::string  dev_root = "/sys/bus/pci/devices/";
    std::string errmsg;
    auto pdev = get_pcidev(device);

    auto sysfsname = boost::str( boost::format("%04x:%02x:%02x.%x") % pdev->domain % pdev->bus % pdev->dev %  pdev->func);
    if(device->is_userpf())
      pdev->instance = get_render_value(dev_root + sysfsname + "/drm");
    else
      pdev->sysfs_get("", "instance", errmsg, pdev->instance,static_cast<uint32_t>(INVALID_ID));
  
    return pdev->instance;
  }

};


struct kds_scu_info
{
  using result_type = query::kds_scu_info::result_type;
  using data_type = query::kds_scu_info::data_type;
  static constexpr uint32_t scu_domain = 0x10000;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    auto pdev = get_pcidev(device);

    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    std::vector<std::string> stats;
    std::string errmsg;

    // The kds_scustat_raw is printing in formatted string of each line
    // Format: "%d,%s:%s,0x%x,%lu"
    // Using comma as separator.
    pdev->sysfs_get("", "kds_scustat_raw", errmsg, stats);
    if (!errmsg.empty())
      throw xrt_core::query::sysfs_error(errmsg);

    result_type cu_stats;
    std::map<std::string, unsigned int> name2idx; // kernel name -> instance index
    for (auto& line : stats) {
      boost::char_separator<char> sep(",");
      tokenizer tokens(line, sep);

      if ((std::distance(tokens.begin(), tokens.end()) != 4) &&
	  (std::distance(tokens.begin(), tokens.end()) != 5))
        throw xrt_core::query::sysfs_error("PS kernel statistic sysfs node corrupted");

      data_type data;
      const int radix = 16;
      tokenizer::iterator tok_it = tokens.begin();
      if (std::distance(tokens.begin(), tokens.end()) == 5)
	data.slot_index  = std::stoi(std::string(*tok_it++));
      data.index  = std::stoi(std::string(*tok_it++));
      data.name   = std::string(*tok_it++);  // kernel name
      data.status = std::stoul(std::string(*tok_it++), nullptr, radix);
      data.usages = std::stoul(std::string(*tok_it++));

      // TODO: Let's avoid this special handling for PS kernel name
      // Modify instance name to match up with xclbin convention where
      // instance names are numbered 0..n as in kernel_name:[0..n] for
      // kernel_name.  It is important that the instance name matches
      // up with the expectations of core XRT, which is based off of
      // the instance names in the IP_LAYOUT.  By using a knm -> idx
      // map the driver can assign internal indeces in any order it
      // likes and sysfs node does not have to list all kernel
      // instances of a kernel before instances of another kernel.
	  //      auto& kidx = name2idx[data.name];  // integral values are zero-initialized
	  //      data.name += ":scu_" + std::to_string(kidx++);

      cu_stats.push_back(data);
    }

    return cu_stats;
  }
};

/**
 * qspi write protection status
 * byte 0:
 *   '0': status not available, '1': status available
 * byte 1 primary qspi(if status available):
 *   '1': write protect enable, '2': write protect disable
 * byte 2 recovery qspi(if status available):
 *   '1': write protect enable, '2': write protect disable
 */
struct qspi_status
{
  using result_type = query::xmc_qspi_status::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    auto pdev = get_pcidev(device);
  
    std::string status_str, errmsg;
    pdev->sysfs_get("xmc", "xmc_qspi_status", errmsg, status_str);
    if (!errmsg.empty())
      throw xrt_core::query::sysfs_error(errmsg);

    std::string primary, recovery;
    for (auto status_byte : status_str) {
      if(status_byte == '0')
        return std::pair<std::string, std::string>("N/A", "N/A");

      if(primary.empty())
        primary = status_byte == '1' ? "Enabled": status_byte == '2' ? "Disabled" : "Invalid";
      else
        recovery = status_byte == '1' ? "Enabled": status_byte == '2' ? "Disabled" : "Invalid";
    }
    return std::pair<std::string, std::string>(primary, recovery);
  }
};

struct mac_addr_list
{
  using result_type = query::mac_addr_list::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    std::vector<std::string> list;
    auto pdev = get_pcidev(device);

    //legacy code exposes only 4 mac addr sysfs nodes (0-3)
    for (int i=0; i < LEGACY_COUNT; i++) {
      std::string addr, errmsg;
      pdev->sysfs_get("xmc", "mac_addr"+std::to_string(i), errmsg, addr);
      list.push_back(addr);
    }

    return list;
  }
};

/* AIM counter values 
 * In PCIe Linux, access the sysfs file for AIM to retrieve the AIM counter values
 */ 
struct aim_counter
{
  using result_type = query::aim_counter::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = boost::any_cast<query::aim_counter::debug_ip_data_type>(dbg_ip_dt);

    std::string aim_name("aximm_mon_");
    aim_name += std::to_string(dbg_ip_data->m_base_address);

    result_type retval_buf(XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT, 0);

    result_type val_buf = get_counter_status_from_sysfs(aim_name, "counters", XAIM_TOTAL_DEBUG_SAMPLE_COUNTERS_PER_SLOT, device);

    /* Note that required return values are NOT in contiguous sequential order 
     * in AIM subdevice file. So, need to read only a few isolated indices in val_buf.
     */
    retval_buf[XAIM_WRITE_BYTES_INDEX]        = val_buf[XAIM_IOCTL_WRITE_BYTES_INDEX];
    retval_buf[XAIM_WRITE_TRANX_INDEX]        = val_buf[XAIM_IOCTL_WRITE_TRANX_INDEX];
    retval_buf[XAIM_READ_BYTES_INDEX]         = val_buf[XAIM_IOCTL_READ_BYTES_INDEX];
    retval_buf[XAIM_READ_TRANX_INDEX]         = val_buf[XAIM_IOCTL_READ_TRANX_INDEX];
    retval_buf[XAIM_OUTSTANDING_COUNT_INDEX]  = val_buf[XAIM_IOCTL_OUTSTANDING_COUNT_INDEX];
    retval_buf[XAIM_WRITE_LAST_ADDRESS_INDEX] = val_buf[XAIM_IOCTL_WRITE_LAST_ADDRESS_INDEX];
    retval_buf[XAIM_WRITE_LAST_DATA_INDEX]    = val_buf[XAIM_IOCTL_WRITE_LAST_DATA_INDEX];
    retval_buf[XAIM_READ_LAST_ADDRESS_INDEX]  = val_buf[XAIM_IOCTL_READ_LAST_ADDRESS_INDEX];
    retval_buf[XAIM_READ_LAST_DATA_INDEX]     = val_buf[XAIM_IOCTL_READ_LAST_DATA_INDEX];

    return retval_buf;
  }

};


/* AM counter values 
 * In PCIe Linux, access the sysfs file for AM to retrieve the AM counter values
 */ 
struct am_counter
{
  using result_type = query::am_counter::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = boost::any_cast<query::am_counter::debug_ip_data_type>(dbg_ip_dt);

    std::string am_name("accel_mon_");
    am_name += std::to_string(dbg_ip_data->m_base_address);

    result_type val_buf = get_counter_status_from_sysfs(am_name, "counters", XAM_TOTAL_DEBUG_COUNTERS_PER_SLOT, device);

    return val_buf;
  }

};


/* ASM counter values 
 * In PCIe Linux, access the sysfs file for ASM to retrieve the ASM counter values
 */ 
struct asm_counter
{
  using result_type = query::asm_counter::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = boost::any_cast<query::asm_counter::debug_ip_data_type>(dbg_ip_dt);

    std::string asm_name("axistream_mon_");
    asm_name += std::to_string(dbg_ip_data->m_base_address);

    result_type val_buf = get_counter_status_from_sysfs(asm_name, "counters", XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT, device);

    return val_buf;
  }
};


/* LAPC status 
 * In PCIe Linux, access the sysfs file for LAPC to retrieve the LAPC status 
 */ 
struct lapc_status
{
  using result_type = query::lapc_status::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = boost::any_cast<query::lapc_status::debug_ip_data_type>(dbg_ip_dt);

    std::string lapc_name("lapc_");
    lapc_name += std::to_string(dbg_ip_data->m_base_address);

    std::vector<uint64_t> val_buf = get_counter_status_from_sysfs(lapc_name, "status", XLAPC_STATUS_PER_SLOT, device);

    result_type ret_val;
    for(auto& e: val_buf) {
      ret_val.push_back(static_cast<uint32_t>(e));
    }

    return ret_val;
  }
};


/* SPC status 
 * In PCIe Linux, access the sysfs file for SPC to retrieve the SPC status
 */ 
struct spc_status
{
  using result_type = query::spc_status::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = boost::any_cast<query::spc_status::debug_ip_data_type>(dbg_ip_dt);

    std::string spc_name("spc_");
    spc_name += std::to_string(dbg_ip_data->m_base_address);

    std::vector<uint64_t> val_buf = get_counter_status_from_sysfs(spc_name, "status", XSPC_STATUS_PER_SLOT, device);

    result_type ret_val;
    for(auto& e: val_buf) {
      ret_val.push_back(static_cast<uint32_t>(e));
    }

    return ret_val;
  }
};


/* Accelerator Deadlock Detector status 
 * In PCIe Linux, access the sysfs file for Accelerator Deadlock Detector to retrieve the deadlock status
 */ 
struct accel_deadlock_status
{
  using result_type = query::accel_deadlock_status::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const boost::any& dbg_ip_dt)
  {
    const auto dbg_ip_data = boost::any_cast<query::accel_deadlock_status::debug_ip_data_type>(dbg_ip_dt);

    std::string mon_name("accel_deadlock_");
    mon_name += std::to_string(dbg_ip_data->m_base_address);

    std::vector<uint64_t> val_buf = get_counter_status_from_sysfs(mon_name, "status", 1, device);

    result_type ret_val = val_buf[0];

    return ret_val;
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
      throw xrt_core::query::sysfs_error(err);

    return value;
  }

  static void
  put(const pdev& dev, const char* subdev, const char* entry, ValueType value)
  {
    std::string err;
    dev->sysfs_put(subdev, entry, err, value);
    if (!err.empty())
      throw xrt_core::query::sysfs_error(err);
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
      throw xrt_core::query::sysfs_error(err);

    return value;
  }

  static void
  put(const pdev& dev, const char* subdev, const char* entry, const ValueType& value)
  {
    std::string err;
    dev->sysfs_put(subdev, entry, err, value);
    if (!err.empty())
      throw xrt_core::query::sysfs_error(err);
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
      throw xrt_core::query::sysfs_error(err);

    return value;
  }

  static void 
  put(const pdev& dev, const char* subdev, const char* entry, const ValueType& value)
  {
    std::string err;
    dev->sysfs_put(subdev, entry, err, value);
    if (!err.empty())
      throw xrt_core::query::sysfs_error(err);
  }
};

template <typename QueryRequestType>
struct sysfs_get : virtual QueryRequestType
{
  const char* subdev;
  const char* entry;

  sysfs_get(const char* s, const char* e)
    : subdev(s), entry(e)
  {}

  boost::any
  get(const xrt_core::device* device) const
  {
    return sysfs_fcn<typename QueryRequestType::result_type>
      ::get(get_pcidev(device), subdev, entry);
  }

  boost::any
  get(const xrt_core::device* device, query::request::modifier m, const std::string& v) const
  {
    auto ms = (m == query::request::modifier::subdev) ? v.c_str() : subdev;
    auto me = (m == query::request::modifier::entry) ? v.c_str() : entry;
    return sysfs_fcn<typename QueryRequestType::result_type>
      ::get(get_pcidev(device), ms, me);
  }
};

template <typename QueryRequestType>
struct sysfs_put : virtual QueryRequestType
{
  const char* subdev;
  const char* entry;

  sysfs_put(const char* s, const char* e)
    : subdev(s), entry(e)
  {}

  void
  put(const xrt_core::device* device, const boost::any& any) const
  {
    auto value = boost::any_cast<typename QueryRequestType::value_type>(any);
    sysfs_fcn<typename QueryRequestType::value_type>
      ::put(get_pcidev(device), this->subdev, this->entry, value);
  }
};

template <typename QueryRequestType>
struct sysfs_getput : sysfs_get<QueryRequestType>, sysfs_put<QueryRequestType>
{
  sysfs_getput(const char* s, const char* e)
    : sysfs_get<QueryRequestType>(s, e), sysfs_put<QueryRequestType>(s, e)
  {}
};

template <typename QueryRequestType, typename Getter>
struct function0_get : virtual QueryRequestType
{
  boost::any
  get(const xrt_core::device* device) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k);
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

static std::map<xrt_core::query::key_type, std::unique_ptr<query::request>> query_tbl;

template <typename QueryRequestType>
static void
emplace_sysfs_get(const char* subdev, const char* entry)
{
  auto x = QueryRequestType::key;
  query_tbl.emplace(x, std::make_unique<sysfs_get<QueryRequestType>>(subdev, entry));
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
emplace_func4_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function4_get<QueryRequestType, Getter>>());
}

template <typename QueryRequestType>
static void
emplace_sysfs_put(const char* subdev, const char* entry)
{
  auto x = QueryRequestType::key;
  query_tbl.emplace(x, std::make_unique<sysfs_put<QueryRequestType>>(subdev, entry));
}

template <typename QueryRequestType>
static void
emplace_sysfs_getput(const char* subdev, const char* entry)
{
  auto x = QueryRequestType::key;
  query_tbl.emplace(x, std::make_unique<sysfs_getput<QueryRequestType>>(subdev, entry));
}

static void
initialize_query_table()
{
  emplace_sysfs_get<query::pcie_vendor>                        ("", "vendor");
  emplace_sysfs_get<query::pcie_device>                        ("", "device");
  emplace_sysfs_get<query::pcie_subsystem_vendor>              ("", "subsystem_vendor");
  emplace_sysfs_get<query::pcie_subsystem_id>                  ("", "subsystem_device");
  emplace_sysfs_get<query::pcie_link_speed>                    ("", "link_speed");
  emplace_sysfs_get<query::pcie_link_speed_max>                ("", "link_speed_max");
  emplace_sysfs_get<query::pcie_express_lane_width>            ("", "link_width");
  emplace_sysfs_get<query::pcie_express_lane_width_max>        ("", "link_width_max");
  emplace_sysfs_get<query::dma_threads_raw>                    ("dma", "channel_stat_raw");
  emplace_sysfs_get<query::rom_vbnv>                           ("rom", "VBNV");
  emplace_sysfs_get<query::rom_ddr_bank_size_gb>               ("rom", "ddr_bank_size");
  emplace_sysfs_get<query::rom_ddr_bank_count_max>             ("rom", "ddr_bank_count_max");
  emplace_sysfs_get<query::rom_fpga_name>                      ("rom", "FPGA");
  emplace_sysfs_get<query::rom_raw>                            ("rom", "raw");
  emplace_sysfs_get<query::rom_uuid>                           ("rom", "uuid");
  emplace_sysfs_get<query::rom_time_since_epoch>               ("rom", "timestamp");
  emplace_sysfs_get<query::xclbin_uuid>                        ("", "xclbinuuid");
  emplace_sysfs_getput<query::ic_enable>                       ("icap_controller", "enable");
  emplace_sysfs_getput<query::ic_load_flash_address>           ("icap_controller", "load_flash_addr");
  emplace_sysfs_get<query::memstat>                            ("", "memstat");
  emplace_sysfs_get<query::memstat_raw>                        ("", "memstat_raw");
  emplace_sysfs_get<query::mem_topology_raw>                   ("icap", "mem_topology");
  emplace_sysfs_get<query::dma_stream>                         ("dma", "");
  emplace_sysfs_get<query::group_topology>                     ("icap", "group_topology");
  emplace_sysfs_get<query::ip_layout_raw>                      ("icap", "ip_layout");
  emplace_sysfs_get<query::debug_ip_layout_raw>                ("icap", "debug_ip_layout");
  emplace_sysfs_get<query::clock_freq_topology_raw>            ("icap", "clock_freq_topology");
  emplace_sysfs_get<query::clock_freqs_mhz>                    ("icap", "clock_freqs");
  emplace_sysfs_get<query::idcode>                             ("icap", "idcode");
  emplace_sysfs_getput<query::data_retention>                  ("icap", "data_retention");
  emplace_sysfs_getput<query::sec_level>                       ("icap", "sec_level");
  emplace_sysfs_get<query::max_shared_host_mem_aperture_bytes> ("icap", "max_host_mem_aperture");
  emplace_sysfs_get<query::status_mig_calibrated>              ("", "mig_calibration");
  emplace_sysfs_getput<query::mig_cache_update>                ("", "mig_cache_update");
  emplace_sysfs_get<query::temp_by_mem_topology>               ("xmc", "temp_by_mem_topology");
  emplace_sysfs_get<query::xmc_version>                        ("xmc", "version");
  emplace_sysfs_get<query::xmc_board_name>                     ("xmc", "bd_name");
  emplace_sysfs_get<query::xmc_serial_num>                     ("xmc", "serial_num");
  emplace_sysfs_get<query::max_power_level>                    ("xmc", "max_power");
  emplace_sysfs_get<query::xmc_sc_presence>                    ("xmc", "sc_presence");
  emplace_sysfs_get<query::is_sc_fixed>                        ("xmc", "sc_is_fixed");
  emplace_sysfs_get<query::xmc_sc_version>                     ("xmc", "bmc_ver");
  emplace_sysfs_get<query::expected_sc_version>                ("xmc", "exp_bmc_ver");
  emplace_sysfs_get<query::xmc_status>                         ("xmc", "status");
  emplace_sysfs_get<query::xmc_reg_base>                       ("xmc", "reg_base");
  emplace_sysfs_get<query::xmc_scaling_support>                ("xmc", "scaling_support");
  emplace_sysfs_getput<query::xmc_scaling_enabled>             ("xmc", "scaling_enabled");
  emplace_sysfs_get<query::xmc_scaling_critical_pow_threshold> ("xmc", "scaling_critical_power_threshold");
  emplace_sysfs_get<query::xmc_scaling_critical_temp_threshold>("xmc", "scaling_critical_temp_threshold");
  emplace_sysfs_get<query::xmc_scaling_threshold_power_limit>  ("xmc", "scaling_threshold_power_limit");
  emplace_sysfs_get<query::xmc_scaling_threshold_temp_limit>   ("xmc", "scaling_threshold_temp_limit");
  emplace_sysfs_get<query::xmc_scaling_power_override_enable>  ("xmc", "scaling_threshold_power_override_en");
  emplace_sysfs_get<query::xmc_scaling_temp_override_enable>   ("xmc", "scaling_threshold_temp_override_en");
  emplace_sysfs_getput<query::xmc_scaling_power_override>      ("xmc", "scaling_threshold_power_override");
  emplace_sysfs_getput<query::xmc_scaling_temp_override>       ("xmc", "scaling_threshold_temp_override");
  emplace_sysfs_put<query::xmc_scaling_reset>                  ("xmc", "scaling_reset");
  emplace_sysfs_get<query::m2m>                                ("m2m", "");
  emplace_sysfs_get<query::nodma>                              ("", "nodma");
  emplace_sysfs_get<query::dna_serial_num>                     ("dna", "dna");
  emplace_sysfs_get<query::p2p_config>                         ("p2p", "config");
  emplace_sysfs_get<query::temp_card_top_front>                ("xmc", "xmc_se98_temp0");
  emplace_sysfs_get<query::temp_card_top_rear>                 ("xmc", "xmc_se98_temp1");
  emplace_sysfs_get<query::temp_card_bottom_front>             ("xmc", "xmc_se98_temp2");
  emplace_sysfs_get<query::temp_fpga>                          ("xmc", "xmc_fpga_temp");
  emplace_sysfs_get<query::fan_trigger_critical_temp>          ("xmc", "xmc_fan_temp");
  emplace_sysfs_get<query::fan_fan_presence>                   ("xmc", "fan_presence");
  emplace_sysfs_get<query::fan_speed_rpm>                      ("xmc", "xmc_fan_rpm");
  emplace_sysfs_get<query::ddr_temp_0>                         ("xmc", "xmc_ddr_temp0");
  emplace_sysfs_get<query::ddr_temp_1>                         ("xmc", "xmc_ddr_temp1");
  emplace_sysfs_get<query::ddr_temp_2>                         ("xmc", "xmc_ddr_temp2");
  emplace_sysfs_get<query::ddr_temp_3>                         ("xmc", "xmc_ddr_temp3");
  emplace_sysfs_get<query::hbm_temp>                           ("xmc", "xmc_hbm_temp");
  emplace_sysfs_get<query::cage_temp_0>                        ("xmc", "xmc_cage_temp0");
  emplace_sysfs_get<query::cage_temp_1>                        ("xmc", "xmc_cage_temp1");
  emplace_sysfs_get<query::cage_temp_2>                        ("xmc", "xmc_cage_temp2");
  emplace_sysfs_get<query::cage_temp_3>                        ("xmc", "xmc_cage_temp3");
  emplace_sysfs_get<query::v12v_pex_millivolts>                ("xmc", "xmc_12v_pex_vol");
  emplace_sysfs_get<query::v12v_pex_milliamps>                 ("xmc", "xmc_12v_pex_curr");
  emplace_sysfs_get<query::v12v_aux_millivolts>                ("xmc", "xmc_12v_aux_vol");
  emplace_sysfs_get<query::v12v_aux_milliamps>                 ("xmc", "xmc_12v_aux_curr");
  emplace_sysfs_get<query::v3v3_pex_millivolts>                ("xmc", "xmc_3v3_pex_vol");
  emplace_sysfs_get<query::v3v3_aux_millivolts>                ("xmc", "xmc_3v3_aux_vol");
  emplace_sysfs_get<query::v3v3_aux_milliamps>                 ("xmc", "xmc_3v3_aux_cur");
  emplace_sysfs_get<query::ddr_vpp_bottom_millivolts>          ("xmc", "xmc_ddr_vpp_btm");
  emplace_sysfs_get<query::ddr_vpp_top_millivolts>             ("xmc", "xmc_ddr_vpp_top");

  emplace_sysfs_get<query::v5v5_system_millivolts>             ("xmc", "xmc_sys_5v5");
  emplace_sysfs_get<query::v1v2_vcc_top_millivolts>            ("xmc", "xmc_1v2_top");
  emplace_sysfs_get<query::v1v2_vcc_bottom_millivolts>         ("xmc", "xmc_vcc1v2_btm");
  emplace_sysfs_get<query::v1v8_millivolts>                    ("xmc", "xmc_1v8");
  emplace_sysfs_get<query::v0v85_millivolts>                   ("xmc", "xmc_0v85");
  emplace_sysfs_get<query::v0v9_vcc_millivolts>                ("xmc", "xmc_mgt0v9avcc");
  emplace_sysfs_get<query::v12v_sw_millivolts>                 ("xmc", "xmc_12v_sw");
  emplace_sysfs_get<query::mgt_vtt_millivolts>                 ("xmc", "xmc_mgtavtt");
  emplace_sysfs_get<query::int_vcc_millivolts>                 ("xmc", "xmc_vccint_vol");
  emplace_sysfs_get<query::int_vcc_milliamps>                  ("xmc", "xmc_vccint_curr");
  emplace_sysfs_get<query::int_vcc_temp>                       ("xmc", "xmc_vccint_temp");

  emplace_sysfs_get<query::v12_aux1_millivolts>                ("xmc", "xmc_12v_aux1");
  emplace_sysfs_get<query::vcc1v2_i_milliamps>                 ("xmc", "xmc_vcc1v2_i");
  emplace_sysfs_get<query::v12_in_i_milliamps>                 ("xmc", "xmc_v12_in_i");
  emplace_sysfs_get<query::v12_in_aux0_i_milliamps>            ("xmc", "xmc_v12_in_aux0_i");
  emplace_sysfs_get<query::v12_in_aux1_i_milliamps>            ("xmc", "xmc_v12_in_aux1_i");
  emplace_sysfs_get<query::vcc_aux_millivolts>                 ("xmc", "xmc_vccaux");
  emplace_sysfs_get<query::vcc_aux_pmc_millivolts>             ("xmc", "xmc_vccaux_pmc");
  emplace_sysfs_get<query::vcc_ram_millivolts>                 ("xmc", "xmc_vccram");

  emplace_sysfs_get<query::v3v3_pex_milliamps>                 ("xmc", "xmc_3v3_pex_curr");
  emplace_sysfs_get<query::v3v3_aux_milliamps>                 ("xmc", "xmc_3v3_aux_cur");
  emplace_sysfs_get<query::int_vcc_io_milliamps>               ("xmc", "xmc_0v85_curr");
  emplace_sysfs_get<query::v3v3_vcc_millivolts>                ("xmc", "xmc_3v3_vcc_vol");
  emplace_sysfs_get<query::hbm_1v2_millivolts>                 ("xmc", "xmc_hbm_1v2_vol");
  emplace_sysfs_get<query::v2v5_vpp_millivolts>                ("xmc", "xmc_vpp2v5_vol");
  emplace_sysfs_get<query::int_vcc_io_millivolts>              ("xmc", "xmc_vccint_bram_vol");
  emplace_sysfs_get<query::v0v9_int_vcc_vcu_millivolts>        ("xmc", "xmc_vccint_vcu_0v9");
  emplace_sysfs_get<query::mac_contiguous_num>                 ("xmc", "mac_contiguous_num");
  emplace_sysfs_get<query::mac_addr_first>                     ("xmc", "mac_addr_first");
  emplace_sysfs_get<query::oem_id>                             ("xmc", "xmc_oem_id");
  emplace_sysfs_get<query::heartbeat_count>                    ("xmc", "xmc_heartbeat_count");
  emplace_sysfs_get<query::heartbeat_err_code>                 ("xmc", "xmc_heartbeat_err_code");
  emplace_sysfs_get<query::heartbeat_err_time>                 ("xmc", "xmc_heartbeat_err_time");
  emplace_sysfs_get<query::heartbeat_stall>                    ("xmc", "xmc_heartbeat_stall");
  emplace_func0_request<query::xmc_qspi_status,                qspi_status>();
  emplace_func0_request<query::mac_addr_list,                  mac_addr_list>();

  emplace_sysfs_get<query::firewall_detect_level>              ("firewall", "detected_level");
  emplace_sysfs_get<query::firewall_detect_level_name>         ("firewall", "detected_level_name");
  emplace_sysfs_get<query::firewall_status>                    ("firewall", "detected_status");
  emplace_sysfs_get<query::firewall_time_sec>                  ("firewall", "detected_time");

  emplace_sysfs_get<query::power_microwatts>                   ("xmc", "xmc_power");
  emplace_sysfs_get<query::power_warning>                      ("xmc", "xmc_power_warn");
  emplace_sysfs_get<query::host_mem_size>                      ("address_translator", "host_mem_size");

  emplace_sysfs_get<query::mig_ecc_status>                     ("mig", "ecc_status");
  emplace_sysfs_get<query::mig_ecc_ce_cnt>                     ("mig", "ecc_ce_cnt");
  emplace_sysfs_get<query::mig_ecc_ue_cnt>                     ("mig", "ecc_ue_cnt");
  emplace_sysfs_get<query::mig_ecc_ce_ffa>                     ("mig", "ecc_ce_ffa");
  emplace_sysfs_get<query::mig_ecc_ue_ffa>                     ("mig", "ecc_ue_ffa");
  emplace_sysfs_get<query::flash_bar_offset>                   ("flash", "bar_off");
  emplace_sysfs_get<query::is_mfg>                             ("", "mfg");
  emplace_sysfs_get<query::mfg_ver>                            ("", "mfg_ver");
  emplace_sysfs_get<query::is_recovery>                        ("", "recovery");
  emplace_sysfs_get<query::is_ready>                           ("", "ready");
  emplace_sysfs_get<query::is_offline>                         ("", "dev_offline");
  emplace_sysfs_get<query::f_flash_type>                       ("flash", "flash_type");
  emplace_sysfs_get<query::flash_type>                         ("", "flash_type");
  emplace_sysfs_get<query::board_name>                         ("", "board_name");
  emplace_sysfs_get<query::logic_uuids>                        ("", "logic_uuids");
  emplace_sysfs_get<query::interface_uuids>                    ("", "interface_uuids");
  emplace_sysfs_getput<query::rp_program_status>               ("", "rp_program");
  emplace_sysfs_get<query::shared_host_mem>                    ("", "host_mem_size");
  emplace_sysfs_get<query::enabled_host_mem>                   ("address_translator", "host_mem_size");
  emplace_sysfs_get<query::cpu_affinity>                       ("", "local_cpulist");
  emplace_sysfs_get<query::mailbox_metrics>                    ("mailbox", "recv_metrics");
  emplace_sysfs_get<query::clock_timestamp>                    ("ert_ctrl", "clock_timestamp");
  emplace_sysfs_getput<query::ert_sleep>                       ("ert_ctrl", "mb_sleep");
  emplace_sysfs_get<query::ert_cq_read>                        ("ert_ctrl", "cq_read_cnt");
  emplace_sysfs_get<query::ert_cq_write>                       ("ert_ctrl", "cq_write_cnt");
  emplace_sysfs_get<query::ert_cu_read>                        ("ert_ctrl", "cu_read_cnt");
  emplace_sysfs_get<query::ert_cu_write>                       ("ert_ctrl", "cu_write_cnt");
  emplace_sysfs_get<query::ert_data_integrity>                 ("ert_ctrl", "data_integrity");
  emplace_sysfs_getput<query::config_mailbox_channel_disable>  ("", "config_mailbox_channel_disable");
  emplace_sysfs_getput<query::config_mailbox_channel_switch>   ("", "config_mailbox_channel_switch");
  emplace_sysfs_getput<query::config_xclbin_change>            ("", "config_xclbin_change");
  emplace_sysfs_getput<query::cache_xclbin>                    ("", "cache_xclbin");

  emplace_sysfs_get<query::kds_numcdmas>                       ("", "kds_numcdmas");
  emplace_func0_request<query::kds_cu_info,                    kds_cu_info>();
  emplace_func0_request<query::kds_scu_info,                   kds_scu_info>();
  emplace_sysfs_get<query::ps_kernel>                          ("icap", "ps_kernel");
  emplace_sysfs_get<query::xocl_errors>                        ("", "xocl_errors");

  emplace_func0_request<query::pcie_bdf,                       bdf>();
  emplace_func0_request<query::kds_cu_info,                    kds_cu_info>();
  emplace_func0_request<query::instance,                       instance>();

  emplace_func4_request<query::aim_counter,                    aim_counter>();
  emplace_func4_request<query::am_counter,                     am_counter>();
  emplace_func4_request<query::asm_counter,                    asm_counter>();
  emplace_func4_request<query::lapc_status,                    lapc_status>();
  emplace_func4_request<query::spc_status,                     spc_status>();
  emplace_func4_request<query::accel_deadlock_status,          accel_deadlock_status>();
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
reset(query::reset_type& key) const 
{
  std::string err;
  pcidev::get_dev(get_device_id(), false)->sysfs_put(key.get_subdev(), key.get_entry(), err, key.get_value());
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

void
device_linux::
xclmgmt_load_xclbin(const char* buffer) const {
  //resolves to xclbin2
  const char xclbin_magic_str[] = { 0x78, 0x63, 0x6c, 0x62, 0x69, 0x6e, 0x32 };
  if (sizeof(buffer) < sizeof(xclbin_magic_str))
    throw xrt_core::error("Xclbin is smaller than expected");
  if (std::memcmp(buffer, xclbin_magic_str, sizeof(xclbin_magic_str)) != 0)
    throw xrt_core::error(boost::str(boost::format("Bad binary version '%s'") % xclbin_magic_str));
  int ret = 0;
  try {
    xrt_core::scope_value_guard<int, std::function<void()>> fd = file_open("", O_RDWR);
    xclmgmt_ioc_bitstream_axlf obj = { reinterpret_cast<axlf *>( const_cast<char*>(buffer) ) };
    ret = pcidev::get_dev(get_device_id(), false)->ioctl(fd.get(), XCLMGMT_IOCICAPDOWNLOAD_AXLF, &obj);
  } catch (const std::exception& e) {
    xrt_core::send_exception_message(e.what(), "Failed to open device");
  }

  if(ret != 0) {
    throw error(ret, "Failed to download xclbin");
  }
}

////////////////////////////////////////////////////////////////
// Custom ishim implementation
// Redefined from xrt_core::ishim for functions that are not
// universally implemented by all shims
////////////////////////////////////////////////////////////////
// User Managed IP Interrupt Handling
xclInterruptNotifyHandle
device_linux::
open_ip_interrupt_notify(unsigned int ip_index)
{
  return xclOpenIPInterruptNotify(get_device_handle(), ip_index, 0);
}
  
void
device_linux::
close_ip_interrupt_notify(xclInterruptNotifyHandle handle)
{
  xclCloseIPInterruptNotify(get_device_handle(), handle);
}

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
  int disable = 0;
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

xclBufferHandle
device_linux::
import_bo(pid_t pid, xclBufferExportHandle ehdl)
{
  if (getpid() == pid)
    return shim::import_bo(ehdl);

#if defined(SYS_pidfd_open) && defined(SYS_pidfd_getfd)
  auto pidfd = syscall(SYS_pidfd_open, pid, 0);
  if (pidfd < 0)
    throw xrt_core::system_error(errno, "pidfd_open failed");

  auto bofd = syscall(SYS_pidfd_getfd, pidfd, ehdl, 0);
  if (bofd < 0)
    throw xrt_core::system_error
      (errno, "pidfd_getfd failed, check that ptrace access mode "
       "allows PTRACE_MODE_ATTACH_REALCREDS.  For more details please "
       "check /etc/sysctl.d/10-ptrace.conf");

  return shim::import_bo(bofd);
#else
  throw xrt_core::system_error
    (std::errc::not_supported,
     "Importing buffer object from different process requires XRT "
     " built and installed on a system with 'pidfd' kernel support");
#endif
}

} // xrt_core
