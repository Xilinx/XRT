/*
  Copyright (C) 2022 Xilinx, Inc
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
#include "ReportAieMem.h"
#include "core/common/info_aie.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#define fmt4(x) boost::format("%4s%-22s: " x "\n") % " "
#define fmt8(x) boost::format("%8s%-22s: " x "\n") % " "
#define fmt12(x) boost::format("%12s%-22s: " x "\n") % " "
#define fmt16(x) boost::format("%16s%-22s: " x "\n") % " "

boost::property_tree::ptree
populate_aie_mem(const xrt_core::device * _pDevice, const std::string& desc)
{
  xrt::device device(_pDevice->get_device_id());
  boost::property_tree::ptree pt_mem;
  pt_mem.put("description", desc);
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::aie_mem>();
  boost::property_tree::read_json(ss, pt_mem);

  return pt_mem;
}

void
ReportAieMem::getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                                   boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportAieMem::getPropertyTree20202(const xrt_core::device * _pDevice, 
                                boost::property_tree::ptree &_pt) const
{
  _pt.add_child("aie_mem_status", populate_aie_mem(_pDevice, "Aie_Mem_Status"));
}

void 
ReportAieMem::writeReport( const xrt_core::device* /*_pDevice*/,
                            const boost::property_tree::ptree& _pt, 
                            const std::vector<std::string>& _elementsFilter,
                            std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;
  std::vector<std::string> aieTileList;

  _output << "AIE\n";

  // Loop through all the parameters given under _elementsFilter i.e. -e option
  for (auto it = _elementsFilter.begin(); it != _elementsFilter.end(); ++it) {
    // Only show certain selected tiles from aiemem that are passed under tiles
    // Ex. -r aiemem -e tiles 0,3,5
    if(*it == "tiles") {
      auto tile_list = std::next(it);
      if (tile_list != _elementsFilter.end())
        boost::split(aieTileList, *tile_list, boost::is_any_of(","));
    }
  }

  try {
    int count = 0;
    const boost::property_tree::ptree ptMemTiles = _pt.get_child("aie_mem_status.tiles", empty_ptree);

    if (ptMemTiles.empty()) {
      _output << "  <AIE Mem tiles information unavailable>" << std::endl << std::endl;
      return;
    }

    _output << "  Mem Status" << std::endl;

    for (auto &tile : ptMemTiles) {
      int curr_tile = count++;
      if(aieTileList.size() && (std::find(aieTileList.begin(), aieTileList.end(),
	                       std::to_string(curr_tile)) == aieTileList.end()))
        continue;

      _output << boost::format("Tile[%2d]\n") % curr_tile;
      _output << fmt4("%d") % "Column" % tile.second.get<int>("column");
      _output << fmt4("%d") % "Row" % tile.second.get<int>("row");
      if(tile.second.find("dma") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "DMA";
        
        _output << boost::format("%12s:\n") % "FIFO";
        for(auto& node : tile.second.get_child("dma.fifo.counters")) {
          _output << fmt16("%s") % node.second.get<std::string>("index")
                    % node.second.get<std::string>("count");
        }

        _output << boost::format("        %s:\n") % "MM2S";

        _output << boost::format("            %s:\n") % "Channel";
        for(auto& node : tile.second.get_child("dma.mm2s.channel")) {
          _output << fmt16("%s") % "Id" % node.second.get<std::string>("id");
          _output << fmt16("%s") % "Channel Status" % node.second.get<std::string>("channel_status");
          _output << fmt16("%s") % "Queue Size" % node.second.get<std::string>("queue_size");
          _output << fmt16("%s") % "Queue Status" % node.second.get<std::string>("queue_status");
          _output << fmt16("%s") % "Current BD" % node.second.get<std::string>("current_bd");
          _output << std::endl;
        }

        _output << boost::format("        %s:\n") % "S2MM";

        _output << boost::format("            %s:\n") % "Channel";
        for(auto& node : tile.second.get_child("dma.s2mm.channel")) {
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
        for(auto& node : tile.second.get_child("locks",empty_ptree)) {
          _output << fmt8("%s")  % node.second.get<std::string>("name")
                                 % node.second.get<std::string>("value");
        }
        _output << std::endl;
      }

      if(tile.second.find("errors") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "Errors";
        for(auto& node : tile.second.get_child("errors",empty_ptree)) {
          _output << boost::format("        %s:\n") % node.second.get<std::string>("module");
          for(auto& enode : node.second.get_child("error",empty_ptree)) {
            _output << fmt12("%s")  % enode.second.get<std::string>("name")
                                    % enode.second.get<std::string>("value");
          }
        }
        _output << std::endl;
      }

      if(tile.second.find("events") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "Events";
        for(auto& node : tile.second.get_child("events",empty_ptree)) {
          _output << fmt8("%s")  % node.second.get<std::string>("name")
                                 % node.second.get<std::string>("value");
        }
        _output << std::endl;
      }
    }
  } catch(std::exception const& e) {
    _output <<  e.what() << std::endl;
  }
  _output << std::endl;
}