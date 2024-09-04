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

#include "aie_parser.h"
#include "core/common/device.h"
#include "core/include/xclbin.h"

#include <sstream>
#include <string>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/optional.hpp>

namespace {

namespace pt = boost::property_tree;
using tile_type = xrt_core::edge::aie::tile_type;
using gmio_type = xrt_core::edge::aie::gmio_type;
using counter_type = xrt_core::edge::aie::counter_type;
using module_type = xrt_core::edge::aie::module_type;

static constexpr uint32_t default_id = 1;
static constexpr uint32_t default_start_column = 0;

inline void
throw_if_error(bool err, const char* msg)
{
  if (err)
    throw std::runtime_error(msg);
}

static void
read_aie_metadata(const char* data, size_t size, pt::ptree& aie_project)
{
  std::stringstream aie_stream;
  aie_stream.write(data,size);
  pt::read_json(aie_stream,aie_project);
}

adf::driver_config
get_driver_config(const pt::ptree& aie_meta)
{
  adf::driver_config driver_config;
  driver_config.hw_gen = aie_meta.get<uint8_t>("aie_metadata.driver_config.hw_gen");
  driver_config.base_address = aie_meta.get<uint64_t>("aie_metadata.driver_config.base_address");
  driver_config.column_shift = aie_meta.get<uint8_t>("aie_metadata.driver_config.column_shift");
  driver_config.row_shift = aie_meta.get<uint8_t>("aie_metadata.driver_config.row_shift");
  driver_config.num_columns = aie_meta.get<uint8_t>("aie_metadata.driver_config.num_columns");
  driver_config.num_rows = aie_meta.get<uint8_t>("aie_metadata.driver_config.num_rows");
  driver_config.shim_row = aie_meta.get<uint8_t>("aie_metadata.driver_config.shim_row");
  if (!aie_meta.get_optional<uint8_t>("aie_metadata.driver_config.mem_tile_row_start") ||
      !aie_meta.get_optional<uint8_t>("aie_metadata.driver_config.mem_tile_num_rows")) {
    driver_config.mem_row_start = aie_meta.get<uint8_t>("aie_metadata.driver_config.reserved_row_start");
    driver_config.mem_num_rows = aie_meta.get<uint8_t>("aie_metadata.driver_config.reserved_num_rows");
  }
  else {
    driver_config.mem_row_start = aie_meta.get<uint8_t>("aie_metadata.driver_config.mem_tile_row_start");
    driver_config.mem_num_rows = aie_meta.get<uint8_t>("aie_metadata.driver_config.mem_tile_num_rows");
  }
  driver_config.aie_tile_row_start = aie_meta.get<uint8_t>("aie_metadata.driver_config.aie_tile_row_start");
  driver_config.aie_tile_num_rows = aie_meta.get<uint8_t>("aie_metadata.driver_config.aie_tile_num_rows");
  return driver_config;
}

uint8_t
get_hw_gen(const pt::ptree& aie_meta)
{
  return aie_meta.get<uint8_t>("aie_metadata.driver_config.hw_gen");
}

static uint32_t
get_partition_id(const pt::ptree& aie_meta)
{
  auto num_col = aie_meta.get_child_optional("aie_metadata.driver_config.partition_num_cols");
  if (!num_col) 
    return default_id;
  auto num_col_value = num_col->get_value<uint32_t>();
  auto start_col = 0; 
  auto overlay_start_cols = aie_meta.get_child_optional("aie_metadata.driver_config.partition_overlay_start_cols");
  if (overlay_start_cols && !overlay_start_cols->empty()) 
    start_col = overlay_start_cols->begin()->second.get_value<uint8_t>();

  // AIE Driver expects the partition id format as below
  uint32_t part = (num_col_value << 8U) | (start_col & 0xffU);   
  return part;
}

adf::aiecompiler_options
get_aiecompiler_options(const pt::ptree& aie_meta)
{
  adf::aiecompiler_options aiecompiler_options;
  aiecompiler_options.broadcast_enable_core = aie_meta.get<bool>("aie_metadata.aiecompiler_options.broadcast_enable_core");
  aiecompiler_options.event_trace = aie_meta.get("aie_metadata.aiecompiler_options.event_trace", "runtime");
  return aiecompiler_options;
}

static uint8_t
get_start_col(const pt::ptree& aie_meta)
{
  auto start_col = 0;
  auto overlay_start_cols = aie_meta.get_child_optional("aie_metadata.driver_config.partition_overlay_start_cols");
  if (overlay_start_cols && !overlay_start_cols->empty())
    start_col = overlay_start_cols->begin()->second.get_value<uint8_t>();
  return start_col;
}

// get the start column of partition which gets used for broadcasting core start
// event
static uint32_t
get_partition_start_column(const pt::ptree& aie_meta, int column)
{
  try {
    auto partitions = aie_meta.get_child_optional("aie_metadata.driver_config.aie_partition_json.AIE.ai_engine_0.partitions");

    if (!partitions)
      return default_start_column; // if no patitions are available, 0 would be start column for partition

    auto itr = std::find_if(partitions.get().begin(), partitions.get().end(),[column] (const auto& part) {
			    uint32_t start_column = part.second.template get<uint32_t>("startColumn");
			    uint32_t num_columns = part.second.template get<uint32_t>("numColumns");
			    return (start_column <= column) && (column < (start_column + num_columns));
		});

    if(itr != partitions.get().end())
      return itr->second.get<uint32_t>("startColumn");
  }
  catch(...) {
    // old xclbins may not have these sections. Use default_start_column
  }
  return default_start_column;
}

adf::graph_config
get_graph(const pt::ptree& aie_meta, const std::string& graph_name)
{
  adf::graph_config graph_config;
  auto start_col = get_start_col(aie_meta);

  for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
    if (graph.second.get<std::string>("name") != graph_name)
      continue;

    graph_config.id = graph.second.get<int>("id");
    graph_config.name = graph.second.get<std::string>("name");

    int count = 0;
    for (auto& node : graph.second.get_child("core_columns")) {
      graph_config.coreColumns.push_back(std::stoul(node.second.data()) + start_col);
      count++;
    }

    if (graph_config.coreColumns.size()) // broadcasting column is same for one partition
      graph_config.broadcast_column = get_partition_start_column(aie_meta, graph_config.coreColumns[0]);
    else
      graph_config.broadcast_column = default_start_column;

    int num_tiles = count;

    count = 0;
    for (auto& node : graph.second.get_child("core_rows")) {
      graph_config.coreRows.push_back(std::stoul(node.second.data()));
      count++;
    }
    throw_if_error(count < num_tiles,"core_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_columns")) {
      graph_config.iterMemColumns.push_back(std::stoul(node.second.data()) + start_col);
      count++;
    }
    throw_if_error(count < num_tiles,"iteration_memory_columns < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_rows")) {
      graph_config.iterMemRows.push_back(std::stoul(node.second.data()));
      count++;
    }
    throw_if_error(count < num_tiles,"iteration_memory_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_addresses")) {
      graph_config.iterMemAddrs.push_back(std::stoul(node.second.data()));
      count++;
    }
    throw_if_error(count < num_tiles,"iteration_memory_addresses < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("multirate_triggers")) {
      graph_config.triggered.push_back(node.second.data() == "true");
      count++;
    }
    throw_if_error(count < num_tiles,"multirate_triggers < num_tiles");

  }

  return graph_config;
}

int
get_graph_id(const pt::ptree& aie_meta, const std::string& graph_name)
{
  for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
    if (graph.second.get<std::string>("name") != graph_name)
      continue;

    int id = graph.second.get<int>("id");
    return id;
  }

  return -1;
}

std::vector<std::string>
get_graphs(const pt::ptree& aie_meta)
{
  std::vector<std::string> graphs;

  for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
    std::string graphName = graph.second.get<std::string>("name");
    graphs.push_back(graphName);
  }

  return graphs;
}

std::vector<tile_type>
get_tiles(const pt::ptree& aie_meta, const std::string& graph_name)
{
  std::vector<tile_type> tiles;
  auto start_col = get_start_col(aie_meta); 

  for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
    if (graph.second.get<std::string>("name") != graph_name)
      continue;

    int count = 0;
    for (auto& node : graph.second.get_child("core_columns")) {
      tiles.push_back(tile_type());
      auto& t = tiles.at(count++);
      t.col = std::stoul(node.second.data()) + start_col;
    }

    int num_tiles = count;
    count = 0;
    for (auto& node : graph.second.get_child("core_rows"))
      tiles.at(count++).row = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"core_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_columns"))
      tiles.at(count++).itr_mem_col = std::stoul(node.second.data()) + start_col;
    throw_if_error(count < num_tiles,"iteration_memory_columns < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_rows"))
      tiles.at(count++).itr_mem_row = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"iteration_memory_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_addresses"))
      tiles.at(count++).itr_mem_addr = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"iteration_memory_addresses < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("multirate_triggers"))
      tiles.at(count++).is_trigger = (node.second.data() == "true");
    throw_if_error(count < num_tiles,"multirate_triggers < num_tiles");

  }

  return tiles;
}

std::vector<tile_type>
get_event_tiles(const pt::ptree& aie_meta, const std::string& graph_name,
                module_type type)
{
  // Not supported yet
  if (type == module_type::shim)
    return {};

  const char* col_name = (type == module_type::core) ? "core_columns" : "dma_columns";
  const char* row_name = (type == module_type::core) ?    "core_rows" :    "dma_rows";
  
  auto start_col = get_start_col(aie_meta); 

  std::vector<tile_type> tiles;
 
  for (auto& graph : aie_meta.get_child("aie_metadata.EventGraphs")) {
    if (graph.second.get<std::string>("name") != graph_name)
      continue;

    int count = 0;
      for (auto& node : graph.second.get_child(col_name)) {
        tiles.push_back(tile_type());
        auto& t = tiles.at(count++);
        t.col = std::stoul(node.second.data()) + start_col;
      }

      int num_tiles = count;
      count = 0;
      for (auto& node : graph.second.get_child(row_name))
        tiles.at(count++).row = std::stoul(node.second.data());
      throw_if_error(count < num_tiles,"rows < num_tiles");
  }

  return tiles;
}

std::unordered_map<std::string, adf::rtp_config>
get_rtp(const pt::ptree& aie_meta, int graph_id)
{
  std::unordered_map<std::string, adf::rtp_config> rtps;
  auto start_col = get_start_col(aie_meta); 

  for (auto& rtp_node : aie_meta.get_child("aie_metadata.RTPs")) {
    if (rtp_node.second.get<int>("graph_id") != graph_id)
      continue;

    adf::rtp_config rtp;
    rtp.portId = rtp_node.second.get<int>("port_id");
    rtp.aliasId = rtp_node.second.get<int>("alias_id");
    rtp.portName = rtp_node.second.get<std::string>("port_name");
    rtp.aliasName = rtp_node.second.get<std::string>("alias_name");
    rtp.graphId = rtp_node.second.get<int>("graph_id");
    rtp.numBytes = rtp_node.second.get<size_t>("number_of_bytes");
    rtp.selectorRow = rtp_node.second.get<short>("selector_row");
    rtp.selectorColumn = rtp_node.second.get<short>("selector_column") + start_col;
    rtp.selectorLockId = rtp_node.second.get<unsigned short>("selector_lock_id");
    rtp.selectorAddr = rtp_node.second.get<size_t>("selector_address");

    rtp.pingRow = rtp_node.second.get<short>("ping_buffer_row");
    rtp.pingColumn = rtp_node.second.get<short>("ping_buffer_column") + start_col;
    rtp.pingLockId = rtp_node.second.get<unsigned short>("ping_buffer_lock_id");
    rtp.pingAddr = rtp_node.second.get<size_t>("ping_buffer_address");

    rtp.pongRow = rtp_node.second.get<short>("pong_buffer_row");
    rtp.pongColumn = rtp_node.second.get<short>("pong_buffer_column") + start_col;
    rtp.pongLockId = rtp_node.second.get<unsigned short>("pong_buffer_lock_id");
    rtp.pongAddr = rtp_node.second.get<size_t>("pong_buffer_address");

    rtp.isPL = rtp_node.second.get<bool>("is_PL_RTP");
    rtp.isInput = rtp_node.second.get<bool>("is_input");
    rtp.isAsync = rtp_node.second.get<bool>("is_asynchronous");
    rtp.isConnect = rtp_node.second.get<bool>("is_connected");
    rtp.hasLock = rtp_node.second.get<bool>("requires_lock");

    rtps[rtp.portName] = rtp;
    rtps[rtp.aliasName] = rtp;
  }

  return rtps;
}

std::unordered_map<std::string, adf::gmio_config>
get_gmios(const pt::ptree& aie_meta)
{
  std::unordered_map<std::string, adf::gmio_config> gmios;
  auto start_col = get_start_col(aie_meta); 

  for (auto& gmio_node : aie_meta.get_child("aie_metadata.GMIOs")) {
    adf::gmio_config gmio;

    // Only get AIE GMIO type, 0: GM->AIE; 1: AIE->GM
    auto type = (adf::gmio_config::gmio_type)gmio_node.second.get<uint16_t>("type");
    if (type != adf::gmio_config::gm2aie && type != adf::gmio_config::aie2gm)
      continue;

    gmio.id = gmio_node.second.get<int>("id");
    gmio.name = gmio_node.second.get<std::string>("name");
    gmio.logicalName = gmio_node.second.get<std::string>("logical_name");
    gmio.type = type;
    gmio.shimColumn = gmio_node.second.get<short>("shim_column") + start_col;
    gmio.channelNum = gmio_node.second.get<short>("channel_number");
    gmio.streamId = gmio_node.second.get<short>("stream_id");
    gmio.burstLength = gmio_node.second.get<short>("burst_length_in_16byte");

    gmios[gmio.name] = gmio;
  }

  return gmios;
}

std::unordered_map<std::string, adf::external_buffer_config>
get_external_buffers(const pt::ptree& aie_meta)
{
  auto start_col = get_start_col(aie_meta);
  std::unordered_map<std::string, adf::external_buffer_config> external_buffer_configs;

  auto ebuf_tree = aie_meta.get_child_optional("aie_metadata.ExternalBufferConfigs");
  if (!ebuf_tree)
    return external_buffer_configs;

  for (auto& item: aie_meta.get_child("aie_metadata.ExternalBufferConfigs")) {
    adf::external_buffer_config buffer_config;
    buffer_config.id = item.second.get<int>("id");
    buffer_config.name = item.second.get<std::string>("name");

    for (const auto& port : item.second.get_child("shimPortConfigs")) {
      adf::shim_port_config port_config;
      port_config.port_id = port.second.get<int>("portId");
      port_config.port_name = port.second.get<std::string>("portName");
      std::string direction = port.second.get<std::string>("direction");
      port_config.direction = direction.compare("s2mm") ? 1 : 0;
      port_config.shim_column = port.second.get<int>("shim_column");
      port_config.channel_number = port.second.get<int>("channel_number");
      port_config.task_repetition = port.second.get<int>("task_repetition");
      port_config.enable_task_complete_token = port.second.get<int>("enable_task_complete_token");

      for (const auto& bd : port.second.get_child("shimBDInfos")) {
        adf::shim_bd_info bd_info;
        bd_info.bd_id = bd.second.get<int>("bd_id");
        bd_info.buf_idx = bd.second.get<int>("buf_idx");
        bd_info.offset = bd.second.get<int>("offset");
        bd_info.transaction_size = bd.second.get<int>("transaction_size");
        port_config.shim_bd_infos.push_back(bd_info);
      }
      buffer_config.shim_port_configs.push_back(port_config);
    }
    external_buffer_configs[buffer_config.name] = buffer_config;
  }

  /* Print the parsed data to verify
  for (const auto& config : external_buffer_configs) {
    config.second.print();
  }*/

  return external_buffer_configs;
}

std::unordered_map<std::string, adf::plio_config>
get_plios(const pt::ptree& aie_meta)
{
  std::unordered_map<std::string, adf::plio_config> plios;
  auto start_col = get_start_col(aie_meta);

  for (auto& plio_node : aie_meta.get_child("aie_metadata.PLIOs")) {
    adf::plio_config plio;

    plio.id = plio_node.second.get<uint32_t>("id");
    plio.name = plio_node.second.get<std::string>("name");
    plio.logicalName = plio_node.second.get<std::string>("logical_name");
    plio.shimColumn = plio_node.second.get<uint16_t>("shim_column") + start_col;
    plio.streamId = plio_node.second.get<uint16_t>("stream_id");
    plio.slaveOrMaster = plio_node.second.get<bool>("slaveOrMaster");

    plios[plio.name] = plio;
  }

  return plios;
}

double
get_clock_freq_mhz(const pt::ptree& aie_meta)
{
  auto dev_node = aie_meta.get_child("aie_metadata.DeviceData");
  double clockFreqMhz = dev_node.get<double>("AIEFrequency");
  return clockFreqMhz;
}

std::vector<counter_type>
get_profile_counter(const pt::ptree& aie_meta)
{
  // If counters not found, then return empty vector
  auto counterTree = aie_meta.get_child_optional("aie_metadata.PerformanceCounter");
  if (!counterTree)
    return {};

  // First grab clock frequency
  auto clockFreqMhz = get_clock_freq_mhz(aie_meta);
  auto start_col = get_start_col(aie_meta); 

  std::vector<counter_type> counters;

  // Now parse all counters
  for (auto const &counter_node : counterTree.get()) {
    counter_type counter;

    counter.id = counter_node.second.get<uint32_t>("id");
    counter.column = counter_node.second.get<uint16_t>("core_column") + start_col;
    counter.row = counter_node.second.get<uint16_t>("core_row");
    counter.counterNumber = counter_node.second.get<uint8_t>("counterId");
    counter.startEvent = counter_node.second.get<uint8_t>("start");
    counter.endEvent = counter_node.second.get<uint8_t>("stop");
    //counter.resetEvent = counter_node.second.get<uint8_t>("reset");
    // Assume common clock frequency for all AIE tiles
    counter.clockFreqMhz = clockFreqMhz;
    counter.module = counter_node.second.get<std::string>("module");
    counter.name = counter_node.second.get<std::string>("name");

    counters.emplace_back(std::move(counter));
  }

  return counters;
}

std::vector<gmio_type>
get_trace_gmio(const pt::ptree& aie_meta)
{
  auto trace_gmios = aie_meta.get_child_optional("aie_metadata.TraceGMIOs");
  if (!trace_gmios)
    return {};

  auto start_col = get_start_col(aie_meta); 
  std::vector<gmio_type> gmios;

  for (auto& gmio_node : trace_gmios.get()) {
    gmio_type gmio;

    gmio.id = gmio_node.second.get<uint32_t>("id");
    //gmio.name = gmio_node.second.get<std::string>("name");
    //gmio.type = gmio_node.second.get<uint16_t>("type");
    gmio.shimColumn = gmio_node.second.get<uint16_t>("shim_column") + start_col;
    gmio.channelNum = gmio_node.second.get<uint16_t>("channel_number");
    gmio.streamId = gmio_node.second.get<uint16_t>("stream_id");
    gmio.burstLength = gmio_node.second.get<uint16_t>("burst_length_in_16byte");

    gmios.emplace_back(std::move(gmio));
  }

  return gmios;
}

} // namespace

namespace xrt_core { namespace edge { namespace aie {

adf::driver_config
get_driver_config(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_driver_config(aie_meta);
}

adf::aiecompiler_options
get_aiecompiler_options(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_aiecompiler_options(aie_meta);
}

adf::graph_config
get_graph(const xrt_core::device* device, const std::string& graph_name, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_graph(aie_meta, graph_name);
}

int
get_graph_id(const xrt_core::device* device, const std::string& graph_name, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return -1;

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_graph_id(aie_meta, graph_name);
}

std::vector<std::string>
get_graphs(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_graphs(aie_meta);
}

std::vector<tile_type>
get_tiles(const xrt_core::device* device, const std::string& graph_name, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_tiles(aie_meta, graph_name);
}

std::vector<tile_type>
get_event_tiles(const xrt_core::device* device, const std::string& graph_name,
                    module_type type, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_event_tiles(aie_meta, graph_name, type);
}

std::unordered_map<std::string, adf::rtp_config>
get_rtp(const xrt_core::device* device, int graph_id, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_rtp(aie_meta, graph_id);
}

std::unordered_map<std::string, adf::gmio_config>
get_gmios(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_gmios(aie_meta);
}

std::unordered_map<std::string, adf::external_buffer_config>
get_external_buffers(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_external_buffers(aie_meta);
}

std::unordered_map<std::string, adf::plio_config>
get_plios(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_plios(aie_meta);
}

double
get_clock_freq_mhz(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return 1000.0;  // magic

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_clock_freq_mhz(aie_meta);
}

std::vector<counter_type>
get_profile_counters(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_profile_counter(aie_meta);
}

std::vector<gmio_type>
get_trace_gmios(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return {};

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_trace_gmio(aie_meta);
}
/* hw_gen represents aie version 1.aie, 2.aie-ml etc */
uint8_t
get_hw_gen(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return 1; // default is aie-1

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_hw_gen(aie_meta);
}

uint32_t
get_partition_id(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx)
{
  auto xclbin_uuid = hwctx ? hwctx->get_xclbin_uuid() : uuid();
  auto data = device->get_axlf_section(AIE_METADATA, xclbin_uuid);
  if (!data.first || !data.second)
    return 1; 

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_partition_id(aie_meta);
}

}}} // aie, edge, xrt_core

