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
#include "ReportAie.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include <boost/property_tree/json_parser.hpp>
namespace qr = xrt_core::query;

boost::property_tree::ptree
populate_aie(const xrt_core::device * device, const std::string& loc_id, const std::string& desc)
{
  boost::property_tree::ptree pt;
  std::string aie_data;
  pt.put("aiedata", loc_id);
  pt.put("description", desc);
  try {
    aie_data = xrt_core::device_query<qr::aie_metadata>(device);
    std::stringstream ss(aie_data);
    boost::property_tree::ptree _pt;
    boost::property_tree::read_json(ss, _pt);
    pt.add_child("data", _pt);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }
  
  
  return pt;
}

void
ReportAie::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportAie::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree aie_array;
  aie_array.push_back(std::make_pair("", populate_aie(_pDevice, "aie_metadata", "Aie_Metadata")));

  // There can only be 1 root node
  _pt.add_child("aie_metadata", aie_array);
}

void 
ReportAie::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "Aie\n";
  boost::property_tree::ptree& aies = _pt.get_child("aie_metadata", empty_ptree);
  for(auto& kv : aies) {
    boost::property_tree::ptree& aie_data = kv.second;
    _output << boost::format("  %-10s\n") % aie_data.get<std::string>("description");
        boost::property_tree::ptree& pt = aie_data.get_child("data", empty_ptree);
        try {
            for (auto& gr: pt.get_child("aie_metadata.graphs")) {
                std::string name,col,row,memcol,memrow,id,memaddr;
                boost::property_tree::ptree& graph = gr.second;
                _output << boost::format("  GRAPH[%2d] Name:%2s\n") % graph.get<std::string>("id") % graph.get<std::string>("name");
                _output << boost::format("    SNo.  %-20s%-30s%-30s\n") % "Core [C:R]" % "Iteration_Memory [C:R]" % "Iteration_Memory_Addresses";
                int count = 0;
                auto row_it = gr.second.get_child("core_rows").begin();
                auto memcol_it = gr.second.get_child("iteration_memory_columns").begin();
                auto memrow_it = gr.second.get_child("iteration_memory_rows").begin();
                auto memaddr_it = gr.second.get_child("iteration_memory_addresses").begin();
                for (auto& node : gr.second.get_child("core_columns")) {
                    col = node.second.data();
                    row = row_it->second.data();
                    memcol = memcol_it->second.data();
                    memrow = memrow_it->second.data();
                    memaddr = memaddr_it->second.data();
                    _output << boost::format("    [%2d]   %-20s%-30s%-30d\n") % count % (col+":"+row) % (memcol+":"+memrow) % memaddr;
                    row_it++;memcol_it++;memrow_it++;memaddr_it++;count++;
                }
                _output << std::endl;

                _output << boost::format("    %s\n") % "Pl Kernel Instances in Graph:";
                std::string plname;
                for (auto& node : gr.second.get_child("pl_kernel_instance_names")) {
                    plname = node.second.data();
                    _output << boost::format("      %s\n") % plname;
                }
                _output << std::endl;
            }

            int count = 0;
            for (auto& rtp_node : pt.get_child("aie_metadata.RTPs")) {
                _output << boost::format("  RTPs:[%2d]\n") % count;
                _output << boost::format("    name:%-25s\n") % rtp_node.second.get<std::string>("port_name");
                _output << boost::format("    selector_row:%-25d\n") % rtp_node.second.get<uint16_t>("selector_row");

                _output << boost::format("    selector_col:%-25d\n") % rtp_node.second.get<uint16_t>("selector_column");
                _output << boost::format("    selector_lock_id:%-25d\n") % rtp_node.second.get<uint16_t>("selector_lock_id");
                _output << boost::format("    selector_addr:%-25d\n") %  rtp_node.second.get<uint64_t>("selector_address");

                _output << boost::format("    ping_row:%-25d\n") % rtp_node.second.get<uint16_t>("ping_buffer_row");
                _output << boost::format("    ping_col:%-25d\n") % rtp_node.second.get<uint16_t>("ping_buffer_column");
                _output << boost::format("    ping_lock_id:%-25d\n") % rtp_node.second.get<uint16_t>("ping_buffer_lock_id");
                _output << boost::format("    ping_addr:%-25d\n") % rtp_node.second.get<uint64_t>("ping_buffer_address");

                _output << boost::format("    pong_row:%-25d\n") % rtp_node.second.get<uint16_t>("pong_buffer_row");
                _output << boost::format("    pong_col:%-25d\n") % rtp_node.second.get<uint16_t>("pong_buffer_column");
                _output << boost::format("    pong_lock_id:%-25d\n") % rtp_node.second.get<uint16_t>("pong_buffer_lock_id");
                _output << boost::format("    pong_addr:%-25d\n") % rtp_node.second.get<uint64_t>("pong_buffer_address");

                _output << boost::format("    is_plrtp:%-25b\n") % rtp_node.second.get<bool>("is_PL_RTP");
                _output << boost::format("    is_input:%-25b\n") % rtp_node.second.get<bool>("is_input");
                _output << boost::format("    is_async:%-25b\n") % rtp_node.second.get<bool>("is_asynchronous");
                _output << boost::format("    is_connected:%-25b\n") % rtp_node.second.get<bool>("is_connected");
                _output << boost::format("    require_lock:%-25b\n") % rtp_node.second.get<bool>("requires_lock");
                count++;
                _output << std::endl;
            }
            _output << std::endl;

            count = 0;
            for (auto& gmio_node : pt.get_child("aie_metadata.GMIOs")) {
                _output << boost::format("  GMIOs:[%2d]\n") % count;
                _output << boost::format("    id:%-25s\n") % gmio_node.second.get<std::string>("id");
                _output << boost::format("    name:%-25s\n") % gmio_node.second.get<std::string>("name");
                _output << boost::format("    logical_name:%-25s\n") % gmio_node.second.get<std::string>("logical_name");
                _output << boost::format("    type:%-25d\n") % gmio_node.second.get<uint16_t>("type");
                _output << boost::format("    shim_col:%-25d\n") % gmio_node.second.get<uint16_t>("shim_column");
                _output << boost::format("    channel_number:%-25d\n") % gmio_node.second.get<uint16_t>("channel_number");
                _output << boost::format("    stream_id:%-25d\n") % gmio_node.second.get<uint16_t>("stream_id");
                _output << boost::format("    burst_len:%-25d\n") % gmio_node.second.get<uint16_t>("burst_length_in_16byte");
                _output << boost::format("    PL_port_name:%-25d\n") % gmio_node.second.get<std::string>("PL_port_name");
                _output << boost::format("    PL_parameter_name:%-25d\n") % gmio_node.second.get<std::string>("PL_parameter_name");
                count++;
                _output << std::endl;
            }

        } catch(std::exception const& e) {
            // eat the exception, probably bad path
        }
  }
  _output << std::endl;
}
