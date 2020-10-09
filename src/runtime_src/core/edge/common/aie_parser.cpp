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
using tile_type = xrt_core::edge::aie::tile_type;
using rtp_type = xrt_core::edge::aie::rtp_type;
using gmio_type = xrt_core::edge::aie::gmio_type;
using plio_type = xrt_core::edge::aie::plio_type;
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

std::vector<tile_type>
get_tiles(const pt::ptree& aie_meta, const std::string& graph_name)
{
  std::vector<tile_type> tiles;

  for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
    if (graph.second.get<std::string>("name") != graph_name)
      continue;

    int count = 0;
    for (auto& node : graph.second.get_child("core_columns")) {
      tiles.push_back(tile_type());
      auto& t = tiles.at(count++);
      t.col = std::stoul(node.second.data());
    }

    int num_tiles = count;
    count = 0;
    for (auto& node : graph.second.get_child("core_rows"))
      tiles.at(count++).row = std::stoul(node.second.data());
    throw_if_error(count < num_tiles,"core_rows < num_tiles");

    count = 0;
    for (auto& node : graph.second.get_child("iteration_memory_columns"))
      tiles.at(count++).itr_mem_col = std::stoul(node.second.data());
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

std::vector<rtp_type>
get_rtp(const pt::ptree& aie_meta)
{
  std::vector<rtp_type> rtps;

  for (auto& rtp_node : aie_meta.get_child("aie_metadata.RTPs")) {
    rtp_type rtp;

    rtp.name = rtp_node.second.get<std::string>("port_name");
    rtp.selector_row = rtp_node.second.get<uint16_t>("selector_row");
    rtp.selector_col = rtp_node.second.get<uint16_t>("selector_column");
    rtp.selector_lock_id = rtp_node.second.get<uint16_t>("selector_lock_id");
    rtp.selector_addr = rtp_node.second.get<uint64_t>("selector_address");

    rtp.ping_row = rtp_node.second.get<uint16_t>("ping_buffer_row");
    rtp.ping_col = rtp_node.second.get<uint16_t>("ping_buffer_column");
    rtp.ping_lock_id = rtp_node.second.get<uint16_t>("ping_buffer_lock_id");
    rtp.ping_addr = rtp_node.second.get<uint64_t>("ping_buffer_address");

    rtp.pong_row = rtp_node.second.get<uint16_t>("pong_buffer_row");
    rtp.pong_col = rtp_node.second.get<uint16_t>("pong_buffer_column");
    rtp.pong_lock_id = rtp_node.second.get<uint16_t>("pong_buffer_lock_id");
    rtp.pong_addr = rtp_node.second.get<uint64_t>("pong_buffer_address");

    rtp.is_plrtp = rtp_node.second.get<bool>("is_PL_RTP");
    rtp.is_input = rtp_node.second.get<bool>("is_input");
    rtp.is_async = rtp_node.second.get<bool>("is_asynchronous");
    rtp.is_connected = rtp_node.second.get<bool>("is_connected");
    rtp.require_lock = rtp_node.second.get<bool>("requires_lock");

    rtps.emplace_back(std::move(rtp));
  }

  return rtps;
}

std::vector<gmio_type>
get_gmio(const pt::ptree& aie_meta)
{
  std::vector<gmio_type> gmios;

  for (auto& gmio_node : aie_meta.get_child("aie_metadata.GMIOs")) {
    gmio_type gmio;

    // Only get AIE GMIO type, 0: GM->AIE; 1: AIE->GM
    auto type = gmio_node.second.get<uint16_t>("type");
    if (type != 0 && type != 1)
      continue;

    gmio.id = gmio_node.second.get<uint32_t>("id");
    gmio.name = gmio_node.second.get<std::string>("name");
    gmio.type = type;
    gmio.shim_col = gmio_node.second.get<uint16_t>("shim_column");
    gmio.channel_number = gmio_node.second.get<uint16_t>("channel_number");
    gmio.stream_id = gmio_node.second.get<uint16_t>("stream_id");
    gmio.burst_len = gmio_node.second.get<uint16_t>("burst_length_in_16byte");

    gmios.emplace_back(std::move(gmio));
  }

  return gmios;
}

std::vector<plio_type>
get_plio(const pt::ptree& aie_meta)
{
  auto plio_nodes = aie_meta.get_child_optional("aie_metadata.PLIOs");
  if (!plio_nodes)
    return {};

  std::vector<plio_type> plios;

  for (auto& plio_node : aie_meta.get_child("aie_metadata.PLIOs")) {
    plio_type plio;

    plio.id = plio_node.second.get<uint32_t>("id");
    plio.name = plio_node.second.get<std::string>("name");
    plio.logical_name = plio_node.second.get<std::string>("logical_name");
    plio.shim_col = plio_node.second.get<uint16_t>("shim_column");
    plio.stream_id = plio_node.second.get<uint16_t>("stream_id");
    plio.is_master = plio_node.second.get<bool>("slaveOrMaster");

    plios.emplace_back(std::move(plio));
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

std::vector<tile_type>
get_tiles(const xrt_core::device* device, const std::string& graph_name)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::vector<tile_type>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_tiles(aie_meta, graph_name);
}

std::vector<rtp_type>
get_rtp(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::vector<rtp_type>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_rtp(aie_meta);
}

std::vector<gmio_type>
get_gmios(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::vector<gmio_type>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_gmio(aie_meta);
}

std::vector<plio_type>
get_plios(const xrt_core::device* device)
{
  auto data = device->get_axlf_section(AIE_METADATA);
  if (!data.first || !data.second)
    return std::vector<plio_type>();

  pt::ptree aie_meta;
  read_aie_metadata(data.first, data.second, aie_meta);
  return ::get_plio(aie_meta);
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

