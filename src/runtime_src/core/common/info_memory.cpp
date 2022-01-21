/**
 * Copyright (C) 2021 Xilinx, Inc
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
#define XRT_CORE_COMMON_SOURCE
#include "info_memory.h"
#include "query_requests.h"
#include "xclbin.h"
#include "ps_kernel.h"
#include "utils.h"

#include <boost/algorithm/string.hpp>

// Too much typing
using ptree_type = boost::property_tree::ptree;
namespace xq = xrt_core::query;

namespace {

// memtype2str() - Convert MEM_TYPE to readable string
static std::string
memtype2str(MEM_TYPE mt)
{
  static const std::map<MEM_TYPE, std::string> memtype_map =
  {
   {MEM_DDR3,                 "MEM_DDR3"},
   {MEM_DDR4,                 "MEM_DDR4"},
   {MEM_DRAM,                 "MEM_DRAM"},
   {MEM_STREAMING,            "MEM_STREAMING"},
   {MEM_PREALLOCATED_GLOB,    "MEM_PREALLOCATED_GLOB"},
   {MEM_ARE,                  "MEM_ARE"},
   {MEM_HBM,                  "MEM_HBM"},
   {MEM_BRAM,                 "MEM_BRAM"},
   {MEM_URAM,                 "MEM_URAM"},
   {MEM_STREAMING_CONNECTION, "MEM_STREAMING_CONNECTION"}
  };

  auto itr = memtype_map.find(mt);
  if (itr != memtype_map.end())
    return (*itr).second;

  throw xrt_core::error("Invalid memtype");
}

// memtype2str() - Convert xclbin::mem_data::m_type to string
inline std::string
memtype2str(decltype(mem_data::m_type) mt)
{
  return memtype2str(static_cast<MEM_TYPE>(mt));
}

// ecc_status2str - Convert ECC status to readable string
static std::string
ecc_status2str(uint64_t status)
{
  constexpr int ce_mask = 0b10;  // correctable error mask
  constexpr int ue_mask = 0b1;   // uncorrectable error mask

  // If unknown status bits, can't support.
  if (!status || (status & ~(ce_mask | ue_mask)))
    // Bad ECC status detected!
    throw xrt_core::error("Bad ECC status detected");

  std::string str;

  if (status & ue_mask)
    str += "UE ";
  if (status & ce_mask)
    str += "CE ";

  return str;
}

struct memory_info_collector
{
  const xrt_core::device* device;          // device to query for info

  const std::vector<char> mem_topo_raw;    // xclbin raw mem topology
  const std::vector<char> grp_topo_raw;    // xclbin raw grp topology
  std::vector<char> mem_temp_raw;    // xclbin temperator raw data

  const mem_topology* mem_topo = nullptr;  // xclbin mem topology from device
  const mem_topology* grp_topo = nullptr;  // xclbin group topology from device
  const std::vector<std::string> mem_stat; // raw memory stat from device
  const uint32_t* mem_temp = nullptr;      // temperature stat from device

  // Get topology index of a mem_data element
  static decltype(mem_topology::m_count)
  get_mem_data_index(const mem_topology* mt, const mem_data* mem)
  {
    auto idx = std::distance(mt->m_mem_data, mem);
    if (idx >= 0 && idx < mt->m_count)
      return static_cast<decltype(mem_topology::m_count)>(idx);

    throw xrt_core::internal_error("add_temp_mem_info: invalid mem_data entry");
  }

  // Add bytes transferred by each PCIe channel to tree
  void
  add_channel_info(ptree_type& pt)
  {
    ptree_type pt_dma_array;
    try {
      // list of "c2h h2c" strings representing bytes in either direction
      auto dma_threads = xrt_core::device_query<xq::dma_threads_raw>(device);
      for (size_t i = 0; i < dma_threads.size(); ++i) {
        ptree_type pt_dma;
        uint64_t c2h = 0, h2c = 0;
        std::stringstream {dma_threads[i]} >> c2h >> h2c;
        pt_dma.put("channel_id", i);
        pt_dma.put("host_to_card_bytes", boost::format("0x%x") % h2c);
        pt_dma.put("card_to_host_bytes", boost::format("0x%x") % c2h);
        pt_dma_array.push_back(std::make_pair("",pt_dma));
      }
    }
    catch (const xq::exception& ex) {
      pt.put("error_msg", ex.what());
    }

    // append potentially empty pt_dma_array, why?
    pt.put("board.direct_memory_accesses.type", "pcie xdma");
    pt.add_child("board.direct_memory_accesses.metrics", pt_dma_array);
  }

  // Update MIG cache
  void
  update_mig_cache(ptree_type& pt)
  {
    try {
      xrt_core::device_query<xq::mig_cache_update>(device);
    }
    catch (const xq::exception& ex) {
      pt.put("error_msg", ex.what());
    }
  }

  // Append info from a mem topology streaming entry
  // Pre-cond: mem is a streaming entry
  void
  add_stream_info(const mem_data* mem, ptree_type& pt_stream_array)
  {
    ptree_type pt_stream;

    try {
      pt_stream.put("tag", mem->m_tag);

      // dma_stream sysfs entry name depends on write or read stream
      // which is indicated by trailing 'w' or 'r' in tag name
      std::string lname {reinterpret_cast<const char*>(mem->m_tag)};
      if (lname.back() == 'w')
        lname = "route" + std::to_string(mem->route_id) + "/stat";
      else if (lname.back() == 'r')
        lname = "flow" + std::to_string(mem->flow_id) + "/stat";

      // list of "???" strings presenting what?
      auto stream_stat = xrt_core::device_query<xq::dma_stream>(device, xq::request::modifier::entry, lname);

      // what is being parsed here?
      std::map<std::string, std::string> stat_map;
      for (const auto& str : stream_stat) {
        std::vector<std::string> strs;
        boost::split(strs, str, boost::is_any_of(":"));
        if (strs.size() > 1)
          stat_map[strs[0]] = strs[1];
      }

      // absolute magic without knowing what was parsed above
      auto total = stat_map["complete_bytes"] + "/" + stat_map["complete_requests"];
      auto pending = stat_map["pending_bytes"] + "/" + stat_map["pending_requests"];

      pt_stream.put("usage.status", "Active");
      pt_stream.put("usage.total", total);
      pt_stream.put("usage.pending", pending);
    }
    catch (const xq::exception&) {
      // eat the exception, probably bad path
    }

    pt_stream_array.push_back(std::make_pair("",pt_stream));
  }

  // Add info from all streaming entries in mem topology
  void
  add_streaming_info(ptree_type& pt)
  {
    ptree_type pt_stream_array;

    for (int i = 0; i < mem_topo->m_count; ++i) {
      const auto& mem = mem_topo->m_mem_data[i];
      if (mem.m_type == MEM_STREAMING || mem.m_type == MEM_STREAMING_CONNECTION)
        add_stream_info(&mem, pt_stream_array);
    }

    pt.add_child("board.memory.data_streams", pt_stream_array);
  }

  // Add ecc info for specified mem entry
  void
  add_mem_ecc_info(const mem_data* mem, ptree_type& pt_mem)
  {
    if (!mem->m_used)
      return;

    try {
      std::string tag(reinterpret_cast<const char*>(mem->m_tag));
      auto ecc_st = xrt_core::device_query<xq::mig_ecc_status>(device, xq::request::modifier::subdev, tag);
      auto ce_cnt = xrt_core::device_query<xq::mig_ecc_ce_cnt>(device, xq::request::modifier::subdev, tag);
      auto ue_cnt = xrt_core::device_query<xq::mig_ecc_ue_cnt>(device, xq::request::modifier::subdev, tag);
      auto ce_ffa = xrt_core::device_query<xq::mig_ecc_ce_ffa>(device, xq::request::modifier::subdev, tag);
      auto ue_ffa = xrt_core::device_query<xq::mig_ecc_ue_ffa>(device, xq::request::modifier::subdev, tag);

      pt_mem.put("extended_info.ecc.status", ecc_status2str(ecc_st));
      pt_mem.put("extended_info.ecc.error.correctable.count", ce_cnt);
      pt_mem.put("extended_info.ecc.error.correctable.first_failure_address", boost::format("0x%x") % ce_ffa);
      pt_mem.put("extended_info.ecc.error.uncorrectable.count", ue_cnt);
      pt_mem.put("extended_info.ecc.error.uncorrectable.first_failure_address", boost::format("0x%x") % ue_ffa);
    }
    catch (const xq::exception& ex) {
      pt_mem.put("error_msg", ex.what());
    }
    catch (const std::exception&) {
      // Error from ecc_status2str, not sure why that is ignored?
    }
  }

  // Add general mem info for specified mem entry
  static void
  add_mem_general_info(const mem_data* mem, ptree_type& pt_mem)
  {
    pt_mem.put("type", memtype2str(mem->m_type));
    pt_mem.put("tag", mem->m_tag);
    pt_mem.put("enabled", mem->m_used ? true : false);
    pt_mem.put("base_address", boost::format("0x%x") % mem->m_base_address);
    pt_mem.put("range_bytes", boost::format("0x%x") % (mem->m_size << 10)); // magic 10
  }

  // Add mem usage info for specified mem entry.
  // This function is shared with group topology, hence need to
  // know where the mem entry is comining from
  void
  add_mem_usage_info(const mem_topology* mtopo, const mem_data* mem, ptree_type& pt_mem)
  {
    auto idx = get_mem_data_index(mtopo, mem);
    uint64_t memory_usage = 0, bo_count = 0;
    std::stringstream {mem_stat[idx]} >> memory_usage >> bo_count; // idx has been validated
    pt_mem.put("extended_info.usage.allocated_bytes", memory_usage);
    pt_mem.put("extended_info.usage.buffer_objects_count", bo_count);
  }

  // Add mem temperature info for specified mem entry
  void
  add_mem_temp_info(const mem_data* mem, ptree_type& pt_mem)
  {
    auto idx = get_mem_data_index(mem_topo, mem);

    // temperature is guaranteed to match up with mem_topo entries
    // indexing is safe because idx is validated
    constexpr int invalid_sensor_value = 0;
    if (mem_temp && mem_temp[idx] != invalid_sensor_value)
      pt_mem.put("extended_info.temperature_C", mem_temp[idx]);
  }

  // Add mem info for specified mem data entry
  void
  add_mem_info(const mem_data* mem, ptree_type& pt_mem_array)
  {
    ptree_type pt_mem;
    add_mem_ecc_info(mem, pt_mem);
    add_mem_general_info(mem, pt_mem);
    add_mem_usage_info(mem_topo, mem, pt_mem);
    add_mem_temp_info(mem, pt_mem);
    pt_mem_array.push_back(std::make_pair("",pt_mem));
  }

  // Add mem info for all mem entries in mem_topology section
  void
  add_mem_info(ptree_type& pt)
  {
    ptree_type pt_mem_array;

    for (int i = 0; i < mem_topo->m_count; ++i) {
      const auto& mem = mem_topo->m_mem_data[i];
      if (mem.m_type == MEM_STREAMING || mem.m_type == MEM_STREAMING_CONNECTION)
        continue;

      add_mem_info(&mem, pt_mem_array);
    }

    pt.add_child("board.memory.memories", pt_mem_array );
  }

  // Add mem info for specified mem_data entry in group topology section
  void
  add_grp_info(const mem_data* mem, ptree_type& pt_grp_array)
  {
    ptree_type pt_grp;
    add_mem_general_info(mem, pt_grp);
    add_mem_usage_info(grp_topo, mem, pt_grp);
    pt_grp_array.push_back(std::make_pair("",pt_grp));
  }

  // Add grp info for all mem entries in group_topology section
  void
  add_grp_info(ptree_type& pt)
  {
    if (!grp_topo)
      return;

    ptree_type pt_grp_array;

    // group_topology prepends all mem_topology entries so groups
    // are following at index mem_topo->m_count
    for (int i = mem_topo->m_count; i < grp_topo->m_count; i++) {
      const auto& mem = grp_topo->m_mem_data[i];
      add_grp_info(&mem, pt_grp_array);
    }

    if (!pt_grp_array.empty())
      pt.add_child("board.memory.memory_groups", pt_grp_array);
  }

public:
  explicit
  memory_info_collector(const xrt_core::device* dev)
    : device(dev)
    , mem_topo_raw(xrt_core::device_query<xq::mem_topology_raw>(device))
    , grp_topo_raw(xrt_core::device_query<xq::group_topology>(device))
    , mem_topo(mem_topo_raw.empty() ? nullptr : reinterpret_cast<const mem_topology*>(mem_topo_raw.data()))
    , grp_topo(grp_topo_raw.empty() ? nullptr : reinterpret_cast<const mem_topology*>(grp_topo_raw.data()))
    , mem_stat(xrt_core::device_query<xq::memstat_raw>(device))
    , mem_temp(0)
  {
    try {
      mem_temp_raw = xrt_core::device_query<xq::temp_by_mem_topology>(dev);
      mem_temp = mem_temp_raw.empty() ? nullptr : reinterpret_cast<const uint32_t*>(mem_temp_raw.data());
    }
    catch (const xq::exception&) {
      //ignore if xmc is not present 
    }
    // info gathering functions indexes mem_stat by mem_toplogy entry index
    if (mem_topo && mem_stat.size() < static_cast<size_t>(mem_topo->m_count))
      throw xrt_core::internal_error("incorrect memstat_raw entries");

    // info gathering functions indexes mem_temp by mem_topology entry index
    if (mem_topo && mem_temp && mem_temp_raw.size() < static_cast<size_t>(mem_topo->m_count))
      throw xrt_core::internal_error("incorrect temp_by_mem_topology entries");

    // info gathering functions indexes mem_stat by group_toplogy entry index
    if (grp_topo && mem_stat.size() < static_cast<size_t>(grp_topo->m_count))
      throw xrt_core::internal_error("incorrect temp_by_mem_topology entries");
  }

  void
  collect(ptree_type& pt)
  {
    if (!mem_topo)
      return;

    add_channel_info(pt);
    update_mig_cache(pt);  // why?
    add_streaming_info(pt);
    add_mem_info(pt);
    add_grp_info(pt);
  }
};


} //unnamed namespace

namespace xrt_core { namespace memory {

ptree_type
memory_topology(const xrt_core::device* device)
{
  ptree_type pt;

  try {
    memory_info_collector mic(device);
    mic.collect(pt);
  }
  catch (xq::exception& ex) {
    pt.put("error_msg", ex.what());
  }

  return pt;
}

ptree_type
xclbin_info(const xrt_core::device * device)
{
  ptree_type pt;

  try {
    auto uuid_str =  device->get_xclbin_uuid().to_string();
    boost::algorithm::to_upper(uuid_str);
    pt.put("xclbin_uuid", uuid_str);
  }
  catch (const xq::exception& ex) {
    pt.put("error_msg", ex.what());
  }

  return pt;
}

enum class cu_type {
  PL,
  PS
};

static std::string 
enum_to_str(cu_type type) {
  switch(type) {
    case cu_type::PL:
      return "PL";
    case cu_type::PS:
      return "PS";
    default:
      break;
  }
  return "UNKNOWN";
}

boost::property_tree::ptree
get_cu_status(uint32_t cu_status)
{
  boost::property_tree::ptree pt;
  std::vector<std::string> bit_set;
  if (cu_status & 0x1)
    bit_set.push_back("START");
  if (cu_status & 0x2)
    bit_set.push_back("DONE");
  if (cu_status & 0x4)
    bit_set.push_back("IDLE");
  if (cu_status & 0x8)
    bit_set.push_back("READY");
  if (cu_status & 0x10)
    bit_set.push_back("RESTART");

  pt.put("bit_mask",	boost::str(boost::format("0x%x") % cu_status));
  boost::property_tree::ptree ptSt_arr;
  for(auto& str : bit_set)
    ptSt_arr.push_back(std::make_pair("", boost::property_tree::ptree(str)));

  if (!ptSt_arr.empty())
    pt.add_child( std::string("bits_set"), ptSt_arr);

  return pt;
}

static void
schedulerUpdateStat(xrt_core::device *device)
{
  try {
    // lock xclbin
    std::string xclbin_uuid = xrt_core::device_query<xq::xclbin_uuid>(device);
    // dont open a context if xclbin_uuid is empty
    if(xclbin_uuid.empty())
	    return;
    auto uuid = xrt::uuid(xclbin_uuid);
    device->open_context(uuid.get(), std::numeric_limits<unsigned int>::max(), true);
    auto at_exit = [] (auto device, auto uuid) { device->close_context(uuid.get(), std::numeric_limits<unsigned int>::max()); };
    xrt_core::scope_guard<std::function<void()>> g(std::bind(at_exit, device, uuid));

    device->update_scheduler_status();
  }
  catch (const std::exception&) {
    // xclbin_lock failed, safe to ignore
  }
}

int 
getPSKernels(std::vector<ps_kernel_data> &psKernels, const xrt_core::device *device)
{
  try {
    std::vector<char> buf = xrt_core::device_query<xq::ps_kernel>(device);
    if (buf.empty())
      return 0;
    const ps_kernel_node *map = reinterpret_cast<ps_kernel_node *>(buf.data());
    if(map->pkn_count < 0)
      return -EINVAL;

    for (unsigned int i = 0; i < map->pkn_count; i++)
      psKernels.emplace_back(map->pkn_data[i]);
  }
  catch (const xq::no_such_key&) {
    // Ignoring if not available: Edge Case
  }

  return 0;
}

boost::property_tree::ptree
populate_cus(const xrt_core::device *device)
{
  schedulerUpdateStat(const_cast<xrt_core::device *>(device));

  boost::property_tree::ptree pt;
  using cu_data_type = xq::kds_cu_info::data_type;
  using scu_data_type = xq::kds_scu_info::data_type;
  std::vector<cu_data_type> cu_stats;
  std::vector<scu_data_type> scu_stats;
  boost::property_tree::ptree ptree;
  try {
    std::string uuid = xrt::uuid(xrt_core::device_query<xq::xclbin_uuid>(device)).to_string();
    boost::algorithm::to_upper(uuid);
    ptree.put("xclbin_uuid", uuid);
  } catch (xq::exception&) {  }

  try {
    cu_stats  = xrt_core::device_query<xq::kds_cu_info>(device);
    scu_stats = xrt_core::device_query<xq::kds_scu_info>(device);
  }
  catch (const xq::no_such_key&) {
    // Ignoring if not available: Edge Case
  }
  catch (const std::exception& ex) {
    ptree.put("error_msg", ex.what());
    return ptree;
  }

  for (auto& stat : cu_stats) {
    boost::property_tree::ptree ptCu;
    ptCu.put( "name",           stat.name);
    ptCu.put( "base_address",   boost::str(boost::format("0x%x") % stat.base_addr));
    ptCu.put( "usage",          stat.usages);
    ptCu.put( "type", enum_to_str(cu_type::PL));
    ptCu.add_child( std::string("status"),	get_cu_status(stat.status));
    pt.push_back(std::make_pair("", ptCu));
  }

  std::vector<ps_kernel_data> psKernels;
  if (getPSKernels(psKernels, device) < 0) {
    std::cout << "WARNING: 'ps_kernel' invalid. Has the PS kernel been loaded? See 'xbutil program'.\n";
    return ptree;
  }

  uint32_t psk_inst = 0;
  uint32_t num_scu = 0;
  boost::property_tree::ptree pscu_list;
  for (auto& stat : scu_stats) {
    boost::property_tree::ptree ptCu;
    std::string scu_name = "Illegal";
    if (psk_inst >= psKernels.size()) {
      scu_name = stat.name;
      //This means something is wrong
      //scu_name e.g. kernel_vcu_encoder:scu_34
    } else {
      scu_name = psKernels.at(psk_inst).pkd_sym_name;
      scu_name.append("_");
      scu_name.append(std::to_string(num_scu));
      //scu_name e.g. kernel_vcu_encoder_2
    }
    ptCu.put( "name",           scu_name);
    ptCu.put( "base_address",   "0x0");
    ptCu.put( "usage",          stat.usages);
    ptCu.put( "type", enum_to_str(cu_type::PS));
    ptCu.add_child( std::string("status"),	get_cu_status(stat.status));
    pt.push_back(std::make_pair("", ptCu));

    if (psk_inst >= psKernels.size()) {
      continue;
    }
    num_scu++;
    if (num_scu == psKernels.at(psk_inst).pkd_num_instances) {
      //Handled all instances of a PS Kernel, so next is a new PS Kernel
      num_scu = 0;
      psk_inst++;
    }
  }

  boost::property_tree::ptree pt_dynamic_regions;
  pt_dynamic_regions = xclbin_info(device);
  pt_dynamic_regions.add_child("compute_units", pt);
  return pt_dynamic_regions;
}

boost::property_tree::ptree
dynamic_regions(const xrt_core::device * device)
{
  ptree_type pt;
  boost::property_tree::ptree pt_dynamic_region;
  pt_dynamic_region.push_back(std::make_pair("", populate_cus(device)));
  pt.add_child("dynamic_regions", pt_dynamic_region);
  return pt;
}

}} // memory, xrt
