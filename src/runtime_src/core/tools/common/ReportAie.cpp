/*
  Copyright (C) 2020-2023 Xilinx, Inc
  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 
  Licensed under the Apache License, Version 2.0 (the "License"). You may
  not use this file except in compliance with the License. A copy of the
  License is located at
 
      http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
  License for the specific language governing permissions and limitations
  under the License.
 */

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportAie.h"
#include "core/common/info_aie.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#define fmtCommon(x) boost::format("    %-22s: " x "\n")
#define fmt4(x) boost::format("%4s%-22s: " x "\n") % " "
#define fmt8(x) boost::format("%8s%-22s: " x "\n") % " "
#define fmt12(x) boost::format("%12s%-22s: " x "\n") % " "
#define fmt16(x) boost::format("%16s%-22s: " x "\n") % " "

boost::property_tree::ptree
populate_aie(const xrt_core::device * _pDevice, const std::string& desc)
{
  xrt::device device(_pDevice->get_device_id());
  boost::property_tree::ptree pt_aie;
  pt_aie.put("description", desc);
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::aie>();
  boost::property_tree::read_json(ss, pt_aie);

  return pt_aie;
}

void
ReportAie::getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                                   boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportAie::getPropertyTree20202(const xrt_core::device * _pDevice, 
                                boost::property_tree::ptree &_pt) const
{
  _pt.add_child("aie_metadata", populate_aie(_pDevice, "Aie_Metadata"));
}

void
ReportAie::writeReport(const xrt_core::device* /*_pDevice*/,
                       const boost::property_tree::ptree& _pt,
                       const std::vector<std::string>& _elementsFilter,
                       std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;
  std::vector<std::string> aieCoreList;
  bool is_less = false;

  // Loop through all the parameters given under _elementsFilter i.e. -e option
  for (auto it = _elementsFilter.begin(); it != _elementsFilter.end(); ++it) {
    // Only show certain selected cores from aie that are passed under cores
    // Ex. -r aie -e cores 0,3,5
    if(*it == "cores") {
      auto core_list = std::next(it);
      if (core_list != _elementsFilter.end())
        boost::split(aieCoreList, *core_list, boost::is_any_of(","));
    }
    // Show less information (core Status, Program Counter, Link Register, Stack
    // Pointer) for each cores.
    // Ex. -r aie -e less
    if(*it == "less") {
      is_less = true;
    }
  }

  // validate and print aie metadata by checking schema_version node
  if(!_pt.get_child_optional("aie_metadata.schema_version"))
    return;

  _output << "Aie\n";
  _output << boost::format("  %-10s\n") % _pt.get<std::string>("aie_metadata.description");

  try {
    int count = 0;
    for (auto& gr: _pt.get_child("aie_metadata.graphs")) {
      const boost::property_tree::ptree& graph = gr.second;
      _output << boost::format("  GRAPH[%2d] %-10s: %s\n") % graph.get<std::string>("id")
           % "Name" % graph.get<std::string>("name");
      _output << boost::format("            %-10s: %s\n") % "Status" % graph.get<std::string>("status");
      _output << boost::format("    SNo.  %-20s%-30s%-30s\n") % "Core [C:R]"
           % "Iteration_Memory [C:R]" % "Iteration_Memory_Addresses";
      for (auto& node : graph.get_child("tile")) {
        const boost::property_tree::ptree& tile = node.second;

	      if (tile.get<std::string>("memory_column", "") == "")
	        continue;

        _output << boost::format("    [%2d]   %-20s%-30s%-30d\n") % count
            % (tile.get<std::string>("column") + ":" + tile.get<std::string>("row"))
            % (tile.get<std::string>("memory_column") + ":" + tile.get<std::string>("memory_row"))
            % node.second.get<uint16_t>("memory_address");
        count++;
      }

      _output << std::endl;
      count = 0;
      for (auto& tile : graph.get_child("tile")) {
        int curr_core = count++;
        if(aieCoreList.size() && (std::find(aieCoreList.begin(), aieCoreList.end(),
            std::to_string(curr_core)) == aieCoreList.end()))
          continue;

        _output << boost::format("Core [%2d]\n") % curr_core;
        _output << fmt4("%d") % "Column" % tile.second.get<int>("column");
        _output << fmt4("%d") % "Row" % tile.second.get<int>("row");

        _output << boost::format("    %s:\n") % "Core";
        _output << fmt8("%s") % "Status" % tile.second.get<std::string>("core.status");
        _output << fmt8("%s") % "Program Counter" % tile.second.get<std::string>("core.program_counter");
        _output << fmt8("%s") % "Link Register" % tile.second.get<std::string>("core.link_register");
        _output << fmt8("%s") % "Stack Pointer" % tile.second.get<std::string>("core.stack_pointer");

	      if(is_less) {
	        _output << std::endl;
	        continue;
	      }

	      if(tile.second.find("dma") != tile.second.not_found()) {
          _output << boost::format("    %s:\n") % "DMA";

	        if(tile.second.find("dma.fifo") != tile.second.not_found()) {
            _output << boost::format("%12s:\n") % "FIFO";
            for(const auto& node : tile.second.get_child("dma.fifo.counters")) {
              _output << fmt16("%s") % node.second.get<std::string>("index")
		              % node.second.get<std::string>("count");
	          }
          }

          _output << boost::format("        %s:\n") % "MM2S";

          _output << boost::format("            %s:\n") % "Channel";
          for(const auto& node : tile.second.get_child("dma.mm2s.channel")) {
            _output << fmt16("%s") % "Id" % node.second.get<std::string>("id");
            _output << fmt16("%s") % "Channel Status" % node.second.get<std::string>("channel_status");
            _output << fmt16("%s") % "Queue Size" % node.second.get<std::string>("queue_size");
            _output << fmt16("%s") % "Queue Status" % node.second.get<std::string>("queue_status");
            _output << fmt16("%s") % "Current BD" % node.second.get<std::string>("current_bd");
            _output << std::endl;
          }

          _output << boost::format("        %s:\n") % "S2MM";

          _output << boost::format("            %s:\n") % "Channel";
          for(const auto& node : tile.second.get_child("dma.s2mm.channel")) {
            _output << fmt16("%s") % "Id" % node.second.get<std::string>("id");
            _output << fmt16("%s") % "Channel Status" % node.second.get<std::string>("channel_status");
            _output << fmt16("%s") % "Queue Size" % node.second.get<std::string>("queue_size");
            _output << fmt16("%s") % "Queue Status" % node.second.get<std::string>("queue_status");
            _output << fmt16("%s") % "Current BD" % node.second.get<std::string>("current_bd");
            _output << std::endl;
          }
        }

        if(tile.second.find("locks") != tile.second.not_found()) {
          _output << boost::format("    %s:\n") % "Locks";
          for(const auto& node : tile.second.get_child("locks", empty_ptree)) {
            _output << fmt8("%s")  % node.second.get<std::string>("name")
                                   % node.second.get<std::string>("value");
          }
          _output << std::endl;
        }

        if(tile.second.find("errors") != tile.second.not_found()) {
          _output << boost::format("    %s:\n") % "Errors";
          for(const auto& node : tile.second.get_child("errors", empty_ptree)) {
            _output << boost::format("        %s:\n") % node.second.get<std::string>("module");
            for(const auto& enode : node.second.get_child("error", empty_ptree)) {
              _output << fmt12("%s")  % enode.second.get<std::string>("name")
                                     % enode.second.get<std::string>("value");
            }
          }
          _output << std::endl;
        }

        if(tile.second.find("events") != tile.second.not_found()) {
          _output << boost::format("    %s:\n") % "Events";
          for(const auto& node : tile.second.get_child("events", empty_ptree)) {
            _output << fmt8("%s")  % node.second.get<std::string>("name")
                                   % node.second.get<std::string>("value");
          }
          _output << std::endl;
        }
      }

      const boost::property_tree::ptree& pl_kernel = graph.get_child("pl_kernel");
      if (!pl_kernel.empty()) {
        _output << boost::format("    %s\n") % "Pl Kernel Instances in Graph:";
        for (auto& node : graph.get_child("pl_kernel"))
          _output << boost::format("      %s\n") % node.second.data();
      }
      _output << std::endl;
    }

    count = 0;
    for (const auto& rtp_node : _pt.get_child("aie_metadata.rtps")) {

      _output << boost::format("  %-3s:[%2d]\n") % "RTP" % count;
      _output << fmtCommon("%s") % "Port Name" % rtp_node.second.get<std::string>("port_name");
      _output << fmtCommon("%d") % "Selector Row" % rtp_node.second.get<uint16_t>("selector_row");
      _output << fmtCommon("%d") % "Selector Column" % rtp_node.second.get<uint16_t>("selector_column");
      _output << fmtCommon("%d") % "Selector Lock Id" % rtp_node.second.get<uint16_t>("selector_lock_id");
      _output << fmtCommon("0x%x") % "Selector Address" %  rtp_node.second.get<uint64_t>("selector_address");
      _output << fmtCommon("%d") % "Ping Buffer Row" % rtp_node.second.get<uint16_t>("ping_buffer_row");
      _output << fmtCommon("%d") % "Ping Buffer Column" % rtp_node.second.get<uint16_t>("ping_buffer_column");
      _output << fmtCommon("%d") % "Ping Buffer Lock Id" % rtp_node.second.get<uint16_t>("ping_buffer_lock_id");
      _output << fmtCommon("0x%x") % "Ping Buffer Address" % rtp_node.second.get<uint64_t>("ping_buffer_address");
      _output << fmtCommon("%d") % "Pong Buffer Row" % rtp_node.second.get<uint16_t>("pong_buffer_row");
      _output << fmtCommon("%d") % "Pong Buffer Column" % rtp_node.second.get<uint16_t>("pong_buffer_column");
      _output << fmtCommon("%d") % "Pong Buffer Lock Id" % rtp_node.second.get<uint16_t>("pong_buffer_lock_id");
      _output << fmtCommon("0x%x") % "Pong Buffer Address" % rtp_node.second.get<uint64_t>("pong_buffer_address");
      _output << fmtCommon("%b") % "Is Plrtp" % rtp_node.second.get<bool>("is_pl_rtp");
      _output << fmtCommon("%b") % "Is Input" % rtp_node.second.get<bool>("is_input");
      _output << fmtCommon("%b") % "Is Async" % rtp_node.second.get<bool>("is_asynchronous");
      _output << fmtCommon("%b") % "Is Connected" % rtp_node.second.get<bool>("is_connected");
      _output << fmtCommon("%b") % "Require Lock" % rtp_node.second.get<bool>("requires_lock");
      count++;
      _output << std::endl;
    }
    _output << std::endl;

    count = 0;
    for (auto& gmio_node : _pt.get_child("aie_metadata.gmios")) {
      _output << boost::format("  %-4s: [%2d]\n") % "GMIO" % count;
      _output << fmtCommon("%s") % "Id" % gmio_node.second.get<std::string>("id");
      _output << fmtCommon("%s") % "Name" %gmio_node.second.get<std::string>("name");
      _output << fmtCommon("%s") % "Logical Name" % gmio_node.second.get<std::string>("logical_name");
      _output << fmtCommon("%d") % "Type" % gmio_node.second.get<uint16_t>("type");
      _output << fmtCommon("%d") % "Shim column" % gmio_node.second.get<uint16_t>("shim_column");
      _output << fmtCommon("%d") % "Channel Number" % gmio_node.second.get<uint16_t>("channel_number");
      _output << fmtCommon("%d") % "Stream Id" % gmio_node.second.get<uint16_t>("stream_id");
      _output << fmtCommon("%d") % "Burst Length in 16byte" % gmio_node.second.get<uint16_t>("burst_length_in_16byte");
      _output << fmtCommon("%s") % "PL Port Name" % gmio_node.second.get<std::string>("pl_port_name");
      _output << fmtCommon("%s") % "PL Parameter Name" % gmio_node.second.get<std::string>("pl_parameter_name");
      count++;
      _output << std::endl;
    }

  } catch(std::exception const& e) {
    _output <<  e.what() << std::endl;
  }
  _output << std::endl;
}
