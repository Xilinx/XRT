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

// ------ I N C L U D E   F I L E S -------------------------------------------
#define XRT_CORE_COMMON_SOURCE

// Local - Include Files
#include "info_memory.h"
#include "query_requests.h"

// 3rd Party Library - Include Files
#include <boost/algorithm/string.hpp>

// ------ S T A T I C   V A R I A B L E S -------------------------------------
constexpr int invalid_sensor_value = 0;

// ------ S T A T I C   F U N C T I O N S -------------------------------------
namespace {
static const std::map<MEM_TYPE, std::string> memtype_map = {
    {MEM_DDR3, "MEM_DDR3"},
    {MEM_DDR4, "MEM_DDR4"},
    {MEM_DRAM, "MEM_DRAM"},
    {MEM_STREAMING, "MEM_STREAMING"},
    {MEM_PREALLOCATED_GLOB, "MEM_PREALLOCATED_GLOB"},
    {MEM_ARE, "MEM_ARE"},
    {MEM_HBM, "MEM_HBM"},
    {MEM_BRAM, "MEM_BRAM"},
    {MEM_URAM, "MEM_URAM"},
    {MEM_STREAMING_CONNECTION, "MEM_STREAMING_CONNECTION"}
};

static int eccStatus2Str(uint64_t status, std::string& str) 
{    
  const int ce_mask = (0x1 << 1);
  const int ue_mask = (0x1 << 0);

  str.clear();

  // If unknown status bits, can't support.
  if (status & ~(ce_mask | ue_mask)) {
    //Bad ECC status detected!
    return -EINVAL;
  }    

  if (status == 0) { 
    str = "(None)";
    return -EINVAL;
  }    

  if (status & ue_mask)
    str += "UE ";
  if (status & ce_mask)
    str += "CE ";
  // Remove the trailing space.
  str.pop_back();
  return 0;
}

void getChannelinfo(const xrt_core::device * device, boost::property_tree::ptree& pt) {
  std::vector<std::string> dma_threads;
  boost::property_tree::ptree pt_dma_array;
  try {
    dma_threads = xrt_core::device_query<xrt_core::query::dma_threads_raw>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }
  uint64_t h2c[8];
  uint64_t c2h[8];
  for (unsigned i = 0; i < dma_threads.size(); i++) {
    boost::property_tree::ptree pt_dma;
    std::stringstream ss(dma_threads[i]);
    ss >> c2h[i] >> h2c[i];
    pt_dma.put("channel_id", i);
    pt_dma.put("host_to_card_bytes", boost::format("0x%x") % h2c[i]);
    pt_dma.put("card_to_host_bytes", boost::format("0x%x") % c2h[i]);
    pt_dma_array.push_back(std::make_pair("",pt_dma));
  }
  pt.put(std::string("board.direct_memory_accesses.type"), "pcie xdma");
  pt.add_child(std::string("board.direct_memory_accesses.metrics"), pt_dma_array);
}

} //unnamed namespace

namespace xrt_core {
namespace memory {

boost::property_tree::ptree
memory_topology(const xrt_core::device * device)
{
  boost::property_tree::ptree pt;
  std::vector<std::string> mm_buf, stream_stat;
  std::vector<char> buf, temp_buf, gbuf;
  uint64_t memoryUsage, boCount;
  getChannelinfo(device, pt);
  try {
    buf = xrt_core::device_query<xrt_core::query::mem_topology_raw>(device);
    mm_buf = xrt_core::device_query<xrt_core::query::memstat_raw>(device);
    temp_buf = xrt_core::device_query<xrt_core::query::temp_by_mem_topology>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }
  
  if(buf.empty() || mm_buf.empty())
    return pt;

  const mem_topology *map = (mem_topology *)buf.data();
  const uint32_t *temp = (uint32_t *)temp_buf.data();

  try {
    boost::any a = std::string("1");
    xrt_core::device_update<xrt_core::query::mig_cache_update>(device,a);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }
  
  boost::property_tree::ptree ptMem_array;
  boost::property_tree::ptree ptStream_array;
  boost::property_tree::ptree ptGrp_array;
  for (int i = 0; i < map->m_count; i++) {
    if (map->m_mem_data[i].m_type == MEM_STREAMING || map->m_mem_data[i].m_type == MEM_STREAMING_CONNECTION) {
      std::string lname, status = "Inactive", total = "N/A", pending = "N/A";
      boost::property_tree::ptree ptStream;
      std::map<std::string, std::string> stat_map;
      lname = std::string((char *)map->m_mem_data[i].m_tag);
      ptStream.put("tag", map->m_mem_data[i].m_tag);
      if (lname.back() == 'w')
        lname = "route" + std::to_string(map->m_mem_data[i].route_id) + "/stat";
      else if (lname.back() == 'r')
        lname = "flow" + std::to_string(map->m_mem_data[i].flow_id) + "/stat";
      else
        status = "N/A";
      try {
        stream_stat = xrt_core::device_query<xrt_core::query::dma_stream>(device, xrt_core::query::request::modifier::entry, lname);
        status = "Active";
        for (unsigned k = 0; k < stream_stat.size(); k++) {
          std::vector<std::string> strs;
          boost::split(strs, stream_stat[k],boost::is_any_of(":"));
          if (strs.size() > 1)
            stat_map[strs[0]] = strs[1];
        }

        total = stat_map[std::string("complete_bytes")] + "/" + stat_map[std::string("complete_requests")];
        pending = stat_map[std::string("pending_bytes")] + "/" + stat_map[std::string("pending_requests")];
        ptStream.put("usage.status", status);
        ptStream.put("usage.total", total);
        ptStream.put("usage.pending", pending);
      } catch (const std::exception& ){
        // eat the exception, probably bad path
      }
  
      ptStream_array.push_back(std::make_pair("",ptStream));
      continue;
    }

    boost::property_tree::ptree ptMem;

    std::string str = "**UNUSED**";
    auto search = memtype_map.find((MEM_TYPE)map->m_mem_data[i].m_type );
    str = search->second;
    if (map->m_mem_data[i].m_used != 0) {
      uint64_t ecc_st = 0xffffffffffffffff;
      std::string ecc_st_str;
      std::string tag(reinterpret_cast<const char *>(map->m_mem_data[i].m_tag));
      try {
        ecc_st = xrt_core::device_query<xrt_core::query::mig_ecc_status>(device, xrt_core::query::request::modifier::subdev, tag);
      } catch (const std::exception& ex){
        pt.put("error_msg", ex.what());
      }
      if (eccStatus2Str(ecc_st, ecc_st_str) == 0) {
        uint64_t ce_cnt = 0;
        ce_cnt = xrt_core::device_query<xrt_core::query::mig_ecc_ce_cnt>(device, xrt_core::query::request::modifier::subdev, tag);
        uint64_t ue_cnt = 0;
        ue_cnt = xrt_core::device_query<xrt_core::query::mig_ecc_ue_cnt>(device, xrt_core::query::request::modifier::subdev, tag);
        uint64_t ce_ffa = 0;
        ce_ffa = xrt_core::device_query<xrt_core::query::mig_ecc_ce_ffa>(device, xrt_core::query::request::modifier::subdev, tag);
        uint64_t ue_ffa = 0;
        ue_ffa = xrt_core::device_query<xrt_core::query::mig_ecc_ue_ffa>(device, xrt_core::query::request::modifier::subdev, tag);

        ptMem.put("extended_info.ecc.status", ecc_st_str);
        ptMem.put("extended_info.ecc.error.correctable.count", ce_cnt);
        ptMem.put("extended_info.ecc.error.correctable.first_failure_address", boost::format("0x%x") % ce_ffa);
        ptMem.put("extended_info.ecc.error.uncorrectable.count", ue_cnt);
        ptMem.put("extended_info.ecc.error.uncorrectable.first_failure_address", boost::format("0x%x") % ue_ffa);
      }
    }
    std::stringstream ss(mm_buf[i]);
    ss >> memoryUsage >> boCount;

    ptMem.put("type", str);
    ptMem.put("tag", map->m_mem_data[i].m_tag);
    ptMem.put("enabled", map->m_mem_data[i].m_used ? true : false);
    ptMem.put("base_address", boost::format("0x%x") % map->m_mem_data[i].m_base_address);
    ptMem.put("range_bytes", boost::format("0x%x") % (map->m_mem_data[i].m_size << 10));
    if (!temp_buf.empty() && temp[i] != invalid_sensor_value)
      ptMem.put("extended_info.temperature_C", temp[i]);
    ptMem.put("extended_info.usage.allocated_bytes", memoryUsage);
    ptMem.put("extended_info.usage.buffer_objects_count", boCount);
    ptMem_array.push_back(std::make_pair("",ptMem));
  }
  pt.add_child(std::string("board.memory.data_streams"), ptStream_array);
  pt.add_child(std::string("board.memory.memories"), ptMem_array );

  try {
    mm_buf = xrt_core::device_query<xrt_core::query::memstat_raw>(device);
    gbuf = xrt_core::device_query<xrt_core::query::group_topology>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }

  if (gbuf.empty() || mm_buf.empty())
    return pt;

  const mem_topology *grp_map = (mem_topology *)gbuf.data();

  for (int i = 0; i < grp_map->m_count; i++) {
    if (grp_map->m_mem_data[i].m_used != 0) {
      boost::property_tree::ptree ptGrp;
      auto search = memtype_map.find((MEM_TYPE)grp_map->m_mem_data[i].m_type );
      std::string str = search->second;
      std::stringstream ss(mm_buf[i]);
      ss >> memoryUsage >> boCount;
      ptGrp.put("type", str);
      ptGrp.put("tag", grp_map->m_mem_data[i].m_tag);
      ptGrp.put("base_address", boost::format("0x%x") % map->m_mem_data[i].m_base_address);
      ptGrp.put("range_bytes", boost::format("0x%x") % (grp_map->m_mem_data[i].m_size << 10));
      ptGrp.put("extended_info.usage.allocated_bytes", memoryUsage);
      ptGrp.put("extended_info.usage.buffer_objects_count", boCount);
      ptGrp_array.push_back(std::make_pair("",ptGrp));
    }
  }
  pt.add_child(std::string("board.memory.memory_groups"), ptGrp_array);
  return pt;
}

}} // memory, xrt