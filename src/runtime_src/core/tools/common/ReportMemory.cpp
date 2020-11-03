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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportMemory.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"
namespace qr = xrt_core::query;

#define XCL_NO_SENSOR_DEV_LL    ~(0ULL)
#define XCL_NO_SENSOR_DEV       ~(0U)
#define XCL_NO_SENSOR_DEV_S     0xffff
#define XCL_INVALID_SENSOR_VAL 0

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

template <typename T>
inline std::string pretty( const T &val, const std::string &default_val = "N/A", bool isHex = false )
{   
    if (typeid(val).name() != typeid(std::string).name()) {
        if (val >= std::numeric_limits<T>::max() || val == 0) {
            return default_val;
        }

        if (isHex) {
            std::stringstream ss;
            ss << "0x" << std::hex << val;
            return ss.str();
        }
    }
    return boost::lexical_cast<std::string>(val);
}

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
    dma_threads = xrt_core::device_query<qr::dma_threads_raw>(device);
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

boost::property_tree::ptree
populate_memtopology(const xrt_core::device * device, const std::string& desc)
{
  boost::property_tree::ptree pt;
  std::vector<std::string> mm_buf, stream_stat;
  std::vector<char> buf, temp_buf, gbuf;
  uint64_t memoryUsage, boCount;
  pt.put("description", desc);
  getChannelinfo(device, pt);
  try {
    buf = xrt_core::device_query<qr::mem_topology_raw>(device);
    mm_buf = xrt_core::device_query<qr::memstat_raw>(device);
    temp_buf = xrt_core::device_query<qr::temp_by_mem_topology>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }
  
  if(buf.empty() || mm_buf.empty())
    return pt;

  const mem_topology *map = (mem_topology *)buf.data();
  const uint32_t *temp = (uint32_t *)temp_buf.data();

  try {
    boost::any a = std::string("1");
    xrt_core::device_update<qr::mig_cache_update>(device,a);
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
        stream_stat = xrt_core::device_query<qr::dma_stream>(device, qr::request::modifier::entry, lname);
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
        ecc_st = xrt_core::device_query<qr::mig_ecc_status>(device, qr::request::modifier::subdev, tag);
      } catch (const std::exception& ex){
        pt.put("error_msg", ex.what());
      }
      if (eccStatus2Str(ecc_st, ecc_st_str) == 0) {
        uint64_t ce_cnt = 0;
        ce_cnt = xrt_core::device_query<qr::mig_ecc_ce_cnt>(device, qr::request::modifier::subdev, tag);
        uint64_t ue_cnt = 0;
        ue_cnt = xrt_core::device_query<qr::mig_ecc_ue_cnt>(device, qr::request::modifier::subdev, tag);
        uint64_t ce_ffa = 0;
        ce_ffa = xrt_core::device_query<qr::mig_ecc_ce_ffa>(device, qr::request::modifier::subdev, tag);
        uint64_t ue_ffa = 0;
        ue_ffa = xrt_core::device_query<qr::mig_ecc_ue_ffa>(device, qr::request::modifier::subdev, tag);

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
    if (!temp_buf.empty() && temp[i] != XCL_INVALID_SENSOR_VAL)
      ptMem.put("extended_info.temperature_C", temp[i]);
    ptMem.put("extended_info.usage.allocated_bytes", memoryUsage);
    ptMem.put("extended_info.usage.buffer_objects_count", boCount);
    ptMem_array.push_back(std::make_pair("",ptMem));
  }
  pt.add_child(std::string("board.memory.data_streams"), ptStream_array);
  pt.add_child(std::string("board.memory.memories"), ptMem_array );

  try {
    mm_buf = xrt_core::device_query<qr::memstat_raw>(device);
    gbuf = xrt_core::device_query<qr::group_topology>(device);
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

void
ReportMemory::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportMemory::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  _pt.add_child("mem_topology", populate_memtopology(_pDevice, "Memory Information"));
}

void 
ReportMemory::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << boost::format("%s\n") % _pt.get<std::string>("mem_topology.description");


  try {
    int index = 0;
    for (auto& v : _pt.get_child("mem_topology.board.memory.memories",empty_ptree)) {
      std::string tag, st;
      unsigned int ce_cnt = 0, ue_cnt = 0;
      uint64_t ce_ffa = 0, ue_ffa = 0;
      for (auto& subv : v.second) {
        if (subv.first == "tag") {
          tag = subv.second.get_value<std::string>();
        } else if (subv.first == "extended_info") {
          st = subv.second.get<std::string>("ecc.status","");
          if (!st.empty()) {
            ce_cnt = subv.second.get<unsigned int>("ecc.error.correctable.count");
            ce_ffa = std::stoll(subv.second.get<std::string>("ecc.error.correctable.first_failure_address"), 0, 16);
            ue_cnt = subv.second.get<unsigned int>("ecc.error.uncorrectable.count");
            ue_ffa = std::stoll(subv.second.get<std::string>("ecc.error.uncorrectable.first_failure_address"), 0, 16);
          }
        }
      }
      if (!st.empty()) {
        if (index == 0) {
          _output << std::endl;
          _output << "  ECC Error Status\n";
          _output << boost::format("    %-8s%-12s%-10s%-10s%-20s%-20s\n") % "Tag" % "Errors"
              % "CE Count" % "UE Count" % "CE FFA" % "UE FFA";
        }
        _output << boost::format("    %-8s%-12s%-10d%-10d0x%-20x0x%-20x\n") % tag
           % st % ce_cnt % ue_cnt % ce_ffa % ue_ffa;
        index++;
      }
    }
  }
  catch( std::exception const&) {
    // eat the exception, probably bad path
  }

  bool mem_is_present = _pt.get_child("mem_topology.board.memory.memories",empty_ptree).size() > 0 ? true:false;
  if (mem_is_present) {
    int index = 0;
    _output << std::endl;
    _output << "  Memory Topology" << std::endl;
    _output << boost::format("    %-17s%-12s%-9s%-10s%-16s\n") % "     Tag" % "Type"
        % "Temp(C)" % "Size" % "Base Address";
    try {
      for (auto& v : _pt.get_child("mem_topology.board.memory.memories",empty_ptree)) {
        std::string tag, size, type, temp, base_addr;
        for (auto& subv : v.second) {
          if (subv.first == "type") {
            type = subv.second.get_value<std::string>();
          } else if (subv.first == "tag") {
            tag = subv.second.get_value<std::string>();
          } else if (subv.first == "extended_info") {
            unsigned int t = subv.second.get<unsigned int>("temperature_C",XCL_INVALID_SENSOR_VAL);
            temp = pretty<unsigned int>(t == XCL_INVALID_SENSOR_VAL ? XCL_NO_SENSOR_DEV : t, "N/A");
          } else if (subv.first == "range_bytes") {
            size = xrt_core::utils::unit_convert(std::stoll(subv.second.get_value<std::string>(), 0, 16));
          } else if (subv.first == "base_address") {
            base_addr = subv.second.get_value<std::string>();
          }
        }
        _output << boost::format("    [%2d] %-12s%-12s%-9s%-10s%-16s\n") % index
            % tag % type % temp % size % base_addr;
        index++;
      }
    }
    catch( std::exception const&) {
        // eat the exception, probably bad path
    }
  }

  bool grp_is_present = _pt.get_child("mem_topology.board.memory.memory_groups",empty_ptree).size() > 0 ? true:false;
  if (grp_is_present) {
    int index = 0;
    _output << std::endl;
    _output << "  Memory Status" << std::endl;
    _output << boost::format("    %-17s%-12s%-8s%-16s%-8s\n") % "     Tag" % "Type" % "Size" % "Mem Usage"
        % "BO count";

    try {
      for (auto& v : _pt.get_child("mem_topology.board.memory.memory_groups",empty_ptree)) {
        std::string mem_usage, tag, size, type, base_addr;
        unsigned bo_count = 0;
        for (auto& subv : v.second) {
          if (subv.first == "type") {
            type = subv.second.get_value<std::string>();
          } else if (subv.first == "tag") {
            tag = subv.second.get_value<std::string>();
          } else if (subv.first == "extended_info") {
            bo_count = subv.second.get<unsigned>("usage.buffer_objects_count",0);
            mem_usage = xrt_core::utils::unit_convert(subv.second.get<size_t>("usage.allocated_bytes",0));
          } else if (subv.first == "range_bytes") {
            size = xrt_core::utils::unit_convert(std::stoll(subv.second.get_value<std::string>(), 0, 16));
          }
        }
        _output << boost::format("    [%2d] %-12s%-12s%-8s%-16s%-8u\n") % index % tag % type
            % size % mem_usage % bo_count;
        index++;
      }
    }
    catch( std::exception const&) {
      // eat the exception, probably bad path
    }
  }

  bool dma_is_present = _pt.get_child("mem_topology.board.direct_memory_accesses.metrics",empty_ptree).size() > 0 ? true:false;
  if (dma_is_present) {
    int index = 0;
    _output << std::endl;
    _output << "  DMA Transfer Metrics" << std::endl;
    try {
      for (auto& v : _pt.get_child("mem_topology.board.direct_memory_accesses.metrics",empty_ptree)) {
        std::string chan_h2c, chan_c2h, chan_val = "N/A";
        for (auto& subv : v.second) {
          chan_val = xrt_core::utils::unit_convert(std::stoll(subv.second.get_value<std::string>(), 0, 16));
          if (subv.first == "host_to_card_bytes")
            chan_h2c = chan_val;
          else if (subv.first == "card_to_host_bytes")
            chan_c2h = chan_val;
        }
        _output << boost::format("    Chan[%2d].h2c:  %lu\n") % index % chan_h2c;
        _output << boost::format("    Chan[%2d].c2h:  %lu\n") % index % chan_c2h;
        index++;
      }
    }
    catch( std::exception const&) {
      // eat the exception, probably bad path
    }
  }

  bool stream_is_present = _pt.get_child("mem_topology.board.memory.data_streams",empty_ptree).size() > 0 ? true:false;
  if (stream_is_present) {
    int index = 0;
    _output << std::endl;
    _output << "  Streams" << std::endl;
    _output << boost::format("    %-17s%-9s%-16s%-16s\n") % "     Tag" 
        % "Status" % "Total (B/#)" % "Pending (B/#)";
    try {
      for (auto& v : _pt.get_child("mem_topology.board.memory.data_streams",empty_ptree)) {
        std::string status = "N/A", tag, total = "N/A" , pending = "N/A";
        for (auto& subv : v.second) {
          if (subv.first == "tag") {
            tag = subv.second.get_value<std::string>();
          } else if (subv.first == "usage") {
            status = subv.second.get<std::string>("status", "N/A");
            total = subv.second.get<std::string>("total", "N/A");
            pending = subv.second.get<std::string>("pending", "N/A");
          }
        }
        _output << boost::format("    [%2d] %-12s%-9s%-16s%-16s\n") % index
           % tag % status % total % pending;
        index++;
      }
    }
    catch( std::exception const& ) {
      // eat the exception, probably bad path
    }
  }
  _output << std::endl;
}
