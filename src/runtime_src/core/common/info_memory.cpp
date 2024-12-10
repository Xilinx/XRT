// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved

#define XRT_CORE_COMMON_SOURCE
#include "info_memory.h"
#include "ps_kernel.h"
#include "query_requests.h"
#include "utils.h"
#include "xrt/detail/xclbin.h"

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
   {MEM_STREAMING_CONNECTION, "MEM_STREAMING_CONNECTION"},
   {MEM_PS_KERNEL,            "MEM_PS_KERNEL"}
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

  std::vector<xrt_core::query::hw_context_memory_info::data_type> hw_context_memories;  // xclbin mem topology from device

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
  add_mem_general_info(
    const xq::hw_context_memory_info::data_type& topology,
    const mem_data* mem,
    ptree_type& pt_mem)
  {
    pt_mem.put("xclbin_uuid", topology.metadata.xclbin_uuid);
    pt_mem.put("hw_context_slot", boost::format("%u") % topology.metadata.id);
    pt_mem.put("type", memtype2str(mem->m_type));
    pt_mem.put("tag", mem->m_tag);
    pt_mem.put("enabled", mem->m_used ? true : false);
    pt_mem.put("base_address", boost::format("0x%x") % mem->m_base_address);
    pt_mem.put("range_bytes", boost::format("0x%x") % (mem->m_size * 1024)); // convert KB to bytes
  }

  // Add mem usage info for specified mem entry.
  // This function is shared with group topology, hence need to
  // know where the mem entry is comining from
  void
  add_mem_usage_info(const std::string& mem_stat, ptree_type& pt_mem)
  {
    uint64_t memory_usage = 0;
    uint64_t bo_count = 0;
    std::stringstream {mem_stat} >> memory_usage >> bo_count; // idx has been validated
    pt_mem.put("extended_info.usage.allocated_bytes", memory_usage);
    pt_mem.put("extended_info.usage.buffer_objects_count", bo_count);
  }

  // Add mem temperature info for specified mem entry
  void
  add_mem_temp_info(
    const size_t idx,
    const xq::temp_by_mem_topology::result_type& temp,
    ptree_type& pt_mem)
  {
    const auto mem_temp = temp.empty() ? nullptr : reinterpret_cast<const uint32_t*>(temp.data());
    // temperature is guaranteed to match up with mem_topo entries
    // indexing is safe because idx is validated
    constexpr int invalid_sensor_value = 0;
    if (mem_temp && mem_temp[idx] != invalid_sensor_value)
      pt_mem.put("extended_info.temperature_C", mem_temp[idx]);
  }

  // Add mem info for all mem entries in mem_topology section
  void
  add_mem_info(ptree_type& pt)
  {
    ptree_type pt_mem_array;
    ptree_type pt_stream_array;

    for (const auto& topology : hw_context_memories) {
      const auto mem_topo = reinterpret_cast<const mem_topology*>(topology.topology.data());

      if (!mem_topo)
        continue;

      for (int i = 0; i < mem_topo->m_count; ++i) {
        const auto& mem = mem_topo->m_mem_data[i];
        
        if (mem.m_type == MEM_STREAMING || mem.m_type == MEM_STREAMING_CONNECTION)
          add_stream_info(&mem, pt_stream_array);
        else {
          ptree_type pt_mem;
          add_mem_ecc_info(&mem, pt_mem);
          add_mem_general_info(topology, &mem, pt_mem);
          add_mem_usage_info(topology.statistics[i], pt_mem);
          add_mem_temp_info(i, topology.temperature, pt_mem);
          pt_mem_array.push_back(std::make_pair("", pt_mem));
        }
      }
    }

    pt.add_child("board.memory.data_streams", pt_stream_array);
    pt.add_child("board.memory.memories", pt_mem_array);
  }

  // Add grp info for all mem entries in group_topology section
  void
  add_grp_info(ptree_type& pt)
  {
    ptree_type pt_grp_array;

    for (const auto& topology : hw_context_memories) {
      const auto mem_topo = reinterpret_cast<const mem_topology*>(topology.topology.data());
      const auto grp_topo = reinterpret_cast<const mem_topology*>(topology.grp_topology.data());

      if (!mem_topo || !grp_topo)
        continue;

      // group_topology prepends all mem_topology entries so groups
      // are following at index mem_topo->m_count
      for (int i = mem_topo->m_count; i < grp_topo->m_count; i++) {
        ptree_type pt_grp;
        add_mem_general_info(topology, &grp_topo->m_mem_data[i], pt_grp);
        add_mem_usage_info(topology.statistics[i], pt_grp);
        pt_grp_array.push_back(std::make_pair("",pt_grp));
      }
    }

    if (!pt_grp_array.empty())
      pt.add_child("board.memory.memory_groups", pt_grp_array);
  }

public:
  explicit
  memory_info_collector(const xrt_core::device* dev)
    : device(dev)
  {
    try {
      hw_context_memories = xrt_core::device_query<xq::hw_context_memory_info>(device);
    }
    catch (const xq::exception&) {
      // Try legacy method
      xq::hw_context_memory_info::data_type hw_context_mem;
      hw_context_mem.metadata.id = "0";
      hw_context_mem.metadata.xclbin_uuid = xrt_core::device_query_default<xq::xclbin_uuid>(device, "");
      hw_context_mem.topology = xrt_core::device_query<xq::mem_topology_raw>(device);
      hw_context_mem.grp_topology = xrt_core::device_query<xq::group_topology>(device);
      hw_context_mem.statistics = xrt_core::device_query<xq::memstat_raw>(device);
      hw_context_mem.temperature = xrt_core::device_query_default<xq::temp_by_mem_topology>(dev, {});
      hw_context_memories.push_back(hw_context_mem);
    }

    // validate the memory topologies for each hardware context
    for (const auto& memory : hw_context_memories) {
      const auto mem_topo = reinterpret_cast<const mem_topology*>(memory.topology.data());
      const auto& mem_stat = memory.statistics;
      const auto grp_topo = reinterpret_cast<const mem_topology*>(memory.grp_topology.data());
      const auto mem_temp = reinterpret_cast<const uint32_t*>(memory.temperature.data());

      // info gathering functions indexes mem_stat by mem_toplogy entry index
      if (mem_topo && mem_stat.size() < static_cast<size_t>(mem_topo->m_count))
        throw xrt_core::internal_error("incorrect memstat_raw entries");

      // info gathering functions indexes mem_temp by mem_topology entry index
      if (mem_topo && mem_temp && memory.temperature.size() < static_cast<size_t>(mem_topo->m_count))
        throw xrt_core::internal_error("incorrect temp_by_mem_topology entries");

      // info gathering functions indexes mem_stat by group_toplogy entry index
      if (grp_topo && mem_stat.size() < static_cast<size_t>(grp_topo->m_count))
        throw xrt_core::internal_error("incorrect temp_by_mem_topology entries");
    }
  }

  void
  collect(ptree_type& pt)
  {
    if (hw_context_memories.empty())
      return;

    add_channel_info(pt);
    update_mig_cache(pt);  // why?
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
xclbin_info(const xrt_core::device* device)
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

enum class cu_type 
{
  pl, // programming logic
  ps  // processor system
};

static std::string 
enum_to_str(cu_type type) 
{
  switch (type) {
    case cu_type::pl:
      return "PL";
    case cu_type::ps:
      return "PS";
  }
  return "UNKNOWN";
}

ptree_type
get_cu_status(uint32_t cu_status)
{
  ptree_type pt;
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

  pt.put("bit_mask", boost::str(boost::format("0x%x") % cu_status));
  ptree_type ptSt_arr;
  for(auto& str : bit_set)
    ptSt_arr.push_back(std::make_pair("", ptree_type(str)));

  if (!ptSt_arr.empty())
    pt.add_child( std::string("bits_set"), ptSt_arr);

  return pt;
}

static void
scheduler_update_stat(const xrt_core::device* device)
{
  // device query and open_context requires a non-cont raw device ptr
  auto dev = const_cast<xrt_core::device *>(device);
  try {
    // lock xclbin
    std::string xclbin_uuid = xrt_core::device_query<xq::xclbin_uuid>(dev);
    // dont open a context if xclbin_uuid is empty or is all zeros
    if (xclbin_uuid.empty() || !xrt::uuid(xclbin_uuid))
      return;
    auto uuid = xrt::uuid(xclbin_uuid);
    dev->open_context(uuid.get(), std::numeric_limits<unsigned int>::max(), true);
    auto at_exit = [] (auto dev, auto uuid) { dev->close_context(uuid.get(), std::numeric_limits<unsigned int>::max()); };
    xrt_core::scope_guard<std::function<void()>> g(std::bind(at_exit, dev, uuid));

    dev->update_scheduler_status();
  }
  catch (const std::exception&) {
    // xclbin_lock failed, safe to ignore
  }
}

std::vector<ps_kernel_data> 
get_ps_kernels(const xrt_core::device* device)
{
  std::vector<ps_kernel_data> ps_kernels;
  try {
    std::vector<char> buf = xrt_core::device_query<xq::ps_kernel>(device);
    if (buf.empty())
      return ps_kernels;
    const ps_kernel_node *map = reinterpret_cast<ps_kernel_node *>(buf.data());
    if(map->pkn_count == 0)
      throw xrt_core::error("'ps_kernel' invalid. Has the PS kernel been loaded? See 'xrt-smi program'.");

    for (unsigned int i = 0; i < map->pkn_count; i++)
      ps_kernels.emplace_back(map->pkn_data[i]);
  }
  catch (const xq::no_such_key&) {
    // Ignoring if not available: Edge Case
  }

  return ps_kernels;
}

ptree_type
populate_cus(const xrt_core::device* device, const std::vector<xq::kds_cu_info::data_type>& cu_stats, const std::vector<xq::kds_scu_info::data_type>& scu_stats)
{
  // Tree that holds all ps and pl objects
  ptree_type pt;

  // Add all CU objects into tree
  for (auto& stat : cu_stats) {
    ptree_type pt_cu;
    pt_cu.put( "name", stat.name);
    pt_cu.put( "base_address", boost::str(boost::format("0x%x") % stat.base_addr));
    pt_cu.put( "usage", stat.usages);
    pt_cu.put( "type", enum_to_str(cu_type::pl));
    pt_cu.add_child( std::string("status"),	get_cu_status(stat.status));
    pt.push_back(std::make_pair("", pt_cu));
  }

  // Collect ps kernel information and correlate it to scu stats
  std::vector<ps_kernel_data> ps_kernels;
  try {
    ps_kernels = get_ps_kernels(device);
  } catch(const xrt_core::error& ex) {
    std::cout << ex.what() <<std::endl;
    return pt;
  }

  // Add all SCU objects into tree
  uint32_t psk_inst = 0;
  uint32_t num_scu = 0;
  ptree_type pscu_list;
  for (auto& stat : scu_stats) {
    ptree_type pt_cu;
    std::string scu_name = "Illegal";
    // This means something is wrong
    // scu_name e.g. kernel_vcu_encoder:scu_34
    if (psk_inst >= ps_kernels.size()) {
      scu_name = stat.name;
    } 
    else { // scu_name e.g. kernel_vcu_encoder_2
      scu_name = ps_kernels.at(psk_inst).pkd_sym_name;
      scu_name.append(":");
      scu_name.append(ps_kernels.at(psk_inst).pkd_sym_name);
      scu_name.append("_");
      scu_name.append(std::to_string(num_scu));
    }
    pt_cu.put( "name", scu_name);
    pt_cu.put( "base_address", "0x0");
    pt_cu.put( "usage", stat.usages);
    pt_cu.put( "type", enum_to_str(cu_type::ps));
    pt_cu.add_child( std::string("status"),	get_cu_status(stat.status));
    pt.push_back(std::make_pair("", pt_cu));

    if (psk_inst >= ps_kernels.size())
      continue;
    num_scu++;
    if (num_scu == ps_kernels.at(psk_inst).pkd_num_instances) {
      //Handled all instances of a PS Kernel, so next is a new PS Kernel
      num_scu = 0;
      psk_inst++;
    }
  }

  return pt;
}

static ptree_type
populate_hardware_context(const xrt_core::device* device)
{
  ptree_type pt;
  scheduler_update_stat(device);

  std::vector<xq::hw_context_info::data_type> hw_context_stats;
  ptree_type ptree;

  // Get HW context info
  try {
    hw_context_stats = xrt_core::device_query<xq::hw_context_info>(device);
  }
  catch (const xq::no_such_key&) {
    // Legacy Case
    xq::hw_context_info::data_type hw_context;

    hw_context.metadata.id = "0";
    hw_context.metadata.xclbin_uuid = xrt_core::device_query_default<xq::xclbin_uuid>(device, "");
    hw_context.pl_compute_units = xrt_core::device_query_default<xq::kds_cu_info>(device, {});
    hw_context.ps_compute_units = xrt_core::device_query_default<xq::kds_scu_info>(device, {});

    // Account for devices that do not have an xclbin uuid but have compute units
    if (!hw_context.metadata.xclbin_uuid.empty() || !hw_context.pl_compute_units.empty() || !hw_context.ps_compute_units.empty())
      hw_context_stats.push_back(hw_context);
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
    return pt;
  }

  for (const auto& hw : hw_context_stats) {
    ptree_type pt_hw;
    pt_hw.put("id", boost::algorithm::to_upper_copy(hw.metadata.id));
    pt_hw.put("xclbin_uuid", boost::algorithm::to_upper_copy(hw.metadata.xclbin_uuid));
    pt_hw.add_child("compute_units", populate_cus(device, hw.pl_compute_units, hw.ps_compute_units));
    pt.push_back(std::make_pair("", pt_hw));
  }

  return pt;
}

ptree_type
dynamic_regions(const xrt_core::device* device)
{
  ptree_type pt;
  pt.add_child("dynamic_regions", populate_hardware_context(device));
  return pt;
}

}} // memory, xrt
