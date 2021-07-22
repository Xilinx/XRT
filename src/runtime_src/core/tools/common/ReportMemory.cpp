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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportMemory.h"
#include "core/common/utils.h"

// 3rd Party Library - Include Files
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>

// ------ S T A T I C   V A R I A B L E S -------------------------------------
constexpr uint32_t no_sensor_dev        = 0xffffffff;
constexpr int      invalid_sensor_value = 0;

// ------ S T A T I C   F U N C T I O N S -------------------------------------
namespace {
template <typename T>
inline std::string pretty(const T &val, const std::string &default_val = "N/A", bool isHex = false)
{   
  if (typeid(val).name() != typeid(std::string).name()){
    if (val >= std::numeric_limits<T>::max() || val == 0)
      return default_val;

    if (isHex){
      std::stringstream ss;
      ss << "0x" << std::hex << val;
      return ss.str();
    }
  }
  return boost::lexical_cast<std::string>(val);
}

} //unnamed namespace

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
  xrt::device device(_pDevice->get_device_id());
  boost::property_tree::ptree pt_memory;
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::memory>();
  boost::property_tree::read_json(ss, pt_memory);

  // There can only be 1 root node
  _pt.add_child("mem_topology", pt_memory);
}

void 
ReportMemory::writeReport( const xrt_core::device* /*_pDevice*/,
                           const boost::property_tree::ptree& _pt, 
                           const std::vector<std::string>& /*_elementsFilter*/,
                           std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

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
            unsigned int t = subv.second.get<unsigned int>("temperature_C", invalid_sensor_value);
            temp = pretty<unsigned int>(t == invalid_sensor_value ? no_sensor_dev : t, "N/A");
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
