/**
 * Copyright (C) 2021-2022 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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
#define XRT_CORE_COMMON_SOURCE
#include "asd_parser.h"
#include "info_aie.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include <boost/algorithm/string.hpp>
#include <boost/optional/optional.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace qr = xrt_core::query;
using ptree_type = boost::property_tree::ptree;

namespace {
// Convert graph status from integer to a human readable
// string format for better understanding.
inline std::string
graph_status_to_string(int status) {
  switch(status)
  {
    case 0:	return "stop";
    case 1:	return "reset";
    case 2:	return "running";
    case 3:	return "suspend";
    case 4:	return "end";
    default:	return "unknown"; // Consider graph status as "unknown" for all the default cases
  }
}

// Add the node to the ouput list
static void
addnodelist(const std::string& search_str, const std::string& node_str,
	    const boost::property_tree::ptree& input_pt, boost::property_tree::ptree& output_pt)
{
  boost::property_tree::ptree pt_array;
  for (const auto& node : input_pt.get_child(search_str)) {
    boost::property_tree::ptree pt;
    std::string val;
    for (const auto& value : node.second) {
      val += val.empty() ? "" : ", ";
      val += value.second.data();
    }

    pt.put("name", node.first);
    pt.put("value", val);
    pt_array.push_back({"", pt});
  }
  output_pt.add_child(node_str, pt_array);
}

// This function extract DMA information for both AIE core and tiles
static void
populate_aie_dma(const boost::property_tree::ptree& pt, boost::property_tree::ptree& pt_dma)
{
  boost::property_tree::ptree fifo_pt;
  boost::property_tree::ptree mm2s_array;
  boost::property_tree::ptree s2mm_array;
  boost::property_tree::ptree empty_pt;

  // Extract FIFO COUNT information
  // Sample sysfs entry for DMA
  // cat /sys/devices/platform/<aie_path>/aiepart_0_50/47_0/dma
  // ichannel_status: mm2s: idle|idle, s2mm: idle|idle
  // queue_size: mm2s: 0|0, s2mm: 0|0
  // queue_status: mm2s: okay|okay, s2mm: okay|okay
  // current_bd: mm2s: 0|0, s2mm: 0|0
  // fifo_len: 0|0
  int id = 0;
  for (const auto& node : pt.get_child("dma.fifo_len", empty_pt)) {
    std::string index = "Counter";
    boost::property_tree::ptree fifo_counter;
    index += std::to_string(id++);
    fifo_counter.put("index", index);
    fifo_counter.put("count", node.second.data());
    fifo_pt.push_back({"", fifo_counter});
  }

  pt_dma.add_child("dma.fifo.counters", fifo_pt);

  auto queue_size = pt.get_child("dma.queue_size.mm2s", empty_pt).begin();
  auto queue_status = pt.get_child("dma.queue_status.mm2s", empty_pt).begin();
  auto current_bd = pt.get_child("dma.current_bd.mm2s", empty_pt).begin();
  id = 0;

  for (const auto& node : pt.get_child("dma.channel_status.mm2s", empty_pt)) {
    boost::property_tree::ptree channel;
    channel.put("id", id++);
    channel.put("channel_status", node.second.data());
    channel.put("queue_size", queue_size->second.data());
    channel.put("queue_status", queue_status->second.data());
    channel.put("current_bd", current_bd->second.data());
    queue_size++;
    queue_status++;
    current_bd++;
    mm2s_array.push_back({"", channel});
  }

  pt_dma.add_child("dma.mm2s.channel", mm2s_array);

  queue_size = pt.get_child("dma.queue_size.s2mm", empty_pt).begin();
  queue_status = pt.get_child("dma.queue_status.s2mm", empty_pt).begin();
  current_bd = pt.get_child("dma.current_bd.s2mm", empty_pt).begin();
  id = 0;
  for (const auto& node : pt.get_child("dma.channel_status.s2mm", empty_pt)) {
    boost::property_tree::ptree channel;
    channel.put("id", id++);
    channel.put("channel_status", node.second.data());
    channel.put("queue_size", queue_size->second.data());
    channel.put("queue_status", queue_status->second.data());
    channel.put("current_bd", current_bd->second.data());
    queue_size++;
    queue_status++;
    current_bd++;
    s2mm_array.push_back({"", channel});
  }

  pt_dma.add_child("dma.s2mm.channel", s2mm_array);

  return;
}

// This function extract ERRORs information for both AIE core and tiles
static void
populate_aie_errors(const boost::property_tree::ptree& pt, boost::property_tree::ptree& pt_err)
{
  boost::property_tree::ptree module_array;
  boost::property_tree::ptree empty_pt;

  for (const auto& node : pt.get_child("errors", empty_pt)) {
    boost::property_tree::ptree module;
    module.put("module",node.first);
    boost::property_tree::ptree type_array;
    for (const auto& type : node.second) {
      boost::property_tree::ptree enode;
      enode.put("name", type.first);
      std::string val;
      for (const auto& value: type.second) {
        val += val.empty() ? "" : ", ";
	val += value.second.data();
      }

      enode.put("value", val);
      type_array.push_back({"", enode});
    }

    module.add_child("error", type_array);
    module_array.push_back({"", module});
  }

  pt_err.add_child("errors", module_array);

  return;
}

/*
 * Sample AIE Shim output
{
    "aie_shim": {
        "0_0": {
            "col": "0",
            "row": "0",
            "dma": {
                "channel_status": {
                    "mm2s": [
                        "Running"
                    ],
                    "s2mm": [
                        "Stalled_on_lock"
                    ]
                },
                "queue_size": {
                    "mm2s": [
                        "2"
                    ],
                    "s2mm": [
                        "3"
                    ]
                },
                "queue_status": {
                    "mm2s": [
                        "channel0_overflow"
                    ],
                    "s2mm": [
                        "channel0_overflow"
                    ]
                },
                "current_bd": {
                    "mm2s": [
                        "3"
                    ],
                    "s2mm": [
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
                "core": {
                    "Bus": [
                        "AXI-MM_slave_error"
                    ]
                },
                "memory": {
                    "ECC": [
                        "DM_ECC_error_scrub_2-bit",
                        "DM_ECC_error_2-bit"
                    ]
                },
                "pl": {
                    "DMA": [
                        "DMA_S2MM_0_error",
                        "DMA_MM2S_1_error"
                    ]
                }
            },
            "event": {
                "core": [
                    "Perf_Cnt0",
                    "PC_0",
                    "Memory_Stall"
                ],
                "memory": [
                    "Lock_0_Acquired",
                    "DMA_S2MM_0_go_to_idle"
                ],
                "pl": [
                    "DMA_S2MM_0_Error",
                    "Lock_0_Acquired"
                ]
            }
        },
        ....
}
*/

// Populate the AIE Shim information from the input XRT device
boost::property_tree::ptree
populate_aie_shim(const xrt_core::device *device, const std::string& desc)
{
  boost::property_tree::ptree pt;
  pt.put("description", desc);
  boost::property_tree::ptree pt_shim;

  // Read AIE Shim information of the device
  try {
    // On Edge platforms this info is populated using sysfs
    std::string aie_data = xrt_core::device_query<qr::aie_shim_info_sysfs>(device);
    std::stringstream ss(aie_data);
    boost::property_tree::read_json(ss, pt_shim);
  }
  catch (const qr::no_such_key&) {
    // Not Edge device
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
    return pt;
  }

  try {
    // On Pcie platforms use driver calls to get AIE Shim info
    pt_shim = asd_parser::get_formated_tiles_info(device, asd_parser::aie_tile_type::shim);
  }
  catch (const xrt_core::query::no_such_key&) {
    // Not Pcie device
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
    return pt;
  }

  if (pt_shim.empty()) {
    // AIE Shim tile not available
    pt.put("error_msg", "AIE Shim tile information is not available");
    return pt;
  }

  try {
    boost::property_tree::ptree tile_array;

    // Populate the shim information such as dma, lock, error, events
    // for each tiles.
    for (const auto& as: pt_shim.get_child("aie_shim")) {
      const boost::property_tree::ptree& oshim = as.second;
      boost::property_tree::ptree ishim;
      int col = oshim.get<uint32_t>("col");
      int row = oshim.get<uint32_t>("row");

      ishim.put("column", col);
      ishim.put("row", row);

      // Populate DMA information
      if (oshim.find("dma") != oshim.not_found())
        populate_aie_dma(oshim, ishim);

      // Populate ERROR information
      if (oshim.find("errors") != oshim.not_found())
        populate_aie_errors(oshim, ishim);

      // Populate LOCK information
      if (oshim.find("lock") != oshim.not_found())
        addnodelist("lock", "locks", oshim, ishim);

      // Populate EVENT information
      if (oshim.find("event") != oshim.not_found())
        addnodelist("event", "events", oshim, ishim);

      tile_array.push_back({"tile" + std::to_string(col), ishim});
    }

    pt.add_child("tiles", tile_array);

  }
  catch (const std::exception& ex) {
    pt.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE shim"));
  }

  return pt;
}

// Populate the AIE Mem tile information from the input XRT device
boost::property_tree::ptree
populate_aie_mem(const xrt_core::device* device, const std::string& desc)
{
  boost::property_tree::ptree pt;
  pt.put("description", desc);
  boost::property_tree::ptree pt_mem;

  // Read AIE Mem information of the device
  try {
    // On Edge platforms this info is populated using sysfs
    std::string aie_data = xrt_core::device_query<qr::aie_mem_info_sysfs>(device);
    std::stringstream ss(aie_data);
    boost::property_tree::read_json(ss, pt_mem);
  }
  catch (const xrt_core::query::no_such_key&) {
    // Not Edge device
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
    return pt;
  }

  try {
    // On Pcie platforms use driver calls to get AIE mem info
    pt_mem = asd_parser::get_formated_tiles_info(device, asd_parser::aie_tile_type::mem);
  }
  catch (const xrt_core::query::no_such_key&) {
    // Not Pcie device
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
    return pt;
  }

  if (pt_mem.empty()) {
    // AIE Mem tile not available
    pt.put("error_msg", "AIE Mem tile information is not available");
    return pt;
  }

  try {
    boost::property_tree::ptree tile_array;

    // Populate the mem tile information such as dma, lock, error, events
    // for each tiles.
    for (const auto& am: pt_mem.get_child("aie_mem")) {
      const boost::property_tree::ptree& imem = am.second;
      boost::property_tree::ptree omem;
      int col = imem.get<uint32_t>("col");
      int row = imem.get<uint32_t>("row");

      omem.put("column", col);
      omem.put("row", row);

      // Populate DMA information
      if (imem.find("dma") != imem.not_found())
        populate_aie_dma(imem, omem);

      // Populate ERROR information
      if (imem.find("errors") != imem.not_found())
        populate_aie_errors(imem, omem);

      // Populate LOCK information
      if (imem.find("lock") != imem.not_found())
        addnodelist("lock", "locks", imem, omem);

      // Populate EVENT information
      if (imem.find("event") != imem.not_found())
        addnodelist("event", "events", imem, omem);

      tile_array.push_back({"tile" + std::to_string(col), omem});
    }

    pt.add_child("tiles", tile_array);

  }
  catch (const std::exception& ex) {
    pt.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE shim"));
  }

  return pt;
}

// Populate the AIE Mem tile information from the input XRT device
boost::property_tree::ptree
populate_aie_partition(const xrt_core::device* device, const std::string& desc)
{
  boost::property_tree::ptree pt;
  const auto data = xrt_core::device_query<qr::aie_partition_info>(device);
  // Group the hw contexts based on their which AIE partitions they use
  std::map<std::tuple<uint64_t, uint64_t>, boost::property_tree::ptree> pt_map;
  for (const auto entry : data) {
    auto partition = pt_map.emplace(std::make_tuple(entry.start_col, entry.num_cols), boost::property_tree::ptree());

    boost::property_tree::ptree pt_entry;
    pt_entry.put("xclbin_uuid", entry.xclbin_uuid);
    pt_entry.put("slot_id", entry.slot_id);
    pt_entry.put("usage_count", entry.usage_count);
    pt_entry.put("migration_count", entry.migration_count);
    pt_entry.put("bo_sync_count", entry.bo_sync_count);

    partition.first->second.push_back(std::make_pair("", pt_entry));
  }

  boost::property_tree::ptree pt_data;
  for (const auto entry : pt_map) {
    boost::property_tree::ptree pt_entry;
    pt_entry.put("start_col", std::get<0>(entry.first));
    pt_entry.put("num_cols", std::get<1>(entry.first));
    pt_entry.add_child("hw_contexts", entry.second);
    pt_data.push_back(std::make_pair("", pt_entry));
  }
  pt.add_child("aie_partitions", pt_data);
  return pt;
}

/*
 * Sample AIE Core output
{
    "aie_core": {
        "0_0": {
            "col": "0",
            "row": "1",
            "core": {
                "status": [
                    "Enabled",
                    "North_Lock_Stall"
                ],
                "pc": [
                    "0x12345678"
                ],
                "lr": [
                    "0x45678901"
                ],
                "sp": [
                    "0x78901234"
                ]
            },
            "dma": {
                "channel_status": {
                    "mm2s": [
                        "Running"
                    ],
                    "s2mm": [
                        "Stalled_on_lock"
                    ]
                },
                "queue_size": {
                    "mm2s": [
                        "2"
                    ],
                    "s2mm": [
                        "3"
                    ]
                },
                "queue_status": {
                    "mm2s": [
                        "channel0_overflow"
                    ],
                    "s2mm": [
                        "channel0_overflow"
                    ]
                },
                "current_bd": {
                    "mm2s": [
                        "3"
                    ],
                    "s2mm": [
                        "2"
                    ]
                },
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
                "core": {
                    "Bus": [
                        "AXI-MM_slave_error"
                    ]
                },
                "memory": {
                    "ECC": [
                        "DM_ECC_error_scrub_2-bit",
                        "DM_ECC_error_2-bit"
                    ]
                },
                "pl": {
                    "DMA": [
                        "DMA_S2MM_0_error",
                        "DMA_MM2S_1_error"
                    ]
                }
            },
            "event": {
                "core": [
                    "Perf_Cnt0",
                    "PC_0",
                    "Memory_Stall"
                ],
                "memory": [
                    "Lock_0_Acquired",
                    "DMA_S2MM_0_go_to_idle"
                ],
                "pl": [
                    "DMA_S2MM_0_Error",
                    "Lock_0_Acquired"
                ]
            }
        },
        ....
*/

// Populate a specific AIE core given as an input of [row:col]
void
populate_aie_core(const boost::property_tree::ptree& pt_core, boost::property_tree::ptree& tile)
{
  try {
    boost::property_tree::ptree pt;
    boost::property_tree::ptree empty_pt;

    auto row = tile.get<int>("row");
    auto col = tile.get<int>("column");
    pt = pt_core.get_child("aie_core." + std::to_string(col) + "_" + std::to_string(row));

    std::string status;
    for (const auto& node: pt.get_child("core.status", empty_pt)) {
      status += status.empty() ? "" : ", ";
      status += node.second.data();
    }

    if (!status.empty())
      tile.put("core.status", status);

    for (const auto& node: pt.get_child("core.pc", empty_pt))
      tile.put("core.program_counter", node.second.data());

    for (const auto& node: pt.get_child("core.lr", empty_pt))
      tile.put("core.link_register", node.second.data());

    for (const auto& node: pt.get_child("core.sp", empty_pt))
      tile.put("core.stack_pointer", node.second.data());

    // Add DMA information to the tiles
    if (pt.find("dma") != pt.not_found())
      populate_aie_dma(pt, tile);

    // Add ERROR information to the tiles
    if (pt.find("errors") != pt.not_found())
      populate_aie_errors(pt, tile);

    // Add LOCK information to the tiles
    if (pt.find("lock") != pt.not_found())
      addnodelist("lock", "locks", pt, tile);

    // Add EVENT information to the tiles
    if (pt.find("event") != pt.not_found())
      addnodelist("event", "events", pt, tile);

  }
  catch (const std::exception& ex) {
    tile.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE core"));
  }
}

// Populate RTPs for AIE Cores
void
populate_aie_core_rtp(const boost::property_tree::ptree& pt, boost::property_tree::ptree& pt_array)
{
  boost::property_tree::ptree rtp_array;
  boost::property_tree::ptree empty_pt;

  for (const auto& rtp_node : pt.get_child("aie_metadata.RTPs", empty_pt)) {
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
    rtp_array.push_back({rtp_node.first, rtp});
  }

  pt_array.add_child("rtps", rtp_array);
}

// Populate GMIOs for AIE Cores
void
populate_aie_core_gmio(const boost::property_tree::ptree& pt, boost::property_tree::ptree& pt_array)
{
  boost::property_tree::ptree gmio_array;
  boost::property_tree::ptree empty_pt;

  for (const auto& gmio_node : pt.get_child("aie_metadata.GMIOs", empty_pt)) {
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
    gmio_array.push_back({gmio_node.first, gmio});
  }

  pt_array.add_child("gmios",gmio_array);
}

// Check for duplicate entry in tile_array for the same core [row:col]
bool
is_duplicate_core(const boost::property_tree::ptree& tile_array, boost::property_tree::ptree& tile)
{
  const auto row = tile.get<int>("row");
  const auto col = tile.get<int>("column");
  for (auto& node : tile_array) {
    if ((node.second.get<int>("column") == col) && (node.second.get<int>("row") == row))
      return true;
  }

  return false;
}

// Populate a specific AIE core which is unused but memory buffers exist [row:col]
void
populate_buffer_only_cores(const boost::property_tree::ptree& pt,
			   const boost::property_tree::ptree& core_info, int gr_id,
			   boost::property_tree::ptree& tile_array)
{
  const boost::property_tree::ptree empty_pt;

  for (const auto& g_node : pt.get_child("aie_metadata.EventGraphs", empty_pt)) {
    if (gr_id != g_node.second.get<int>("id"))
      continue;

    boost::property_tree::ptree igraph;
    auto dma_row_it = g_node.second.get_child("dma_rows", empty_pt).begin();
    for (const auto& node : g_node.second.get_child("dma_columns", empty_pt)) {
      boost::property_tree::ptree tile;
      tile.put("column", node.second.data());
      tile.put("row", dma_row_it->second.data());
      // Check whether this core is already added
      if (is_duplicate_core(tile_array, tile))
        continue;

      populate_aie_core(core_info, tile);
      tile_array.push_back({"", tile});
      if (dma_row_it != g_node.second.end())
        dma_row_it++;
    }
  }
}

// Populate AIE core information from aie metadata
static void
populate_aie_from_metadata(const xrt_core::device* device, boost::property_tree::ptree& pt_aie,
                           boost::property_tree::ptree& pt)
{
  boost::property_tree::ptree graph_array;
  boost::property_tree::ptree gh_status;
  boost::property_tree::ptree core_info;
  boost::property_tree::ptree empty_pt;

  try {
    std::vector<std::string> graph_status = xrt_core::device_query<qr::graph_status>(device);
    std::stringstream ss;
    std::copy(graph_status.begin(), graph_status.end(), std::ostream_iterator<std::string>(ss));
    boost::property_tree::read_json(ss, gh_status);
  }
  catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }

  try {
    // version checks are done in below call
    std::string aie_core_data = xrt_core::device_query<qr::aie_core_info_sysfs>(device);
    std::stringstream ss(aie_core_data);
    boost::property_tree::read_json(ss, core_info);
  }
  catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return;
  }

  /*
   * sample AIE json for Edge platforms which can be parsed
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

  pt.put("schema_version.major", pt_aie.get<uint32_t>("schema_version.major"));
  pt.put("schema_version.minor", pt_aie.get<uint32_t>("schema_version.minor"));
  pt.put("schema_version.patch", pt_aie.get<uint32_t>("schema_version.patch"));

  // Extract Graphs from aie_metadata and populate the aie
  for (auto& gr: pt_aie.get_child("aie_metadata.graphs", empty_pt)) {
    boost::property_tree::ptree& igraph = gr.second;
    boost::property_tree::ptree ograph;
    boost::property_tree::ptree tile_array;
    ograph.put("id", igraph.get<std::string>("id"));
    int gr_id = igraph.get<int>("id");
    ograph.put("name", igraph.get<std::string>("name"));
    ograph.put("status", graph_status_to_string(gh_status.get<int>("graphs." + igraph.get<std::string>("name"), -1)));
    auto row_it = gr.second.get_child("core_rows").begin();
    auto memcol_it = gr.second.get_child("iteration_memory_columns").begin();
    auto memrow_it = gr.second.get_child("iteration_memory_rows").begin();
    auto memaddr_it = gr.second.get_child("iteration_memory_addresses").begin();
    for (const auto& node : gr.second.get_child("core_columns", empty_pt)) {
      boost::property_tree::ptree tile;
      tile.put("column", node.second.data());
      tile.put("row", row_it->second.data());
      tile.put("memory_column", memcol_it->second.data());
      tile.put("memory_row", memrow_it->second.data());
      tile.put("memory_address", memaddr_it->second.data());
      populate_aie_core(core_info, tile);
      row_it++;
      memcol_it++;
      memrow_it++;
      memaddr_it++;
      tile_array.push_back({"", tile});
    }

    populate_buffer_only_cores(pt_aie, core_info, gr_id, tile_array);

    boost::property_tree::ptree plkernel_array;
    // Get the name of the kernls available for this graph
    for (const auto& node : gr.second.get_child("pl_kernel_instance_names", empty_pt)) {
      boost::property_tree::ptree plkernel;
      plkernel.put("", node.second.data());
      plkernel_array.push_back({"", plkernel});
    }

    ograph.add_child("tile", tile_array);
    ograph.add_child("pl_kernel", plkernel_array);
    graph_array.push_back({"", ograph});
  }
  pt.add_child("graphs", graph_array);

  // Extract RTPs from aie_metadata and populate the aie_core
  populate_aie_core_rtp(pt_aie, pt);

  // Extract GMIOs from aie_metadata and populate the aie_core
  populate_aie_core_gmio(pt_aie, pt);
}

// TODO: Remove this function
static void
add_dummy_graphs(boost::property_tree::ptree& pt, boost::property_tree::ptree& tile_array)
{
  boost::property_tree::ptree graph;
  boost::property_tree::ptree graph_array;

  graph.put("id", "");
  graph.put("name", "");
  graph.put("status", "");
  graph.add_child("tile", tile_array);
  
  graph_array.push_back({"", graph});
    
  pt.add_child("graphs", graph_array);
}

// Populate AIE core information
static void
populate_aie_helper(const xrt_core::device* device, boost::property_tree::ptree& pt)
{
  boost::property_tree::ptree core_info;

  try {
    boost::property_tree::ptree tile_array;
    asd_parser::aie_tiles_info tiles_info{0};
    uint32_t cols_filled = 0;

    core_info = asd_parser::get_formated_tiles_info(device, asd_parser::aie_tile_type::core, tiles_info,
                                                    cols_filled);

    for (uint16_t col = 0; col < tiles_info.cols; col++) {
      // skip this col if not filled
      if (!(cols_filled & (1 << col)))
        continue;

      for (uint16_t row = tiles_info.core_row_start; row < tiles_info.core_row_start + tiles_info.core_rows; row++) {
        boost::property_tree::ptree tile;
        tile.put("column", col);
        tile.put("row", row);
        populate_aie_core(core_info, tile);

        tile_array.push_back({"", tile});
      }
    }

    // Top level reporter expects graphs, so adding dummy entries
    // TODO: remove this function
    add_dummy_graphs(pt, tile_array);
  }
  catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return;
  }
}

boost::property_tree::ptree
populate_aie(const xrt_core::device* device, const std::string& desc)
{
  boost::property_tree::ptree pt;

  pt.put("description", desc); 
  try {
    boost::property_tree::ptree pt_aie;
    std::string aie_data = xrt_core::device_query<qr::aie_metadata>(device);
    std::stringstream ss(aie_data);
    boost::property_tree::read_json(ss, pt_aie);

    populate_aie_from_metadata(device, pt_aie, pt);
    return pt;
  }
  catch (const qr::no_such_key&) {
    // pcie platforms dont have aie metadata
  }
  catch (const std::exception& ex){
    pt.put("error_msg", (boost::format("%s %s") % ex.what() % "found in the AIE Metadata"));
    return pt;
  }

  // Populate aie info for PCIe platforms
  populate_aie_helper(device, pt);
  return pt;
}

} //unnamed namespace

namespace xrt_core { namespace aie {

// Get AIE core information for this device
ptree_type
aie_core(const xrt_core::device* device)
{
  return populate_aie(device, "Aie_Metadata");
}

// Get AIE shim information for this device
ptree_type
aie_shim(const xrt_core::device* device)
{
  return populate_aie_shim(device, "Aie_Shim_Status");
}

// Get AIE memory information for this device
ptree_type
aie_mem(const xrt_core::device* device)
{
  return populate_aie_mem(device, "Aie_Mem_Status");
}

// Get AIE partition information for this device
ptree_type
aie_partition(const xrt_core::device* device)
{
  return populate_aie_partition(device, "Aie_Partitions");
}

}} // aie, xrt
