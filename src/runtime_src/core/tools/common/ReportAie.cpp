/*
  Copyright (C) 2020-2021 Xilinx, Inc
 
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
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include <boost/optional/optional.hpp>
#include <boost/property_tree/json_parser.hpp>

#define fmtCommon(x) boost::format("    %-22s: " x "\n")
#define fmt4(x) boost::format("%4s%-22s: " x "\n") % " "
#define fmt8(x) boost::format("%8s%-22s: " x "\n") % " "
#define fmt12(x) boost::format("%12s%-22s: " x "\n") % " "
#define fmt16(x) boost::format("%16s%-22s: " x "\n") % " "

namespace qr = xrt_core::query;

const uint32_t major = 1;
const uint32_t minor = 0;
const uint32_t patch = 0;

enum graph_state {   
  STOP = 0,
  RESET = 1,
  RUNNING = 2,
  SUSPEND = 3,
  END = 4
};

inline std::string
graph_status_to_string(int status) {
  switch(status)
  {
    case STOP:    return std::string("stop");
    case RESET:   return std::string("reset");
    case RUNNING: return std::string("running");
    case SUSPEND: return std::string("suspend");
    case END:     return std::string("end");
    default:      return std::string("idle");
  }
}

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
    "aie_core": {
        "0_0": {
            "col": "0",
            "row": "1",
            "core": {
                "Status": [
                    "Enabled",
                    "North_Lock_Stall"
                ],
                "PC": [
                    "0x12345678"
                ],
                "LR": [
                    "0x45678901"
                ],
                "SP": [
                    "0x78901234"
                ]
            },
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
populate_aie_core(boost::property_tree::ptree _pt,boost::property_tree::ptree& tile, int row, int col)
{
  try {
    boost::property_tree::ptree pt;
    boost::property_tree::ptree empty_pt;
    pt =  _pt.get_child("aie_core."+std::to_string(col)+"_"+std::to_string(row));
    tile.put("column", pt.get<uint32_t>("col"));
    tile.put("row", pt.get<uint32_t>("row"));

    std::string status;
    for (auto& node: pt.get_child("core.Status", empty_pt)) {
      status +=(status.empty()?"":", ")+node.second.data();
    }
    if(!status.empty())
      tile.put("core.status", status);

    for (auto& node: pt.get_child("core.PC", empty_pt))
      tile.put("core.program_counter", node.second.data());

    for (auto& node: pt.get_child("core.LR", empty_pt))
      tile.put("core.link_register", node.second.data());

    for (auto& node: pt.get_child("core.SP", empty_pt))
      tile.put("core.stack_pointer", node.second.data());

    std::string mode;
    for (auto& node: pt.get_child("dma.Mode.MM2S", empty_pt)) {
      mode += (mode.empty()?"":", ")+node.second.data();
    }

    if(!mode.empty())
      tile.put("dma.mm2s.mode", mode);

    mode.clear();
    for (auto& node: pt.get_child("dma.Mode.S2MM", empty_pt)) {
      mode += (mode.empty()?"":", ")+node.second.data();
    }

    if(!mode.empty())
      tile.put("dma.s2mm.mode", mode);

    boost::property_tree::ptree mm2s_array;
    auto queue_size = pt.get_child("dma.Queue_size.MM2S").begin();
    auto queue_status = pt.get_child("dma.Queue_status.MM2S").begin();
    auto current_bd = pt.get_child("dma.Current_BD.MM2S").begin();
    auto lock_id = pt.get_child("dma.Lock_ID.MM2S").begin();
    int id = 0;
    for (auto& node : pt.get_child("dma.Channel_status.MM2S")) {
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
    tile.add_child("dma.mm2s.channel", mm2s_array);

    boost::property_tree::ptree s2mm_array;
    queue_size = pt.get_child("dma.Queue_size.S2MM").begin();
    queue_status = pt.get_child("dma.Queue_status.S2MM").begin();
    current_bd = pt.get_child("dma.Current_BD.S2MM").begin();
    lock_id = pt.get_child("dma.Lock_ID.S2MM").begin();
    id = 0;
    for (auto& node : pt.get_child("dma.Channel_status.S2MM")) {
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
    tile.add_child("dma.s2mm.channel", s2mm_array);

    boost::property_tree::ptree module_array;
    for (auto& node : pt.get_child("errors", empty_pt)) {
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
    tile.add_child("errors", module_array);

    addnodelist("lock", "locks", pt, tile);
    addnodelist("event", "events", pt, tile);
  } catch (const std::exception& ex){
    tile.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE core"));
  }
}

boost::property_tree::ptree
populate_aie(const xrt_core::device * device, const std::string& desc)
{
  boost::property_tree::ptree pt;
  std::string aie_data;
  std::string aie_core_data;
  std::vector<std::string> graph_status;
  pt.put("description", desc);
  boost::property_tree::ptree graph_array;
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree _gh_status;
  boost::property_tree::ptree _core_info;

  try {
    aie_data = xrt_core::device_query<qr::aie_metadata>(device);
    std::stringstream ss(aie_data);
    boost::property_tree::read_json(ss, _pt);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return pt;
  }

  try {
    aie_core_data = xrt_core::device_query<qr::aie_core_info>(device);
    std::stringstream ss(aie_core_data);
    boost::property_tree::read_json(ss, _core_info);
    //boost::property_tree::write_json(std::cout, _core_info);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return pt;
  }

  try {
    graph_status = xrt_core::device_query<qr::graph_status>(device);
    std::stringstream ss;
    std::copy(graph_status.begin(), graph_status.end(), std::ostream_iterator<std::string>(ss));
    boost::property_tree::read_json(ss, _gh_status);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }

  try {
    if(_pt.get<uint32_t>("schema_version.major") != major ||
         _pt.get<uint32_t>("schema_version.minor") != minor ||
         _pt.get<uint32_t>("schema_version.patch") != patch ) {
      pt.put("error_msg", (boost::format("major:minor:patch [%d:%d:%d] version are not matching")
          % _pt.get<uint32_t>("schema_version.major")
          % _pt.get<uint32_t>("schema_version.minor")
          % _pt.get<uint32_t>("schema_version.patch")));
      return pt;
    }

  pt.put("schema_version.major", major);
  pt.put("schema_version.minor", minor);
  pt.put("schema_version.patch", patch);

 /*
  sample AIE json which can be parsed
  {
   "schema_version":{
                     "major":"1",
                     "minor":"0",
                     "patch":"0"
                     },
   "aie_metadata":{
                   "graphs":{
                             "graph0":{
                                       "id":"",
                                       "name":"",
                                       "core_columns":[""],
                                       "core_rows":[""],
                                       "iteration_memory_columns":[""],
                                       "iteration_memory_rows":[""],
                                       "iteration_memory_addresses":[""],
                                       "multirate_triggers":[""],
                                       "pl_kernel_instance_names":[""],
                                       "pl_axi_lite_modes":[""]
                                       }
                             },
                   "RTPs":{
                           "rtp0":{
                                   "port_id":"",
                                   "alias_id":"",
                                   "port_name":"",
                                   "alias_name":"",
                                   "graph_id":"",
                                   "is_input":"",
                                   "is_asynchronous":"",
                                   "is_connected":"",
                                   "element_type":"",
                                   "number_of_bytes":"",
                                   "is_PL_RTP":"",
                                   "requires_lock":"",
                                   "selector_column":"",
                                   "selector_row":"",
                                   "selector_address":"",
                                   "selector_lock_id":"",
                                   "ping_buffer_column":"",
                                   "ping_buffer_row":"",
                                   "ping_buffer_address":"",
                                   "ping_buffer_lock_id":"",
                                   "pong_buffer_column":"",
                                   "pong_buffer_row":"",
                                   "pong_buffer_address":"",
                                   "pong_buffer_lock_id":"",
                                   "pl_kernel_instance_name":"",
                                   "pl_parameter_index":""
                                   }, 
                          },
                   "GMIOs":{
                            "gmio0":{
                                      "id":"",
                                      "name":"",
                                      "logical_name":"",
                                      "type":"",
                                      "shim_column":"",
                                      "channel_number":"",
                                      "stream_id":"",
                                      "burst_length_in_16byte":"",
                                      "PL_port_name":"",
                                      "PL_parameter_name":""
                                      }
                           }
                   }
  }
 */
    for (auto& gr: _pt.get_child("aie_metadata.graphs")) {
      boost::property_tree::ptree& ograph = gr.second;
      boost::property_tree::ptree igraph;
      boost::property_tree::ptree tile_array;
      boost::property_tree::ptree core_array;
      igraph.put("id", ograph.get<std::string>("id"));
      igraph.put("name", ograph.get<std::string>("name"));
      igraph.put("status",graph_status_to_string(_gh_status.get<int>("graphs." + ograph.get<std::string>("name"), -1)));
      auto row_it = gr.second.get_child("core_rows").begin();
      auto memcol_it = gr.second.get_child("iteration_memory_columns").begin();
      auto memrow_it = gr.second.get_child("iteration_memory_rows").begin();
      auto memaddr_it = gr.second.get_child("iteration_memory_addresses").begin();
      for (auto& node : gr.second.get_child("core_columns")) {
        boost::property_tree::ptree tile;
        boost::property_tree::ptree core_tile;
        tile.put("column", node.second.data());
        tile.put("row", row_it->second.data());
        tile.put("memory_column", memcol_it->second.data());
        tile.put("memory_row", memrow_it->second.data());
        tile.put("memory_address", memaddr_it->second.data());
        int row = tile.get<int>("row");
        int col = tile.get<int>("column");
        populate_aie_core(_core_info,tile,row,col);
        row_it++;
        memcol_it++;
        memrow_it++;
        memaddr_it++;
        tile_array.push_back(std::make_pair("", tile));
      }

      boost::property_tree::ptree plkernel_array;
      for (auto& node : gr.second.get_child("pl_kernel_instance_names")) {
        boost::property_tree::ptree plkernel;
        plkernel.put("", node.second.data());
        plkernel_array.push_back(std::make_pair("", plkernel));
      }

      igraph.add_child("tile", tile_array);
      igraph.add_child("pl_kernel", plkernel_array);
      graph_array.push_back(std::make_pair("", igraph));
    }
    pt.add_child("graphs", graph_array);


    boost::property_tree::ptree rtp_array;
    for (auto& rtp_node : _pt.get_child("aie_metadata.RTPs")) {
      boost::property_tree::ptree rtp;

      rtp.put("port_name", rtp_node.second.get<std::string>("port_name"));
      rtp.put("selector_row", rtp_node.second.get<uint16_t>("selector_row"));
      rtp.put("selector_column", rtp_node.second.get<uint16_t>("selector_column"));
      rtp.put("selector_lock_id", rtp_node.second.get<uint16_t>("selector_lock_id"));
      rtp.put("selector_address", rtp_node.second.get<uint64_t>("selector_address"));

      rtp.put("ping_buffer_row", rtp_node.second.get<uint16_t>("ping_buffer_row"));
      rtp.put("ping_buffer_column", rtp_node.second.get<uint16_t>("ping_buffer_column"));
      rtp.put("ping_buffer_lock_id", rtp_node.second.get<uint16_t>("ping_buffer_lock_id"));
      rtp.put("ping_buffer_address", rtp_node.second.get<uint64_t>("ping_buffer_address"));

      rtp.put("pong_buffer_row", rtp_node.second.get<uint16_t>("pong_buffer_row"));
      rtp.put("pong_buffer_column", rtp_node.second.get<uint16_t>("pong_buffer_column"));
      rtp.put("pong_buffer_lock_id", rtp_node.second.get<uint16_t>("pong_buffer_lock_id"));
      rtp.put("pong_buffer_address", rtp_node.second.get<uint64_t>("pong_buffer_address"));

      rtp.put("is_pl_rtp", rtp_node.second.get<bool>("is_PL_RTP"));
      rtp.put("is_input", rtp_node.second.get<bool>("is_input"));
      rtp.put("is_asynchronous", rtp_node.second.get<bool>("is_asynchronous"));
      rtp.put("is_connected", rtp_node.second.get<bool>("is_connected"));
      rtp.put("requires_lock", rtp_node.second.get<bool>("requires_lock"));
      rtp_array.push_back(std::make_pair(rtp_node.first, rtp));
    }
    pt.add_child("rtps", rtp_array);


    boost::property_tree::ptree gmio_array;
    for (auto& gmio_node : _pt.get_child("aie_metadata.GMIOs")) {
      boost::property_tree::ptree gmio;
      gmio.put("id", gmio_node.second.get<std::string>("id"));
      gmio.put("name", gmio_node.second.get<std::string>("name"));
      gmio.put("logical_name", gmio_node.second.get<std::string>("logical_name"));
      gmio.put("type", gmio_node.second.get<uint16_t>("type"));
      gmio.put("shim_column", gmio_node.second.get<uint16_t>("shim_column"));
      gmio.put("channel_number", gmio_node.second.get<uint16_t>("channel_number"));
      gmio.put("stream_id", gmio_node.second.get<uint16_t>("stream_id"));
      gmio.put("burst_length_in_16byte", gmio_node.second.get<uint16_t>("burst_length_in_16byte"));
      gmio.put("pl_port_name", gmio_node.second.get<std::string>("PL_port_name","N/A"));
      gmio.put("pl_parameter_name", gmio_node.second.get<std::string>("PL_parameter_name","N/A"));
      gmio_array.push_back(std::make_pair(gmio_node.first, gmio));
    }
    pt.add_child("gmios",gmio_array);

  } catch (const std::exception& ex){
    pt.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE Metadata"));
  }

  return pt;
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
                       const std::vector<std::string>& /*_elementsFilter*/, 
                       std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;

  // validate and print aie metadata by checking schema_version node
  if(!_pt.get_child_optional("aie_metadata.schema_version"))
    return;

  _output << "Aie\n";
  _output << boost::format("  %-10s\n") % _pt.get<std::string>("aie_metadata.description");

  try {
    for (auto& gr: _pt.get_child("aie_metadata.graphs")) {
      const boost::property_tree::ptree& graph = gr.second;
      _output << boost::format("  GRAPH[%2d] %-10s: %s\n") % graph.get<std::string>("id")
           % "Name" % graph.get<std::string>("name");
      _output << boost::format("            %-10s: %s\n") % "Status" % graph.get<std::string>("status");
      _output << boost::format("    SNo.  %-20s%-30s%-30s\n") % "Core [C:R]"
           % "Iteration_Memory [C:R]" % "Iteration_Memory_Addresses";
      int count = 0;
      for (auto& node : graph.get_child("tile")) {
        const boost::property_tree::ptree& tile = node.second;
        _output << boost::format("    [%2d]   %-20s%-30s%-30d\n") % count
            % (tile.get<std::string>("column") + ":" + tile.get<std::string>("row"))
            % (tile.get<std::string>("memory_column") + ":" + tile.get<std::string>("memory_row"))
            % node.second.get<uint16_t>("memory_address");
        count++;
      }
      _output << std::endl;
      count = 0;
      for (auto& tile : graph.get_child("tile")) {
        _output << boost::format("Core [%2d]\n") % count++;
        _output << fmt4("%d") % "Column" % tile.second.get<int>("column");
        _output << fmt4("%d") % "Row" % tile.second.get<int>("row");

        _output << boost::format("    %s:\n") % "Core";
        _output << fmt8("%s") % "Status" % tile.second.get<std::string>("core.status");
        _output << fmt8("%s") % "Program Counter" % tile.second.get<std::string>("core.program_counter");
        _output << fmt8("%s") % "Link Register" % tile.second.get<std::string>("core.link_register");
        _output << fmt8("%s") % "Stack Pointer" % tile.second.get<std::string>("core.stack_pointer");
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
      _output << boost::format("    %s\n") % "Pl Kernel Instances in Graph:";
      for (auto& node : graph.get_child("pl_kernel")) {
        _output << boost::format("      %s\n") % node.second.data();
      }
      _output << std::endl;
    }

    int count = 0;
    for (auto& rtp_node : _pt.get_child("aie_metadata.rtps")) {
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
