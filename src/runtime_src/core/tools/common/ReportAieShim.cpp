/*
  Copyright (C) 2021 Xilinx, Inc
 
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

#define fmt4(x) boost::format("%4s%-22s: " x "\n") % " "
#define fmt8(x) boost::format("%8s%-22s: " x "\n") % " "
#define fmt12(x) boost::format("%12s%-22s: " x "\n") % " "
#define fmt16(x) boost::format("%16s%-22s: " x "\n") % " "


namespace qr = xrt_core::query;

static void
addnodelist(std::string search_str, std::string node_str,boost::property_tree::ptree& input_pt, boost::property_tree::ptree& output_pt)
{
  boost::property_tree::ptree pt_array;
  for (auto& node: input_pt.get_child(search_str)) {
    boost::property_tree::ptree pt;
    std::string val;
    for (auto& value: node.second)
      val +=(val.empty()?"":", ")+ value.second.data();

    pt.put("name", node.first);
    pt.put("value", val);
    pt_array.push_back(std::make_pair("", pt));
  }
  output_pt.add_child(node_str, pt_array);
}

/*
{
    "aie_shim": {
        "0_0": {
            "col": "0",
            "row": "0",
            "dma": {
                "Channel_status": {
                    "MM2S": [
                        "Running"
                    ],
                    "S2MM": [
                        "Stalled_on_lock"
                    ]
                },
                "Mode": {
                    "MM2S": [
                        "A_B"
                    ],
                    "S2MM": [
                        "FIFO",
                        "Packet_Switching"
                    ]
                },
                "Queue_size": {
                    "MM2S": [
                        "2"
                    ],
                    "S2MM": [
                        "3"
                    ]
                },
                "Queue_status": {
                    "MM2S": [
                        "channel0_overflow"
                    ],
                    "S2MM": [
                        "channel0_overflow"
                    ]
                },
                "Current_BD": {
                    "MM2S": [
                        "3"
                    ],
                    "S2MM": [
                        "2"
                    ]
                },
                "Lock_ID": {
                    "MM2S": [
                        "1"
                    ],
                    "S2MM": [
                        "2"
                    ]
                }
            },
            "lock": {
                "lock0": [
                    "Acquired_for_read"
                ],
                "lock1": [
                    "Acquired_for_write"
                ]
            },
            "errors": {
                "Core_module": {
                    "Bus": [
                        "AXI-MM_slave_error"
                    ]
                },
                "Memory_module": {
                    "ECC": [
                        "DM_ECC_error_scrub_2-bit",
                        "DM_ECC_error_2-bit"
                    ]
                },
                "PL_module": {
                    "DMA": [
                        "DMA_S2MM_0_error",
                        "DMA_MM2S_1_error"
                    ]
                }
            },
            "event": {
                "Core_module": [
                    "Perf_Cnt0",
                    "PC_0",
                    "Memory_Stall"
                ],
                "Memory_module": [
                    "Lock_0_Acquired",
                    "DMA_S2MM_0_go_to_idle"
                ],
                "PL_module": [
                    "DMA_S2MM_0_Error",
                    "Lock_0_Acquired"
                ]
            }
        },
        ....
*/
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

      std::string mode;
      boost::property_tree::ptree empty_pt;
      for (auto& node: oshim.get_child("dma.Mode.MM2S", empty_pt)) {
        mode +=(mode.empty()?"":", ")+node.second.data();
      }

      if(!mode.empty())
        ishim.put("dma.mm2s.mode", mode);

      mode.clear();
      for (auto& node: oshim.get_child("dma.Mode.S2MM", empty_pt)) {
        mode +=(mode.empty()?"":", ")+node.second.data();
      }

      if(!mode.empty())
        ishim.put("dma.s2mm.mode", mode);

      boost::property_tree::ptree mm2s_array;
      auto queue_size = oshim.get_child("dma.Queue_size.MM2S").begin();
      auto queue_status = oshim.get_child("dma.Queue_status.MM2S").begin();
      auto current_bd = oshim.get_child("dma.Current_BD.MM2S").begin();
      auto lock_id = oshim.get_child("dma.Lock_ID.MM2S").begin();
      int id = 0;
      for (auto& node : oshim.get_child("dma.Channel_status.MM2S")) {
        boost::property_tree::ptree channel;
        channel.put("id", id);
        channel.put("channel_status", node.second.data());
        channel.put("queue_size", queue_size->second.data());
        channel.put("queue_status", queue_status->second.data());
        channel.put("current_bd", current_bd->second.data());
        channel.put("lock_id", lock_id->second.data());
        queue_size++;
        queue_status++;
        current_bd++;
        lock_id++;
        mm2s_array.push_back(std::make_pair("", channel));
      }
      ishim.add_child("dma.mm2s.channel", mm2s_array);

      boost::property_tree::ptree s2mm_array;
      queue_size = oshim.get_child("dma.Queue_size.S2MM").begin();
      queue_status = oshim.get_child("dma.Queue_status.S2MM").begin();
      current_bd = oshim.get_child("dma.Current_BD.S2MM").begin();
      lock_id = oshim.get_child("dma.Lock_ID.S2MM").begin();
      id = 0;
      for (auto& node : oshim.get_child("dma.Channel_status.S2MM")) {
        boost::property_tree::ptree channel;
        channel.put("id", id);
        channel.put("channel_status", node.second.data());
        channel.put("queue_size", queue_size->second.data());
        channel.put("queue_status", queue_status->second.data());
        channel.put("current_bd", current_bd->second.data());
        channel.put("lock_id", lock_id->second.data());
        queue_size++;
        queue_status++;
        current_bd++;
        lock_id++;
        s2mm_array.push_back(std::make_pair("", channel));
      }
      ishim.add_child("dma.s2mm.channel", s2mm_array);

      addnodelist("lock", "locks", oshim, ishim);
      boost::property_tree::ptree module_array;
      for (auto& node : oshim.get_child("errors", empty_pt)) {
        boost::property_tree::ptree module;
        module.put("module",node.first);
        boost::property_tree::ptree type_array;
        for (auto& type : node.second) {
          boost::property_tree::ptree enode;
          enode.put("name",type.first);
          std::string val;
          for (auto& value: type.second)
            val +=(val.empty()?"":", ")+ value.second.data();
          enode.put("value", val);
          type_array.push_back(std::make_pair("", enode));
        }
        module.add_child("error", type_array);
        module_array.push_back(std::make_pair("", module));
      }
      ishim.add_child("errors", module_array);

      addnodelist("event", "events", oshim, ishim);
      tile_array.push_back(std::make_pair("tile"+std::to_string(col),ishim));
    }
    pt.add_child("tiles",tile_array);
  } catch (const std::exception& ex){
    pt.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE shim"));
  }

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
ReportAieShim::writeReport( const xrt_core::device* /*_pDevice*/,
                            const boost::property_tree::ptree& _pt, 
                            const std::vector<std::string>& /*_elementsFilter*/,
                            std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  _output << "AIE\n";

  try {
    int count = 0;
    const boost::property_tree::ptree ptShimTiles = _pt.get_child("aie_shim_status.tiles", empty_ptree);

    if (ptShimTiles.empty()) {
      _output << "  <AIE information unavailable>" << std::endl << std::endl;
      return;
    }

    _output << "  Shim Status" << std::endl;

    for (auto &tile : ptShimTiles) {
      _output << boost::format("Tile[%2d]\n") % count++;
      _output << fmt4("%d") % "Column" % tile.second.get<int>("column");
      _output << fmt4("%d") % "Row" % tile.second.get<int>("row");
      if(tile.second.find("dma") != tile.second.not_found()) {
        _output << boost::format("    %s:\n") % "DMA";
        _output << boost::format("        %s:\n") % "MM2S";
        _output << fmt12("%s") % "Mode" % tile.second.get<std::string>("dma.mm2s.mode");

        _output << boost::format("            %s:\n") % "Channel";
        for(auto& node : tile.second.get_child("dma.mm2s.channel")) {
          _output << fmt16("%s") % "Id" % node.second.get<std::string>("id");
          _output << fmt16("%s") % "Channel Status" % node.second.get<std::string>("channel_status");
          _output << fmt16("%s") % "Queue Size" % node.second.get<std::string>("queue_size");
          _output << fmt16("%s") % "Queue Status" % node.second.get<std::string>("queue_status");
          _output << fmt16("%s") % "Current BD" % node.second.get<std::string>("current_bd");
          _output << fmt16("%s") % "Lock ID" % node.second.get<std::string>("lock_id");
          _output << std::endl;
        }

        _output << boost::format("        %s:\n") % "S2MM";
        _output << fmt12("%s") % "Mode" % tile.second.get<std::string>("dma.s2mm.mode");

        _output << boost::format("            %s:\n") % "Channel";
        for(auto& node : tile.second.get_child("dma.s2mm.channel")) {
          _output << fmt16("%s") % "Id" % node.second.get<std::string>("id");
          _output << fmt16("%s") % "Channel Status" % node.second.get<std::string>("channel_status");
          _output << fmt16("%s") % "Queue Size" % node.second.get<std::string>("queue_size");
          _output << fmt16("%s") % "Queue Status" % node.second.get<std::string>("queue_status");
          _output << fmt16("%s") % "Current BD" % node.second.get<std::string>("current_bd");
          _output << fmt16("%s") % "Lock ID" % node.second.get<std::string>("lock_id");
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
