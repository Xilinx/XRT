/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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


#include <cstdint>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "core/common/message.h"
#include "xdp/profile/device/tracedefs.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <iostream>
#include <memory>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
//#include "core/edge/user/shim.h"
#include "core/edge/common/aie_parser.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"
#include "xdp/profile/writer/aie_trace/aie_trace_config_writer.h"

#include "aie_trace_metadata.h"
#include "aie_trace_plugin.h"

namespace xdp {
  namespace pt = boost::property_tree;
  using severity_level = xrt_core::message::severity_level;
  constexpr double AIE_DEFAULT_FREQ_MHZ = 1000.0;
  
  AieTraceMetadata::AieTraceMetadata(uint64_t deviceID, void* handle)
  : deviceID(deviceID)
  , handle(handle)
  {
    // Check whether continuous trace is enabled in xrt.ini
    // AIE trace is now supported for HW only
    continuousTrace = xrt_core::config::get_aie_trace_periodic_offload();
    if (continuousTrace) {
      auto offloadIntervalms = xrt_core::config::get_aie_trace_buffer_offload_interval_ms();
      if (offloadIntervalms != 10) {
        std::stringstream msg;
        msg << "aie_trace_buffer_offload_interval_ms will be deprecated in future. "
            << "Please use \"buffer_offload_interval_us\" under \"AIE_trace_settings\" section.";
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
        offloadIntervalUs = offloadIntervalms * uint_constants::one_thousand;
      } else {
        offloadIntervalUs = xrt_core::config::get_aie_trace_settings_buffer_offload_interval_us();
      }
    }

    // Set Delay parameters
    // Update delay with clock cycles when we get device handle later
    mDelayCycles = static_cast<uint32_t>(getTraceStartDelayCycles());
    mUseDelay = (mDelayCycles > 0) ? true : false;

    // Pre-defined metric sets
    metricSets = {"functions", "functions_partial_stalls", "functions_all_stalls", "all"};

    // **** Core Module Counters ****
    // NOTE 1: Reset events are dependent on actual profile counter reserved.
    // NOTE 2: These counters are required HW workarounds with thresholds chosen 
    //         to produce events before hitting the bug. For example, sync packets 
    //         occur after 1024 cycles and with no events, is incorrectly repeated.
    auto counterScheme = xrt_core::config::get_aie_trace_counter_scheme();

    //Process the file dump interval
    aie_trace_file_dump_int_s = xrt_core::config::get_aie_trace_settings_file_dump_interval_s();
    if (aie_trace_file_dump_int_s == DEFAULT_AIE_TRACE_DUMP_INTERVAL_S) {
      // if set to default value, then check for old style config
      aie_trace_file_dump_int_s = xrt_core::config::get_aie_trace_file_dump_interval_s();
      if (aie_trace_file_dump_int_s != DEFAULT_AIE_TRACE_DUMP_INTERVAL_S)
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
          "The xrt.ini flag \"aie_trace_file_dump_interval_s\" is deprecated and will be removed in future release. Please use \"file_dump_interval_s\" under \"AIE_trace_settings\" section.");
    }
    if (aie_trace_file_dump_int_s < MIN_TRACE_DUMP_INTERVAL_S) {
      aie_trace_file_dump_int_s = MIN_TRACE_DUMP_INTERVAL_S;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", AIE_TRACE_DUMP_INTERVAL_WARN_MSG);
    }

    // Catch when compile-time trace is specified (e.g., --event-trace=functions)
    auto device = xrt_core::get_userpf_device(handle);
    auto compilerOptions = get_aiecompiler_options(device.get());
    runtimeMetrics = (compilerOptions.event_trace == "runtime");
  }

  std::string AieTraceMetadata::getMetricSet()
  {
    // Catch when compile-time trace is specified (e.g., --event-trace=functions)
    if (!getRuntimeMetrics()) {
      auto device = xrt_core::get_userpf_device(handle);
      auto compilerOptions = get_aiecompiler_options(device.get());
      std::stringstream msg;
      msg << "Found compiler trace option of " << compilerOptions.event_trace
          << ". No runtime AIE metrics will be changed.";
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return {};
    }

    std::string metricsStr = xrt_core::config::get_aie_trace_metrics();
    if (metricsStr.empty()) {
      std::string msg("The setting aie_trace_metrics was not specified in xrt.ini. AIE event trace will not be available.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return {};
    }

    std::vector<std::string> vec;
    boost::split(vec, metricsStr, boost::is_any_of(":"));

    for (unsigned int i=0; i < vec.size(); ++i) {
      boost::replace_all(vec.at(i), "{", "");
      boost::replace_all(vec.at(i), "}", "");
    }

    // Determine specification type based on vector size:
    //   * Size = 1: All tiles
    //     * aie_trace_metrics = <functions|functions_partial_stalls|functions_all_stalls|all>
    //   * Size = 2: Single tile or kernel name (supported in future release)
    //     * aie_trace_metrics = {<column>,<row>}:<functions|functions_partial_stalls|functions_all_stalls|all>
    //     * aie_trace_metrics= <kernel name>:<functions|functions_partial_stalls|functions_all_stalls|all>
    //   * Size = 3: Range of tiles (supported in future release)
    //     * aie_trace_metrics= {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<functions|functions_partial_stalls|functions_all_stalls|all>
    metricSet = vec.at( vec.size()-1 );

    if (metricSets.find(metricSet) == metricSets.end()) {
      std::string defaultSet = "functions";
      std::stringstream msg;
      msg << "Unable to find AIE trace metric set " << metricSet 
          << ". Using default of " << defaultSet << ".";
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      metricSet = defaultSet;
    }
    return metricSet;
  }

  std::vector<tile_type> AieTraceMetadata::getTilesForTracing()
  {
    std::vector<tile_type> tiles;
    // Create superset of all tiles across all graphs
    // NOTE: future releases will support the specification of tile subsets
    auto device = xrt_core::get_userpf_device(handle);
    auto graphs = get_graphs(device.get());
    for (auto& graph : graphs) {
      auto currTiles = get_tiles(device.get(), graph);
      std::copy(currTiles.begin(), currTiles.end(), back_inserter(tiles));

      // TODO: Differentiate between core and DMA-only tiles when 'all' is supported

      // Core Tiles
      //auto coreTiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph, module_type::core);
      //std::unique_copy(coreTiles.begin(), coreTiles.end(), std::back_inserter(tiles), tileCompare);

      // DMA-Only Tiles
      // NOTE: These tiles are only needed when aie_trace_metrics = all
      //auto dmaTiles = xrt_core::edge::aie::get_event_tiles(device.get(), graph, module_type::dma);
      //std::unique_copy(dmaTiles.begin(), dmaTiles.end(), std::back_inserter(tiles), tileCompare);
    }
    return tiles;
  }

  uint64_t AieTraceMetadata::getTraceStartDelayCycles()
  {
    double freqMhz = AIE_DEFAULT_FREQ_MHZ;

    if (handle != nullptr) {
      auto device = xrt_core::get_userpf_device(handle);
      freqMhz = get_clock_freq_mhz(device.get());
    }

    auto cycles_per_sec = static_cast<uint64_t>(freqMhz * uint_constants::one_million);
    const uint64_t max_cycles = 0xffffffff;
    std::string size_str = xrt_core::config::get_aie_trace_start_time();

    // Catch cases like "1Ms" "1NS"
    std::transform(size_str.begin(), size_str.end(), size_str.begin(),
      [](unsigned char c){ return std::tolower(c); });

    // Default is 0 cycles
    uint64_t cycles = 0;
    // Regex can parse values like : "1s" "1ms" "1ns"
    const std::regex size_regex("\\s*(\\d+\\.?\\d*)\\s*(s|ms|us|ns|)\\s*");
    std::smatch pieces_match;
    if (std::regex_match(size_str, pieces_match, size_regex)) {
      try {
        cycles = static_cast<uint64_t>(std::stof(pieces_match[1]));
      } catch (const std::exception& ) {
        // User specified number cannot be parsed
        std::string msg("Unable to parse aie_trace_start_time. Setting start time to 0.");
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      }
      if (pieces_match[2] == "s") {
        cycles *=  cycles_per_sec;
      } else if (pieces_match[2] == "ms") {
        cycles *= cycles_per_sec /  uint_constants::one_thousand;
      } else if (pieces_match[2] == "us") {
        cycles *= cycles_per_sec /  uint_constants::one_million;
      } else if (pieces_match[2] == "ns") {
        cycles *= cycles_per_sec /  uint_constants::one_billion;
      }
      std::string msg("Parsed aie_trace_start_time: " + std::to_string(cycles) + " cycles.");
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
    } else {
      std::string msg("Unable to parse aie_trace_start_time. Setting start time to 0.");
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
    }

    if (cycles > max_cycles) {
      cycles = max_cycles;
      std::string msg("Setting aie_trace_delay to max supported of 0xffffffff cycles.");
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
    }

    return cycles;
  }

  void AieTraceMetadata::read_aie_metadata(const char* data, size_t size, pt::ptree& aie_project)
  {
    std::stringstream aie_stream;
    aie_stream.write(data,size);
    pt::read_json(aie_stream,aie_project);
  }


  //locally defined xrt::core::edge functions 

  adf::aiecompiler_options AieTraceMetadata::get_aiecompiler_options(const xrt_core::device* device)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    adf::aiecompiler_options aiecompiler_options;
    aiecompiler_options.broadcast_enable_core = aie_meta.get<bool>("aie_metadata.aiecompiler_options.broadcast_enable_core");
    aiecompiler_options.event_trace = aie_meta.get("aie_metadata.aiecompiler_options.event_trace", "runtime");
    return aiecompiler_options;
  }

  std::vector<std::string> AieTraceMetadata::get_graphs(const xrt_core::device* device)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    std::vector<std::string> graphs;

    for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
      std::string graphName = graph.second.get<std::string>("name");
      graphs.push_back(graphName);
    }

    return graphs;
  }

  inline void throw_if_error(bool err, const char* msg)
  {
    if (err)
      throw std::runtime_error(msg);
  }

  std::vector<tile_type> AieTraceMetadata::get_tiles(const xrt_core::device* device, const std::string& graph_name)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    
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

  double AieTraceMetadata::get_clock_freq_mhz(const xrt_core::device* device)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return AIE_DEFAULT_FREQ_MHZ;

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    auto dev_node = aie_meta.get_child("aie_metadata.DeviceData");
    return dev_node.get<double>("AIEFrequency");
  }

  std::vector<gmio_type> AieTraceMetadata::get_trace_gmios(const xrt_core::device* device)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    auto trace_gmios = aie_meta.get_child_optional("aie_metadata.TraceGMIOs");
    if (!trace_gmios)
      return {};

    std::vector<gmio_type> gmios;

    for (auto& gmio_node : trace_gmios.get()) {
      gmio_type gmio;

      gmio.id = gmio_node.second.get<uint32_t>("id");
      //gmio.name = gmio_node.second.get<std::string>("name");
      //gmio.type = gmio_node.second.get<uint16_t>("type");
      gmio.shimColumn = gmio_node.second.get<uint16_t>("shim_column");
      gmio.channelNum = gmio_node.second.get<uint16_t>("channel_number");
      gmio.streamId = gmio_node.second.get<uint16_t>("stream_id");
      gmio.burstLength = gmio_node.second.get<uint16_t>("burst_length_in_16byte");

      gmios.emplace_back(std::move(gmio));
    }

    return gmios;
  }
  
}
