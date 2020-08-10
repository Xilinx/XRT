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
#include "ReportAie.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include <boost/property_tree/json_parser.hpp>

#define fmtCommon(x) boost::format("    %-22s: " x "\n")

namespace qr = xrt_core::query;

const uint32_t major = 1;
const uint32_t minor = 0;
const uint32_t patch = 0;

boost::property_tree::ptree
populate_aie(const xrt_core::device * device, const std::string& desc)
{
  boost::property_tree::ptree pt;
  std::string aie_data;
  pt.put("description", desc);
  boost::property_tree::ptree graph_array;
  boost::property_tree::ptree _pt;

  try {
    aie_data = xrt_core::device_query<qr::aie_metadata>(device);
    std::stringstream ss(aie_data);
    boost::property_tree::read_json(ss, _pt);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return pt;
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
      igraph.put("id", ograph.get<std::string>("id"));
      igraph.put("name", ograph.get<std::string>("name"));
      auto row_it = gr.second.get_child("core_rows").begin();
      auto memcol_it = gr.second.get_child("iteration_memory_columns").begin();
      auto memrow_it = gr.second.get_child("iteration_memory_rows").begin();
      auto memaddr_it = gr.second.get_child("iteration_memory_addresses").begin();
      for (auto& node : gr.second.get_child("core_columns")) {
        boost::property_tree::ptree tile;
        tile.put("column", node.second.data());
        tile.put("row", row_it->second.data());
        tile.put("memory_column", memcol_it->second.data());
        tile.put("memory_row", memrow_it->second.data());
        tile.put("memory_address", memaddr_it->second.data());
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
ReportAie::writeReport(const xrt_core::device * _pDevice,
                       const std::vector<std::string> & /*_elementsFilter*/, 
                       std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "Aie\n";
  _output << boost::format("  %-10s\n") % _pt.get<std::string>("aie_metadata.description");

  try {
    for (auto& gr: _pt.get_child("aie_metadata.graphs")) {
      boost::property_tree::ptree& graph = gr.second;
      _output << boost::format("  GRAPH[%2d] Name:%2s\n") % graph.get<std::string>("id")
           % graph.get<std::string>("name");
      _output << boost::format("    SNo.  %-20s%-30s%-30s\n") % "Core [C:R]"
           % "Iteration_Memory [C:R]" % "Iteration_Memory_Addresses";
      int count = 0;
      for (auto& node : graph.get_child("tile")) {
        boost::property_tree::ptree& tile = node.second;
        _output << boost::format("    [%2d]   %-20s%-30s%-30d\n") % count
            % (tile.get<std::string>("column") + ":" + tile.get<std::string>("row"))
            % (tile.get<std::string>("memory_column") + ":" + tile.get<std::string>("memory_row"))
            % node.second.get<uint16_t>("memory_address");
        count++;
      }
      _output << std::endl;

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
