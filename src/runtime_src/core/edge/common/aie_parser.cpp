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

namespace {

namespace pt = boost::property_tree;
using gmio_type = xrt_core::edge::aie::gmio_type;
using counter_type = xrt_core::edge::aie::counter_type;

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
  driver_config.reserved_row_start = aie_meta.get<uint8_t>("aie_metadata.driver_config.reserved_row_start");
  driver_config.reserved_num_rows = aie_meta.get<uint8_t>("aie_metadata.driver_config.reserved_num_rows");
  driver_config.aie_tile_row_start = aie_meta.get<uint8_t>("aie_metadata.driver_config.aie_tile_row_start");
  driver_config.aie_tile_num_rows = aie_meta.get<uint8_t>("aie_metadata.driver_config.aie_tile_num_rows");
  return driver_config;
}

adf::aiecompiler_options
get_aiecompiler_options(const pt::ptree& aie_meta)
{
    adf::aiecompiler_options aiecompiler_options;
    aiecompiler_options.broadcast_enable_core = aie_meta.get<bool>("aie_metadata.aiecompiler_options.broadcast_enable_core");
    return aiecompiler_options;
}
  
adf::graph_config
get_graph(const pt::ptree& aie_meta, const std::string& graph_name)
{
  adf::graph_config graph_config;

  for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
    if (graph.second.get<std::string>("name") != graph_name)
      continue;

    graph_config.id = graph.second.get<int>("id");
    graph_config.name = graph.second.get<std::string>("name");

    int count = 0;
    for (auto& node : graph.second.get_child("core_columns")) {
      graph_config.coreColumns.push_back(std::stoul(node.second.data()));
      count++;
    }

    int num_tiles = count;

    count = 0;
    for (auto& node : graph.second.get_child("core_rows")) {
      graph_config.coreRows.push_back(std::stoul(node.second.data()));
      count++;
    }
    throw_if_error(count < num_tiles,"core_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_columns")) {
      graph_config.iterMemColumns.push_back(std::stoul(node.second.data()));
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

std::unordered_map<std::string, adf::rtp_config>
get_rtp(const pt::ptree& aie_meta, int graph_id)
{
  std::unordered_map<std::string, adf::rtp_config> rtps;

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
    rtp.selectorColumn = rtp_node.second.get<short>("selector_column");
    rtp.selectorLockId = rtp_node.second.get<unsigned short>("selector_lock_id");
    rtp.selectorAddr = rtp_node.second.get<size_t>("selector_address");

    rtp.pingRow = rtp_node.second.get<short>("ping_buffer_row");
    rtp.pingColumn = rtp_node.second.get<short>("ping_buffer_column");
    rtp.pingLockId = rtp_node.second.get<unsigned short>("ping_buffer_lock_id");
    rtp.pingAddr = rtp_node.second.get<size_t>("ping_buffer_address");

    rtp.pongRow = rtp_node.second.get<short>("pong_buffer_row");
    rtp.pongColumn = rtp_node.second.get<short>("pong_buffer_column");
    rtp.pongLockId = rtp_node.second.get<unsigned short>("pong_buffer_lock_id");
    rtp.pongAddr = rtp_node.second.get<size_t>("pong_buffer_address");

    rtp.isPL = rtp_node.second.get<bool>("is_PL_RTP");
    rtp.isInput = rtp_node.second.get<bool>("is_input");
    rtp.isAsync = rtp_node.second.get<bool>("is_asynchronous");
    rtp.isConnect = rtp_node.second.get<bool>("is_connected");
    rtp.hasLock = rtp_node.second.get<bool>("requires_lock");

    rtps[rtp.portName] = rtp;
  }

  return rtps;
}

std::unordered_map<std::string, adf::gmio_config>
get_gmios(const pt::ptree& aie_meta)
{
  std::unordered_map<std::string, adf::gmio_config> gmios;

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
    gmio.shimColumn = gmio_node.second.get<short>("shim_column");
    gmio.channelNum = gmio_node.second.get<short>("channel_number");
    gmio.streamId = gmio_node.second.get<short>("stream_id");
    gmio.burstLength = gmio_node.second.get<short>("burst_length_in_16byte");

    gmios[gmio.name] = gmio;
  }

  return gmios;
}

std::unordered_map<std::string, adf::plio_config>
get_plios(const pt::ptree& aie_meta)
{
  std::unordered_map<std::string, adf::plio_config> plios;

  for (auto& plio_node : aie_meta.get_child("aie_metadata.PLIOs")) {
    adf::plio_config plio;

    plio.id = plio_node.second.get<uint32_t>("id");
    plio.name = plio_node.second.get<std::string>("name");
    plio.logicalName = plio_node.second.get<std::string>("logical_name");
    plio.shimColumn = plio_node.second.get<uint16_t>("shim_column");
    plio.streamId = plio_node.second.get<uint16_t>("stream_id");
    plio.slaveOrMaster = plio_node.second.get<bool>("slaveOrMaster");

    plios[plio.name] = plio;
  }

  return plios;
}

std::vector<counter_type>
get_profile_counter(const pt::ptree& aie_meta)
{
  std::vector<counter_type> counters;

  // If counters not found, then return empty vector
  auto counterTree = aie_meta.get_child_optional("aie_metadata.PerformanceCounter");
  if (!counterTree)
    return counters;

  // First grab clock frequency
  auto dev_node = aie_meta.get_child("aie_metadata.DeviceData");
  auto clockFreqMhz = dev_node.get<double>("AIEFrequency");

  // Now parse all counters
  for (auto const &counter_node : counterTree.get()) {
    counter_type counter;

    counter.id = counter_node.second.get<uint32_t>("id");
    counter.column = counter_node.second.get<uint16_t>("core_column");
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
  if(!trace_gmios) {
    return {};
  }

  std::vector<gmio_type> gmios;

  for (auto& gmio_node : trace_gmios.get()) {
    gmio_type gmio;

    gmio.id = gmio_node.second.get<uint32_t>("id");
    //gmio.name = gmio_node.second.get<std::string>("name");
    //gmio.type = gmio_node.second.get<uint16_t>("type");
    gmio.shim_col = gmio_node.second.get<uint16_t>("shim_column");
    gmio.channel_number = gmio_node.second.get<uint16_t>("channel_number");
    gmio.stream_id = gmio_node.second.get<uint16_t>("stream_id");
    gmio.burst_len = gmio_node.second.get<uint16_t>("burst_length_in_16byte");

    gmios.emplace_back(std::move(gmio));
  }

  return gmios;
}

} // namespace

namespace xrt_core { namespace edge { namespace aie {

adf::driver_config
get_driver_config(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return adf::driver_config();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_driver_config(aie_meta);
}

adf::aiecompiler_options
get_aiecompiler_options(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return adf::aiecompiler_options();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_aiecompiler_options(aie_meta);
}

adf::graph_config
get_graph(const xrt_core::device* device, const std::string& graph_name)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return adf::graph_config();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_graph(aie_meta, graph_name);
}

int
get_graph_id(const xrt_core::device* device, const std::string& graph_name)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return -1;

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_graph_id(aie_meta, graph_name);
}

std::unordered_map<std::string, adf::rtp_config>
get_rtp(const xrt_core::device* device, int graph_id)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::unordered_map<std::string, adf::rtp_config>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_rtp(aie_meta, graph_id);
}

std::unordered_map<std::string, adf::gmio_config>
get_gmios(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::unordered_map<std::string, adf::gmio_config>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_gmios(aie_meta);
}

std::unordered_map<std::string, adf::plio_config>
get_plios(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::unordered_map<std::string, adf::plio_config>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_plios(aie_meta);
}

std::vector<counter_type>
get_profile_counters(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::vector<counter_type>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_profile_counter(aie_meta);
}

std::vector<gmio_type>
get_trace_gmios(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::vector<gmio_type>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_trace_gmio(aie_meta);
}

}}} // aie, edge, xrt_core

