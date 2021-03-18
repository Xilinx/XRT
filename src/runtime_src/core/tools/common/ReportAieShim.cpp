/*
  Copyright (C) 2020 Xilinx, Inc
 
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
#include "ReportAieShim.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include <boost/optional/optional.hpp>
#include <boost/property_tree/json_parser.hpp>

#define fmt4(x) boost::format("    %-22s: " x "\n")
#define fmt8(x) boost::format("        %-22s: " x "\n")
#define fmt12(x) boost::format("            %-22s: " x "\n")
#define fmt16(x) boost::format("                %-22s: " x "\n")

namespace qr = xrt_core::query;

inline void
addnode(std::string str, std::string nodestr,boost::property_tree::ptree& oshim, boost::property_tree::ptree& ishim)
{
  int count = 0;
  boost::property_tree::ptree empty_pt;
  for (auto& node: oshim.get_child(str, empty_pt)) {
    ishim.put(nodestr+std::to_string(count++), node.second.data());
  }
}

void
populate_aie_shim(const xrt_core::device * device, const std::string& desc, boost::property_tree::ptree& pt)
{
  pt.put("description", desc);
  boost::property_tree::ptree _pt;

  try {
  std::string aie_data = xrt_core::device_query<qr::aie_shim_info>(device);
    std::stringstream ss(aie_data);
    boost::property_tree::read_json(ss, _pt);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return;
  }

  try {
    boost::property_tree::ptree tile_array;
    for (auto& as: _pt.get_child("aie_shim")) {
      boost::property_tree::ptree& oshim = as.second;
      boost::property_tree::ptree ishim;
      int col = oshim.get<uint32_t>("col");
      int row = oshim.get<uint32_t>("row");
      
      ishim.put("column", col);
      ishim.put("row", row);
      int count = 0;

      boost::property_tree::ptree empty_pt;
      for (auto& node: oshim.get_child("dma.Channel_status.MM2S", empty_pt)) {
        ishim.put("dma.channel_status.mm2s.channel_"+std::to_string(count++), node.second.data());
      }

      count = 0;
      for (auto& node: oshim.get_child("dma.Channel_status.S2MM", empty_pt)) {
        ishim.put("dma.channel_status.s2mm.channel_"+std::to_string(count++), node.second.data());
      }

      count = 0;
      std::string mode;
      for (auto& node: oshim.get_child("dma.Mode.MM2S", empty_pt)) {
        mode +=(mode.empty()?"":", ")+node.second.data();
      }

      if(!mode.empty())
        ishim.put("dma.mode.mm2s", mode);

      mode.clear();
      for (auto& node: oshim.get_child("dma.Mode.S2MM", empty_pt)) {
        mode +=(mode.empty()?"":", ")+node.second.data();
      }

      if(!mode.empty())
        ishim.put("dma.mode.s2mm", mode);

      addnode("dma.Queue_size.MM2S", "dma.queue_size.mm2s.channel_", oshim, ishim);
      addnode("dma.Queue_size.S2MM", "dma.queue_size.s2mm.channel_", oshim, ishim);
      addnode("dma.Queue_status.MM2S", "dma.queue_status.mm2s.channel_", oshim, ishim);
      addnode("dma.Queue_status.S2MM", "dma.queue_status.s2mm.channel_", oshim, ishim);
      addnode("dma.Current_BD.MM2S", "dma.current_bd.mm2s.channel_", oshim, ishim);
      addnode("dma.Current_BD.S2MM", "dma.current_bd.s2mm.channel_", oshim, ishim);
      addnode("dma.Lock_ID.MM2S", "dma.lock_id.mm2s.channel_", oshim, ishim);
      addnode("dma.Lock_ID.S2MM", "dma.lock_id.s2mm.channel_", oshim, ishim);

      count = 0;
      boost::property_tree::ptree lockarray;
      for (auto& node: oshim.get_child("lock")) {
        std::string val;
        for (auto& l: node.second)
          val +=(val.empty()?"":", ")+l.second.data();
        if(!val.empty())
          ishim.put("lock."+node.first, val);
      }

      for (auto& node: oshim.get_child("errors.PL_module", empty_pt)) {
        std::string val;
        for (auto& l: node.second)
          val +=(val.empty()?"":", ")+l.second.data();
        if(!val.empty())
          ishim.put("errors.pl_module."+node.first, val);
      }

      for (auto& node: oshim.get_child("errors.Core_module", empty_pt)) {
        std::string val;
        for (auto& l: node.second)
          val +=(val.empty()?"":", ")+l.second.data();
        if(!val.empty())
          ishim.put("errors.core_module."+node.first, val);
      }

      for (auto& node: oshim.get_child("errors.Memory_module", empty_pt)) {
        std::string val;
        for (auto& l: node.second)
          val +=(val.empty()?"":", ")+l.second.data();
        if(!val.empty())
          ishim.put("errors.memory_module."+node.first, val);
      }

      for (auto& node: oshim.get_child("event", empty_pt)) {
        std::string val;
        for (auto& l: node.second)
          val +=(val.empty()?"":", ")+l.second.data();
        if(!val.empty())
          ishim.put("event."+node.first, val);
      }

      for (auto& node: oshim.get_child("event.Memory_module", empty_pt)) {
        std::string val;
        for (auto& l: node.second)
          val +=(val.empty()?"":", ")+l.second.data();
        if(!val.empty())
          ishim.put("event.memory_module."+node.first, val);
      }

      for (auto& node: oshim.get_child("event.Core_module", empty_pt)) {
        std::string val;
        for (auto& l: node.second)
          val +=(val.empty()?"":", ")+l.second.data();
        if(!val.empty())
          ishim.put("event.core_module."+node.first, val);
      }

      tile_array.push_back(std::make_pair("tile"+std::to_string(col),ishim));
    }
    pt.add_child("tiles",tile_array);
  } catch (const std::exception& ex){
    pt.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE shim"));
  }

  //return pt;
}

void
ReportAieShim::getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                                   boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportAieShim::getPropertyTree20202(const xrt_core::device * _pDevice, 
                                boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  populate_aie_shim(_pDevice, "Aie_Shim_Status", pt);
  _pt.add_child("aie_shim_status", pt);
}

void 
ReportAieShim::writeReport(const xrt_core::device * _pDevice,
                       const std::vector<std::string> & /*_elementsFilter*/, 
                       std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "Aie\n";
  _output << boost::format("  %-10s\n") % _pt.get<std::string>("aie_shim_status.description");

  try {
    int count = 0;
    for (auto& tile: _pt.get_child("aie_shim_status.tiles")) {
      _output << boost::format("Tile[%2d]\n") % count;
      _output << fmt4("%d") % "Column" % tile.second.get<int>("column");
      _output << fmt4("%d") % "Row" % tile.second.get<int>("row");
      if(tile.second.find("dma") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "DMA";
        _output << boost::format("        %s:\n") % "Channel Status";

        _output << boost::format("            %s:\n") % "MM2S";
        for(auto& n : tile.second.get_child("dma.channel_status.mm2s")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("            %s:\n") % "S2MM";
        for(auto& n : tile.second.get_child("dma.channel_status.s2mm")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("        %s:\n") % "Mode";

        for(auto& n : tile.second.get_child("dma.mode")) {
          _output << fmt12("%s") % n.first % n.second.data();
        }

        _output << boost::format("        %s:\n") % "Queue Size";

        _output << boost::format("            %s:\n") % "MM2S";
        for(auto& n : tile.second.get_child("dma.queue_size.mm2s")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("            %s:\n") % "S2MM";
        for(auto& n : tile.second.get_child("dma.queue_size.s2mm")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("        %s:\n") % "Queue Status";

        _output << boost::format("            %s:\n") % "MM2S";
        for(auto& n : tile.second.get_child("dma.queue_status.mm2s")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("            %s:\n") % "S2MM";
        for(auto& n : tile.second.get_child("dma.queue_status.s2mm")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("        %s:\n") % "Current BD";

        _output << boost::format("            %s:\n") % "MM2S";
        for(auto& n : tile.second.get_child("dma.current_bd.mm2s")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("            %s:\n") % "S2MM";
        for(auto& n : tile.second.get_child("dma.current_bd.s2mm")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("        %s:\n") % "Lock ID";

        _output << boost::format("            %s:\n") % "MM2S";
        for(auto& n : tile.second.get_child("dma.lock_id.mm2s")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }

        _output << boost::format("            %s:\n") % "S2MM";
        for(auto& n : tile.second.get_child("dma.lock_id.s2mm")) {
          _output << fmt16("%s") % n.first % n.second.data();
        }
      } 

      if(tile.second.find("lock") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "Lock";
        for(auto& n : tile.second.get_child("lock",empty_ptree)) {
          _output << fmt8("%s") % n.first % n.second.data();
        }
      }

      if(tile.second.find("errors") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "Errors";
        int c = 0;
        for(auto& n : tile.second.get_child("errors.pl_module",empty_ptree)) {
          if(c == 0)
            _output << boost::format("        %s:\n") % "PL Module";
          _output << fmt12("%s") % n.first % n.second.data();
          c++;
        }
        c = 0;
        for(auto& n : tile.second.get_child("errors.core_module",empty_ptree)) {
          if(c == 0)
            _output << boost::format("        %s:\n") % "Core Module";
          _output << fmt12("%s") % n.first % n.second.data();
          c++;
        }
        c = 0;
        for(auto& n : tile.second.get_child("errors.memory_module",empty_ptree)) {
          if(c == 0)
            _output << boost::format("        %s:\n") % "Memory Module";
          _output << fmt12("%s") % n.first % n.second.data();
          c++;
        }
      }

      if(tile.second.find("event") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "Event";
        for(auto& n : tile.second.get_child("event",empty_ptree)) {
          _output << fmt12("%s") % n.first % n.second.data();
        }
      }
      count++;
    } 
  } catch(std::exception const& e) {
    _output <<  e.what() << std::endl;
  }
  _output << std::endl;
}
