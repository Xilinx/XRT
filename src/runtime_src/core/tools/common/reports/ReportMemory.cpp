// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2021 Xilinx, Inc
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportMemory.h"
#include "tools/common/Table2D.h"
#include "core/common/info_memory.h"
#include "core/common/utils.h"

#include <map>
#include <string>

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

// ------ S T A T I C   V A R I A B L E S -------------------------------------
constexpr uint32_t no_sensor_dev        = 0xffffffff;
constexpr int      invalid_sensor_value = 0;

// ------ S T A T I C   F U N C T I O N S -------------------------------------
namespace {
inline std::string
pretty(unsigned int val, const std::string &default_val = "N/A", bool isHex = false)
{   
  if (val >= std::numeric_limits<unsigned int>::max() || val == 0)
    return default_val;

  if (isHex) {
    std::stringstream ss;
    ss << "0x" << std::hex << val;
    return ss.str();
  }

  return std::to_string(val);
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
  // There can only be 1 root node
  _pt.add_child("mem_topology", xrt_core::memory::memory_topology(_pDevice));
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
    _output << std::endl;
    _output << "  Memory Topology" << std::endl;

    try {
      // Generate map of hw_context/xclbin uuid to a list of formatted memory table entries
      std::map<std::tuple<std::string, std::string>, std::vector<std::vector<std::string>>> memory_map;

      for (const auto& v : _pt.get_child("mem_topology.board.memory.memories",empty_ptree)) {
        std::string slot, uuid, tag, size, type, temp, base_addr;

        for (auto& subv : v.second) {
          if (subv.first == "type") {
            type = subv.second.get_value<std::string>();
          } else if (subv.first == "hw_context_slot") {
            slot = subv.second.get_value<std::string>();
          } else if (subv.first == "xclbin_uuid") {
            uuid = subv.second.get_value<std::string>();
          } else if (subv.first == "tag") {
            tag = subv.second.get_value<std::string>();
          } else if (subv.first == "extended_info") {
            unsigned int t = subv.second.get<unsigned int>("temperature_C", invalid_sensor_value);
            temp = pretty(t == invalid_sensor_value ? no_sensor_dev : t, "N/A");
          } else if (subv.first == "range_bytes") {
            size = xrt_core::utils::unit_convert(std::stoll(subv.second.get_value<std::string>(), 0, 16));
          } else if (subv.first == "base_address") {
            base_addr = subv.second.get_value<std::string>();
          }
        }
        const std::vector<std::string> entry_data = {tag, type, temp, size, base_addr};

        const auto key = std::make_tuple(slot, uuid);
        const auto& iter = memory_map.emplace(key, std::vector<std::vector<std::string>>());;
        iter.first->second.push_back(entry_data);
      }

      // Output the contents of each hardware context
      for (auto& hw_context : memory_map) {
        _output << boost::format("    HW Context Slot: %s\n") % std::get<0>(hw_context.first);
        _output << boost::format("      Xclbin UUID: %s\n") % std::get<1>(hw_context.first);

        const Table2D::HeaderData h_index = {"Index", Table2D::Justification::left};
        const Table2D::HeaderData h_tag = {"Tag", Table2D::Justification::left};
        const Table2D::HeaderData h_type = {"Type", Table2D::Justification::left};
        const Table2D::HeaderData h_temp = {"Temp(C)", Table2D::Justification::left};
        const Table2D::HeaderData h_size = {"Size", Table2D::Justification::left};
        const Table2D::HeaderData h_address = {"Base Address", Table2D::Justification::left};
        const std::vector<Table2D::HeaderData> table_headers = {h_index, h_tag, h_type, h_temp, h_size, h_address};
        Table2D device_table(table_headers);

        // Place each compute unit into a table
        auto& entry_list = hw_context.second;
        for (size_t hw_index = 0; hw_index < entry_list.size(); hw_index++) {
          auto& entry_list_item = entry_list[hw_index];
          entry_list_item.insert(entry_list_item.begin(), std::to_string(hw_index));
          device_table.addEntry(entry_list_item);
        }
        _output << device_table.toString("      ");
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
