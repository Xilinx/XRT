/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_SOURCE

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

#include <cstdint>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <regex>

#include "core/common/device.h"
#include "core/common/xrt_profiling.h"
#include "aie_trace_metadata.h"
#include "core/common/message.h"
#include "core/edge/common/aie_parser.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/device/tracedefs.h"

namespace xdp {
  namespace pt = boost::property_tree;
  using severity_level = xrt_core::message::severity_level;
  constexpr double AIE_DEFAULT_FREQ_MHZ = 1000.0;
  
  AieTraceMetadata::AieTraceMetadata(uint64_t deviceID, void* handle)
  : deviceID(deviceID)
  , handle(handle)
  {

    counterScheme = xrt_core::config::get_aie_trace_settings_counter_scheme();
    
    // Check whether continuous trace is enabled in xrt.ini
    // AIE trace is now supported for HW only
    continuousTrace = xrt_core::config::get_aie_trace_settings_periodic_offload();
    if (continuousTrace)
      offloadIntervalUs = xrt_core::config::get_aie_trace_settings_buffer_offload_interval_us();

    // Pre-defined metric sets
    metricSets = {"functions", "functions_partial_stalls", "functions_all_stalls", "all"};
    memTileMetricSets = {"input_channels", "input_channels_stalls", "output_channels", "output_channels_stalls"};

    //Process the file dump interval
    aie_trace_file_dump_int_s = xrt_core::config::get_aie_trace_settings_file_dump_interval_s();
    
    if (aie_trace_file_dump_int_s < MIN_TRACE_DUMP_INTERVAL_S) {
      aie_trace_file_dump_int_s = MIN_TRACE_DUMP_INTERVAL_S;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", AIE_TRACE_DUMP_INTERVAL_WARN_MSG);
    }

    // Catch when compile-time trace is specified (e.g., --event-trace=functions)
    auto device = xrt_core::get_userpf_device(handle);
    // auto compilerOptions = get_aiecompiler_options(device.get());
    // runtimeMetrics = (compilerOptions.event_trace == "runtime");
    runtimeMetrics = true;
    
    // Process AIE_trace_settings metrics
    auto aieTileMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_tile_based_aie_tile_metrics());
    auto aieGraphMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_graph_based_aie_tile_metrics());
    auto memTileMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_tile_based_mem_tile_metrics());
    auto memGraphMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_graph_based_mem_tile_metrics());


    if (aieTileMetricsSettings.empty() && aieGraphMetricsSettings.empty()
        && memTileMetricsSettings.empty() && memGraphMetricsSettings.empty()) {
        isValidMetrics = false;
    } else {

      getConfigMetricsForTiles(aieTileMetricsSettings, aieGraphMetricsSettings, module_type::core);
      getConfigMetricsForTiles(memTileMetricsSettings, memGraphMetricsSettings, module_type::mem_tile);
      setTraceStartControl();
    }
  }

  bool tileCompare(tile_type tile1, tile_type tile2) 
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }

  int AieTraceMetadata::getHardwareGen()
  {
    static int hwGen = 1;
    static bool gotValue = false;
    if (!gotValue) {
      auto device = xrt_core::get_userpf_device(handle);
      auto data = device->get_axlf_section(AIE_METADATA);
      if (!data.first || !data.second) {
        hwGen = 1;
      } else {
        pt::ptree aie_meta;
        read_aie_metadata(data.first, data.second, aie_meta);
        hwGen = aie_meta.get_child("aie_metadata.driver_config.hw_gen").get_value<int>();
      }
      gotValue = true;
    }
    return hwGen;
  }

  uint16_t AieTraceMetadata::getAIETileRowOffset()
  {
    static uint16_t rowOffset = 1;
    static bool gotValue = false;
    if (!gotValue) {
      auto device = xrt_core::get_userpf_device(handle);
      auto data = device->get_axlf_section(AIE_METADATA);
      if (!data.first || !data.second) {
        rowOffset = 1;
      } else {
        pt::ptree aie_meta;
        read_aie_metadata(data.first, data.second, aie_meta);
        rowOffset = aie_meta.get_child("aie_metadata.driver_config.aie_tile_row_start").get_value<uint16_t>();
      }
      gotValue = true;
    }
    return rowOffset;
  }

  std::vector<std::string>
  AieTraceMetadata::getSettingsVector(std::string settingsString) 
  {
    if (settingsString.empty())
      return {};

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;
    boost::replace_all(settingsString, " ", "");
    boost::split(settingsVector, settingsString, boost::is_any_of(";"));
    return settingsVector;
  }

  std::vector<tile_type> AieTraceMetadata::getMemTilesForTracing()
  {
    if (getHardwareGen() == 1) 
      return {};

    auto device = xrt_core::get_userpf_device(handle);
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);

    // If counters not found, then return empty vector
    auto sharedBufferTree = aie_meta.get_child_optional("aie_metadata.TileMapping.SharedBufferToTileMapping");
    if (!sharedBufferTree)
      return {};

    std::vector<tile_type> allTiles;
    std::vector<tile_type> memTiles;

    // Now parse all shared buffers
    for (auto const &shared_buffer : sharedBufferTree.get()) {
      tile_type tile;
      tile.col = shared_buffer.second.get<uint16_t>("column");
      tile.row = shared_buffer.second.get<uint16_t>("row");
      allTiles.emplace_back(std::move(tile));
    }

    std::unique_copy(allTiles.begin(), allTiles.end(), std::back_inserter(memTiles), tileCompare);
    return memTiles;
  }

  void AieTraceMetadata::setTraceStartControl()
  {
    useDelay = false;
    useGraphIterator = false;
    useUserControl = false;

    auto startType = xrt_core::config::get_aie_trace_settings_start_type();

    if (startType == "time") {
      // Use number of cycles to start trace
      VPDatabase* db = VPDatabase::Instance();
      double freqMhz = (db->getStaticInfo()).getClockRateMHz(deviceID,false);

      std::smatch pieces_match;
      uint64_t cycles_per_sec = static_cast<uint64_t>(freqMhz * uint_constants::one_million);

      std::string start_str = xrt_core::config::get_aie_trace_settings_start_time();

      // Catch cases like "1Ms" "1NS"
      std::transform(start_str.begin(), start_str.end(), start_str.begin(),
        [](unsigned char c){ return std::tolower(c); });

      // Default is 0 cycles
      uint64_t cycles = 0;
      // Regex can parse values like : "1s" "1ms" "1ns"
      const std::regex size_regex("\\s*(\\d+\\.?\\d*)\\s*(s|ms|us|ns|)\\s*");
      if (std::regex_match(start_str, pieces_match, size_regex)) {
        try {
          if (pieces_match[2] == "s") {
            cycles = static_cast<uint64_t>(std::stof(pieces_match[1]) * cycles_per_sec);
          } else if (pieces_match[2] == "ms") {
            cycles = static_cast<uint64_t>(std::stof(pieces_match[1]) * cycles_per_sec /  uint_constants::one_thousand);
          } else if (pieces_match[2] == "us") {
            cycles = static_cast<uint64_t>(std::stof(pieces_match[1]) * cycles_per_sec /  uint_constants::one_million);
          } else if (pieces_match[2] == "ns") {
            cycles = static_cast<uint64_t>(std::stof(pieces_match[1]) * cycles_per_sec /  uint_constants::one_billion);
          } else {
            cycles = static_cast<uint64_t>(std::stof(pieces_match[1]));
          }
        
          std::string msg("Parsed aie_trace_start_time: " + std::to_string(cycles) + " cycles.");
          xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);

        } catch (const std::exception& ) {
          // User specified number cannot be parsed
          std::string msg("Unable to parse aie_trace_start_time. Setting start time to 0.");
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        }
      } else {
        std::string msg("Unable to parse aie_trace_start_time. Setting start time to 0.");
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      }

    if (cycles > std::numeric_limits<uint32_t>::max())
        useOneDelayCtr = false;

      useDelay = (cycles != 0);
      delayCycles = cycles;
    } else if (startType == "iteration") {
      // Start trace when graph iterator reaches a threshold
      iterationCount = xrt_core::config::get_aie_trace_settings_start_iteration();
      useGraphIterator = (iterationCount != 0);
    } else if (startType == "kernel_event0") {
      // Start trace using user events
      useUserControl = true;
    }

  }

  void AieTraceMetadata::read_aie_metadata(const char* data, size_t size, pt::ptree& aie_project)
  {
    std::stringstream aie_stream;
    aie_stream.write(data,size);
    pt::read_json(aie_stream,aie_project);
  }

  std::vector<std::string> 
  AieTraceMetadata::get_graphs(const xrt_core::device* device)
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

  std::vector<tile_type> 
  AieTraceMetadata::get_aie_tiles(const xrt_core::device* device, const std::string& graph_name)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    
    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();

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
        tiles.at(count++).row = std::stoul(node.second.data()) + rowOffset;
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

  std::vector<tile_type> 
  AieTraceMetadata::get_mem_tiles(const xrt_core::device* device, const std::string& graph_name)
  {
    if (getHardwareGen() == 1) 
      return {};

    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);

    // Grab all shared buffers
    auto sharedBufferTree = aie_meta.get_child_optional("aie_metadata.TileMapping.SharedBufferToTileMapping");
    if (!sharedBufferTree)
      return {};

    std::vector<tile_type> allTiles;
    std::vector<tile_type> memTiles;
    // Always one row of interface tiles
    uint16_t rowOffset = 1;

    // Now parse all shared buffers
    for (auto const &shared_buffer : sharedBufferTree.get()) {
      if (shared_buffer.second.get<std::string>("graph") != graph_name)
        continue;

      tile_type tile;
      tile.col = shared_buffer.second.get<uint16_t>("column");
      tile.row = shared_buffer.second.get<uint16_t>("row") + rowOffset;
      allTiles.emplace_back(std::move(tile));
    }

    std::unique_copy(allTiles.begin(), allTiles.end(), std::back_inserter(memTiles), tileCompare);
    return memTiles;
  }

  std::vector<tile_type> 
  AieTraceMetadata::get_tiles(const xrt_core::device* device, const std::string& graph_name,
                              module_type type, const std::string& kernel_name)
  {
    if (kernel_name.empty() || (kernel_name.compare("all") == 0)) {
      if (type == module_type::mem_tile)
        return get_mem_tiles(device, graph_name);
      return get_aie_tiles(device, graph_name);
    }

    // Now search by graph-kernel pairs
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);

    // Grab all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping)
      return {};

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();

    for (auto const &mapping : kernelToTileMapping.get()) {
      if (mapping.second.get<std::string>("graph") != graph_name)
        continue;

      std::vector<std::string> names;
      std::string functionStr = mapping.second.get<std::string>("function");
      boost::split(names, functionStr, boost::is_any_of("."));
      for (auto &name: names) {
        if (name.compare(kernel_name) == 0) {
          tile_type tile;
          tile.col = mapping.second.get<uint16_t>("column");
          tile.row = mapping.second.get<uint16_t>("row") + rowOffset;
          tiles.emplace_back(std::move(tile));
          break;
        }
      }
    }
    return tiles;
  }

  void
  AieTraceMetadata::getConfigMetricsForTiles(std::vector<std::string>& metricsSettings,
                                             std::vector<std::string>& graphMetricsSettings,
                                             module_type type)
  {
    // Make sure settings are available and appropriate
    if (metricsSettings.empty() && graphMetricsSettings.empty()) {
      return;
    }
    if ((getHardwareGen() == 1) && (type == module_type::mem_tile)) {
      xrt_core::message::send(severity_level::warning, "XRT",
        "MEM tiles are not available in AIE1. Trace settings will be ignored.");
      return;
    }
      
    auto tileName = (type == module_type::mem_tile) ? "mem" : "aie";
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    // STEP 1 : Parse per-graph or per-kernel settings
    /* AIE_trace_settings config format ; Multiple values can be specified for a metric separated with ';'
     * AI Engine Tiles
     * graph_based_aie_tile_metrics = <graph name|all>:<kernel name|all>:<off|functions|functions_partial_stalls|functions_all_stalls>
     * MEM Tiles (AIE2 and beyond)
     * graph_based_mem_tile_metrics = <graph name|all>:<kernel name|all>:<off|input_channels|input_channels_stalls|output_channels|output_channels_stalls>[:<channel 1>][:<channel 2>]
     */

    std::vector<std::vector<std::string>> graphMetrics(graphMetricsSettings.size());

    std::set<tile_type> allValidTiles;
    auto graphs = get_graphs(device.get());
    for (auto& graph : graphs) {
      std::vector<tile_type> currTiles = get_tiles(device.get(), graph, type);
      std::copy(currTiles.begin(), currTiles.end(), std::inserter(allValidTiles, allValidTiles.end()));
    }

    // Graph Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(graphMetrics[i], graphMetricsSettings[i], boost::is_any_of(":"));

      if (graphMetrics[i][0].compare("all") != 0) {
        continue;
      }

      auto tiles = get_tiles(device.get(), graphMetrics[i][0], type, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; MEM tiles only)
      if (graphMetrics[i].size() == 5) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(graphMetrics[i][3]);
            configChannel1[e] = std::stoi(graphMetrics[i][4]);
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_" << tileName 
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Check if already processed
      if (graphMetrics[i][0].compare("all") == 0)
        continue;

      // Check if specified graph exists
      auto graphs = get_graphs(device.get());
      if (!graphs.empty() && (std::find(graphs.begin(), graphs.end(), graphMetrics[i][0]) == graphs.end())) {
        std::stringstream msg;
        msg << "Could not find graph named " << graphMetrics[i][0] 
            << ", as specified in graph_based_" << tileName << "_tile_metrics configuration."
            << " Following graphs are present in the design : " << graphs[0];
        for (size_t j = 1; j < graphs.size(); j++) {
          msg << ", " + graphs[j];
        }
        msg << ".";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = get_tiles(device.get(), graphMetrics[i][0], type, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; MEM tiles only)
      if (graphMetrics[i].size() == 5) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(graphMetrics[i][3]);
            configChannel1[e] = std::stoi(graphMetrics[i][4]);
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 2

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /*
     * AI Engine Tiles
     * Single or all tiles
     * tile_based_aie_tile_metrics = <{<column>,<row>}|all>:<off|functions|functions_partial_stalls|functions_all_stalls>
     * Range of tiles
     * tile_based_aie_tile_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|functions|functions_partial_stalls|functions_all_stalls>
     *  
     * MEM Tiles (AIE2 and beyond)
     * Single or all tiles
     * tile_based_mem_tile_metrics = <{<column>,<row>}|all>:<off|input_channels|input_channels_stalls|output_channels|output_channels_stalls>[:<channel 1>][:<channel 2>]
     * Range of tiles
     * tile_based_mem_tile_metrics = {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|input_channels|input_channels_stalls|output_channels|output_channels_stalls>[:<channel 1>][:<channel 2>]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (metrics[i][0].compare("all") != 0)
        continue;

      for (auto &e : allValidTiles) {
        configMetrics[e] = metrics[i][1];
      }

      // Grab channel numbers (if specified; MEM tiles only)
      if (metrics[i].size() == 4) {
        try {
          for (auto &e : allValidTiles) {
            configChannel0[e] = std::stoi(metrics[i][2]);
            configChannel1[e] = std::stoi(metrics[i][3]);
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Pass 1 

    // Pass 2 : process only range of tiles metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      if ((metrics[i].size() != 3) && (metrics[i].size() != 5))
        continue;
      
      uint32_t minCol = 0, minRow = 0;
      uint32_t maxCol = 0, maxRow = 0;

      try {
        for (size_t j = 0; j < metrics[i].size(); ++j) {
          boost::replace_all(metrics[i][j], "{", "");
          boost::replace_all(metrics[i][j], "}", "");
        }

        std::vector<std::string> minTile;
        boost::split(minTile, metrics[i][0], boost::is_any_of(","));
        minCol = std::stoi(minTile[0]);
        minRow = std::stoi(minTile[1]);

        std::vector<std::string> maxTile;
        boost::split(maxTile, metrics[i][1], boost::is_any_of(","));
        maxCol = std::stoi(maxTile[0]);
        maxRow = std::stoi(maxTile[1]);
      } catch (...) {
        std::stringstream msg;
        msg << "Tile range specification in tile_based_" << tileName
            << "_tile_metrics is not of valid format and hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());           
      }

      // Ensure range is valid 
      if ((minCol > maxCol) || (minRow > maxRow)) {
        std::stringstream msg;
        msg << "Tile range specification in tile_based_" << tileName 
            << "_tile_metrics is not of valid format and hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      uint8_t channel0 = 0;
      uint8_t channel1 = 1;
      if (metrics[i].size() == 5) {
        try {
          channel0 = std::stoi(metrics[i][3]);
          channel1 = std::stoi(metrics[i][4]);
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }

      for (uint32_t col = minCol; col <= maxCol; ++col) {
        for (uint32_t row = minRow; row <= maxRow; ++row) {
          tile_type tile;
          tile.col = col;
          tile.row = row;

          // Make sure tile is used
          if (allValidTiles.find(tile) == allValidTiles.end()) {
            std::stringstream msg;
            msg << "Specified Tile {" << std::to_string(tile.col) << ","
                << std::to_string(tile.row) << "} is not active. Hence skipped.";
            xrt_core::message::send(severity_level::warning, "XRT", msg.str());
            continue;
          }
          
          configMetrics[tile] = metrics[i][2];

          // Grab channel numbers (if specified; MEM tiles only)
          if (metrics[i].size() == 5) {
            configChannel0[tile] = channel0;
            configChannel1[tile] = channel1;
          }
        }
      }
    } // Pass 2

    // Pass 3 : process only single tile metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Check if already processed
      if ((metrics[i][0].compare("all") == 0) || (metrics[i].size() == 3)
          || (metrics[i].size() == 5))
        continue;

      uint16_t col = 0;
      uint16_t row = 0;

      try {
        boost::replace_all(metrics[i][0], "{", "");
        boost::replace_all(metrics[i][0], "}", "");

        std::vector<std::string> tilePos;
        boost::split(tilePos, metrics[i][0], boost::is_any_of(","));
        col = std::stoi(tilePos[0]);
        row = std::stoi(tilePos[1]);
      } catch (...) {
        std::stringstream msg;
        msg << "Tile specification in tile_based_" << tileName
            << "_tile_metrics is not valid format and hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      tile_type tile;
      tile.col = col;
      tile.row = row;

      // Make sure tile is used
      if (allValidTiles.find(tile) == allValidTiles.end()) {
        std::stringstream msg;
        msg << "Specified Tile {" << std::to_string(tile.col) << ","
            << std::to_string(tile.row) << "} is not active. Hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      configMetrics[tile] = metrics[i][2];
      
      // Grab channel numbers (if specified; MEM tiles only)
      if (metrics[i].size() == 4) {
        try {
          configChannel0[tile] = std::stoi(metrics[i][2]);
          configChannel1[tile] = std::stoi(metrics[i][3]);
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Pass 3 

    // Check validity and remove "off" tiles
    std::vector<tile_type> offTiles;

    for (auto &tileMetric : configMetrics) {
      // Save list of "off" tiles
      if (tileMetric.second.empty() || (tileMetric.second.compare("off") == 0)) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      if (std::find(metricSets.begin(), metricSets.end(), tileMetric.second) == metricSets.end()) {
        std::string defaultSet = (type == module_type::mem_tile) ? "input_channels" : "functions";
        std::stringstream msg;
        msg << "Unable to find AIE trace metric set " << tileMetric.second 
            << ". Using default of " << defaultSet << ".";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        tileMetric.second = defaultSet;
      }
    }

    // Remove all the "off" tiles
    for (auto &t : offTiles) {
      configMetrics.erase(t);
    }

    // If needed, turn on debug fal messages
    // xaiefal::Logger::get().setLogLevel(xaiefal::LogLevel::DEBUG);
  }

  std::vector<gmio_type> 
  AieTraceMetadata::get_trace_gmios(const xrt_core::device* device)
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

  std::vector<tile_type>
  AieTraceMetadata::get_event_tiles(const xrt_core::device* device, const std::string& graph_name,
                                    module_type type)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);

    // Not supported yet
    if (type == module_type::shim)
      return {};

    const char* col_name = (type == module_type::core) ? "core_columns" : "dma_columns";
    const char* row_name = (type == module_type::core) ?    "core_rows" :    "dma_rows";

    std::vector<tile_type> tiles;
  
    for (auto& graph : aie_meta.get_child("aie_metadata.EventGraphs")) {
      if (graph.second.get<std::string>("name") != graph_name)
        continue;

      int count = 0;
        for (auto& node : graph.second.get_child(col_name)) {
          tiles.push_back(tile_type());
          auto& t = tiles.at(count++);
          t.col = std::stoul(node.second.data());
        }

        int num_tiles = count;
        count = 0;
        for (auto& node : graph.second.get_child(row_name))
          tiles.at(count++).row = std::stoul(node.second.data());
        throw_if_error(count < num_tiles,"rows < num_tiles");
    }

    return tiles;
  }

   uint8_t AieTraceMetadata::getMetricSetIndex(std::string metricString){    
    auto itr = std::find(metricSets.begin(), metricSets.end(), metricString);
    if (itr != metricSets.cend()){
      return 0;
    } else {
      return std::distance(metricSets.begin(), itr);
    }
  }
  
}
