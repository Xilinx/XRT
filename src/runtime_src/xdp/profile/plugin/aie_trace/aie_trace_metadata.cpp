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

#include "aie_trace_metadata.h"

#include <cstdint>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <regex>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
#include "core/edge/common/aie_parser.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {
  namespace pt = boost::property_tree;
  using severity_level = xrt_core::message::severity_level;
  
  // Constructor
  AieTraceMetadata::AieTraceMetadata(uint64_t deviceID, void* handle)
  : deviceID(deviceID)
  , handle(handle)
  {
    // Verify settings from xrt.ini
    checkSettings();

    counterScheme = xrt_core::config::get_aie_trace_settings_counter_scheme();
    // Get polling interval (in usec)
    pollingInterval = xrt_core::config::get_aie_trace_settings_poll_timers_interval_us();

    // Check whether continuous trace is enabled in xrt.ini
    // AIE trace is now supported for HW only
    continuousTrace = xrt_core::config::get_aie_trace_settings_periodic_offload();
    if (continuousTrace)
      offloadIntervalUs = xrt_core::config::get_aie_trace_settings_buffer_offload_interval_us();

    //Process the file dump interval
    aie_trace_file_dump_int_s = xrt_core::config::get_aie_trace_settings_file_dump_interval_s();
    
    if (aie_trace_file_dump_int_s < MIN_TRACE_DUMP_INTERVAL_S) {
      aie_trace_file_dump_int_s = MIN_TRACE_DUMP_INTERVAL_S;
      xrt_core::message::send(severity_level::warning, "XRT", AIE_TRACE_DUMP_INTERVAL_WARN_MSG);
    }

    // Grab AIE metadata
    auto device = xrt_core::get_userpf_device(handle);
    auto data = device->get_axlf_section(AIE_METADATA);
    invalidXclbinMetadata = (!data.first || !data.second);
    aie::readAIEMetadata(data.first, data.second, aieMeta);

    // Catch when compile-time trace is specified (e.g., --event-trace=functions)
    auto compilerOptions = aie::getAIECompilerOptions(aieMeta);
    setRuntimeMetrics(compilerOptions.event_trace == "runtime");

    if (!getRuntimeMetrics()) {
      std::stringstream msg;
      msg << "Found compiler trace option of " << compilerOptions.event_trace
          << ". No runtime AIE metrics will be changed.";
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
    }

    // Process AIE_trace_settings metrics
    auto aieTileMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_tile_based_aie_tile_metrics());
    auto aieGraphMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_graph_based_aie_tile_metrics());
    auto memTileMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_tile_based_memory_tile_metrics());
    auto memGraphMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_graph_based_memory_tile_metrics());
    auto shimTileMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_tile_based_interface_tile_metrics());
    auto shimGraphMetricsSettings = 
        getSettingsVector(xrt_core::config::get_aie_trace_settings_graph_based_interface_tile_metrics());

    if (aieTileMetricsSettings.empty() && aieGraphMetricsSettings.empty()
        && memTileMetricsSettings.empty() && memGraphMetricsSettings.empty()
        && shimTileMetricsSettings.empty() && shimGraphMetricsSettings.empty()) {
      isValidMetrics = false;
    } else {
      getConfigMetricsForTiles(aieTileMetricsSettings, aieGraphMetricsSettings, module_type::core);
      getConfigMetricsForTiles(memTileMetricsSettings, memGraphMetricsSettings, module_type::mem_tile);
      getConfigMetricsForInterfaceTiles(shimTileMetricsSettings, shimGraphMetricsSettings);
      setTraceStartControl();
    }
  }

  // **************************************************************************
  // Helpers
  // **************************************************************************

  // Verify user settings in xrt.ini
  void AieTraceMetadata::checkSettings()
  {
    using boost::property_tree::ptree;
    const std::set<std::string> validSettings {
      "graph_based_aie_tile_metrics", "tile_based_aie_tile_metrics",
      "graph_based_memory_tile_metrics", "tile_based_memory_tile_metrics",
      "graph_based_interface_tile_metrics", "tile_based_interface_tile_metrics",
      "start_type", "start_time", "start_iteration", "end_type",
      "periodic_offload", "reuse_buffer", "buffer_size", 
      "buffer_offload_interval_us", "file_dump_interval_s",
      "enable_system_timeline"
    };
    const std::map<std::string, std::string> deprecatedSettings {
      {"aie_trace_metrics", "AIE_trace_settings.graph_based_aie_tile_metrics or tile_based_aie_tile_metrics"},
      {"aie_trace_start_time", "AIE_trace_settings.start_time"},
      {"aie_trace_periodic_offload", "AIE_trace_settings.periodic_offload"},
      {"aie_trace_buffer_size", "AIE_trace_settings.buffer_size"}
    };

    // Verify settings in AIE_trace_settings section
    auto tree1 = xrt_core::config::detail::get_ptree_value("AIE_trace_settings");
    for (ptree::iterator pos = tree1.begin(); pos != tree1.end(); pos++) {
      if (validSettings.find(pos->first) == validSettings.end()) {
        std::stringstream msg;
        msg << "The setting AIE_trace_settings." << pos->first << " is not recognized. "
            << "Please check the spelling and compare to supported list:";
        for (auto it = validSettings.cbegin(); it != validSettings.cend(); it++)
          msg << ((it == validSettings.cbegin()) ? " " : ", ") << *it;
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      }
    }

    // Check for deprecated settings
    auto tree2 = xrt_core::config::detail::get_ptree_value("Debug");
    for (ptree::iterator pos = tree2.begin(); pos != tree2.end(); pos++) {
      auto iter = deprecatedSettings.find(pos->first);
      if (iter != deprecatedSettings.end()) {
        std::stringstream msg;
        msg << "The setting Debug." << pos->first << " is deprecated. "
            << "Please instead use " << iter->second << ".";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      }
    }
  }

  // Parse trace start time or events
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
          xrt_core::message::send(severity_level::info, "XRT", msg);

        } catch (const std::exception& ) {
          // User specified number cannot be parsed
          std::string msg("Unable to parse aie_trace_start_time. Setting start time to 0.");
          xrt_core::message::send(severity_level::warning, "XRT", msg);
        }
      } else {
        std::string msg("Unable to parse aie_trace_start_time. Setting start time to 0.");
        xrt_core::message::send(severity_level::warning, "XRT", msg);
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

  // Parse user setting string and convert to vector
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

  // Get index of metric set 
  // NOTE: called by PS kernel on x86
  uint8_t AieTraceMetadata::getMetricSetIndex(std::string metricString) 
  {
    auto aieIter = 
        std::find(metricSets[module_type::core].begin(), metricSets[module_type::core].end(), metricString);
    if (aieIter != metricSets[module_type::core].cend())
      return std::distance(metricSets[module_type::core].begin(), aieIter);

    auto memIter =
        std::find(metricSets[module_type::mem_tile].begin(), metricSets[module_type::mem_tile].end(), metricString);
    if (memIter != metricSets[module_type::mem_tile].cend())
      return std::distance(metricSets[module_type::mem_tile].begin(), memIter);

    auto shimIter =
        std::find(metricSets[module_type::shim].begin(), metricSets[module_type::shim].end(), metricString);
    if (shimIter != metricSets[module_type::shim].cend())
      return std::distance(metricSets[module_type::shim].begin(), shimIter);

    return 0;
  }

  // **************************************************************************
  // Parse Configuration Metrics
  // **************************************************************************

  // Resolve metrics for AIE or memory tiles 
  void
  AieTraceMetadata::getConfigMetricsForTiles(std::vector<std::string>& metricsSettings,
                                             std::vector<std::string>& graphMetricsSettings,
                                             module_type type)
  {
    // Make sure settings are available and appropriate
    if (metricsSettings.empty() && graphMetricsSettings.empty())
      return;
    if ((getHardwareGen() == 1) && (type == module_type::mem_tile)) {
      xrt_core::message::send(severity_level::warning, "XRT",
        "Memory tiles are not available in AIE1. Trace settings will be ignored.");
      return;
    }
      
    uint16_t rowOffset = (type == module_type::mem_tile) ? 1 : getRowOffset();
    auto tileName = (type == module_type::mem_tile) ? "memory" : "aie";

    auto allValidGraphs = aie::getValidGraphs(aieMeta);
    auto allValidKernels = aie::getValidKernels(aieMeta);

    std::set<tile_type> allValidTiles;
    auto validTilesVec = aie::getTiles(aieMeta, "all", type);
    std::unique_copy(validTilesVec.begin(), validTilesVec.end(), std::inserter(allValidTiles, allValidTiles.end()), 
                     xdp::aie::tileCompare);

    // STEP 1 : Parse per-graph or per-kernel settings
    /* AIE_trace_settings config format ; Multiple values can be specified for a metric separated with ';'
     * AI Engine Tiles
     * graph_based_aie_tile_metrics = <graph name|all>:<kernel name|all>:<off|functions|functions_partial_stalls|functions_all_stalls>
     * Memory Tiles (AIE2 and beyond)
     * graph_based_memory_tile_metrics = 
     *     <graph name|all>:<kernel name|all>:<off|input_channels|input_channels_stalls|output_channels|output_channels_stalls>[:<channel 1>][:<channel 2>]
     */

    std::vector<std::vector<std::string>> graphMetrics(graphMetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(graphMetrics[i], graphMetricsSettings[i], boost::is_any_of(":"));

      // Check if graph is not all or if invalid kernel
      if (graphMetrics[i][0].compare("all") != 0)
        continue;
      if ((graphMetrics[i][1].compare("all") != 0)
          && (std::find(allValidKernels.begin(), allValidKernels.end(), graphMetrics[i][1]) == allValidKernels.end())) {
        std::stringstream msg;
        msg << "Could not find kernel " << graphMetrics[i][1] 
            << " as specified in graph_based_" << tileName << "_metrics setting."
            << " The following kernels are valid : " << allValidKernels[0];
        for (size_t j = 1; j < allValidKernels.size(); j++)
          msg << ", " << allValidKernels[j];
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = aie::getTiles(aieMeta, graphMetrics[i][0], type, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(graphMetrics[i][3]);
            configChannel1[e] = std::stoi(graphMetrics[i].back());
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
      // Check if already processed or if invalid
      if (graphMetrics[i][0].compare("all") == 0)
        continue;
      if (std::find(allValidGraphs.begin(), allValidGraphs.end(), graphMetrics[i][0]) == allValidGraphs.end()) {
        std::stringstream msg;
        msg << "Could not find graph " << graphMetrics[i][0] 
            << ", as specified in graph_based_" << tileName << "_tile_metrics setting."
            << " The following graphs are valid : " << allValidGraphs[0];
        for (size_t j = 1; j < allValidGraphs.size(); j++)
          msg << ", " + allValidGraphs[j];
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }
      if ((graphMetrics[i][1].compare("all") != 0)
          && (std::find(allValidKernels.begin(), allValidKernels.end(), graphMetrics[i][1]) == allValidKernels.end())) {
        std::stringstream msg;
        msg << "Could not find kernel " << graphMetrics[i][1] 
            << " as specified in graph_based_" << tileName << "_metrics setting."
            << " The following kernels are valid : " << allValidKernels[0];
        for (size_t j = 1; j < allValidKernels.size(); j++)
          msg << ", " << allValidKernels[j];
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = aie::getTiles(aieMeta, graphMetrics[i][0], type, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(graphMetrics[i][3]);
            configChannel1[e] = std::stoi(graphMetrics[i].back());
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
     * Memory Tiles (AIE2 and beyond)
     * Single or all tiles
     * tile_based_memory_tile_metrics = 
     *     <{<column>,<row>}|all>:<off|input_channels|input_channels_stalls|output_channels|output_channels_stalls>[:<channel 1>][:<channel 2>]
     * Range of tiles
     * tile_based_memory_tile_metrics = 
     *     {<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|input_channels|input_channels_stalls|output_channels|output_channels_stalls>[:<channel 1>][:<channel 2>]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if ((metrics[i][0].compare("all") != 0) || (metrics[i].size() < 2))
        continue;

      auto tiles = aie::getTiles(aieMeta, metrics[i][0], type);
      for (auto &e : tiles) {
        configMetrics[e] = metrics[i][1];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (metrics[i].size() > 2) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(metrics[i][2]);
            configChannel1[e] = std::stoi(metrics[i].back());
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
        minRow = std::stoi(minTile[1]) + rowOffset;

        std::vector<std::string> maxTile;
        boost::split(maxTile, metrics[i][1], boost::is_any_of(","));
        maxCol = std::stoi(maxTile[0]);
        maxRow = std::stoi(maxTile[1]) + rowOffset;
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
      if (metrics[i].size() > 3) {
        try {
          channel0 = std::stoi(metrics[i][3]);
          channel1 = std::stoi(metrics[i].back());
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

          // Grab channel numbers (if specified; memory tiles only)
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
        row = std::stoi(tilePos[1]) + rowOffset;
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

      configMetrics[tile] = metrics[i][1];
      
      // Grab channel numbers (if specified; memory tiles only)
      if (metrics[i].size() > 2) {
        try {
          configChannel0[tile] = std::stoi(metrics[i][2]);
          configChannel1[tile] = std::stoi(metrics[i].back());
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Pass 3 

    // Set default, check validity, and remove "off" tiles
    bool showWarning = true;
    std::vector<tile_type> offTiles;
    auto defaultSet = defaultSets[type];
    auto coreSets = metricSets[module_type::core];
    auto memSets = metricSets[module_type::mem_tile];

    for (auto &tileMetric : configMetrics) {
      // Ignore other types of tiles
      if (allValidTiles.find(tileMetric.first) == allValidTiles.end())
        continue;
      // Save list of "off" tiles
      if (tileMetric.second.empty() || (tileMetric.second.compare("off") == 0)) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      if (((type != module_type::mem_tile) 
          && (std::find(coreSets.begin(), coreSets.end(), tileMetric.second) == coreSets.cend()))
          || ((type == module_type::mem_tile) 
          && (std::find(memSets.begin(), memSets.end(), tileMetric.second) == memSets.cend()))) {
        if (showWarning) {
          std::stringstream msg;
          msg << "Unable to find AIE trace metric set " << tileMetric.second 
              << ". Using default of " << defaultSet << ".";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
          showWarning = false;
        }
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

  // Resolve metrics for interface tiles
  void AieTraceMetadata::getConfigMetricsForInterfaceTiles(const std::vector<std::string>& metricsSettings,
                                                           const std::vector<std::string> graphMetricsSettings)
  {
    if ((metricsSettings.empty()) && (graphMetricsSettings.empty()))
      return;

    auto allValidGraphs = aie::getValidGraphs(aieMeta);
    auto allValidPorts  = aie::getValidPorts(aieMeta);
    
    // STEP 1 : Parse per-graph or per-kernel settings
    /* AIE_trace_settings config format ; Multiple values can be specified for a metric separated with ';'
     * Interface Tiles
     * graph_based_interface_tile_metrics = <graph name|all>:<port name|all>:<ports|input_ports|input_ports_stalls|output_ports|output_ports_stalls>[:<channel 1>][:<channel 2>]
     */

    std::vector<std::vector<std::string>> graphMetrics(graphMetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(graphMetrics[i], graphMetricsSettings[i], boost::is_any_of(":"));

      // Check if graph is not all or if invalid port
      if (graphMetrics[i][0].compare("all") != 0)
        continue;
      if ((graphMetrics[i][1].compare("all") != 0)
          && (std::find(allValidPorts.begin(), allValidPorts.end(), graphMetrics[i][1]) == allValidPorts.end())) {
        std::stringstream msg;
        msg << "Could not find port " << graphMetrics[i][1] 
            << ", as specified in graph_based_interface_tile_metrics setting."
            << " The following ports are valid : " << allValidPorts[0];
        for (size_t j = 1; j < allValidPorts.size(); j++)
          msg << ", " + allValidPorts[j];
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = aie::getInterfaceTiles(aieMeta, graphMetrics[i][0], graphMetrics[i][1], graphMetrics[i][2]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(graphMetrics[i][3]);
            configChannel1[e] = std::stoi(graphMetrics[i].back());
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_interface_metrics " 
              << "are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Check if already processed or if invalid
      if (graphMetrics[i][0].compare("all") == 0)
        continue;
      if (std::find(allValidGraphs.begin(), allValidGraphs.end(), graphMetrics[i][0]) == allValidGraphs.end()) {
        std::stringstream msg;
        msg << "Could not find graph " << graphMetrics[i][0] 
            << ", as specified in graph_based_interface_tile_metrics setting."
            << " The following graphs are valid : " << allValidGraphs[0];
        for (size_t j = 1; j < allValidGraphs.size(); j++)
          msg << ", " + allValidGraphs[j];
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }
      if ((graphMetrics[i][1].compare("all") != 0)
          && (std::find(allValidPorts.begin(), allValidPorts.end(), graphMetrics[i][1]) == allValidPorts.end())) {
        std::stringstream msg;
        msg << "Could not find port " << graphMetrics[i][1] 
            << ", as specified in graph_based_interface_tile_metrics setting."
            << " The following ports are valid : " << allValidPorts[0];
        for (size_t j = 1; j < allValidPorts.size(); j++)
          msg << ", " + allValidPorts[j];
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = aie::getInterfaceTiles(aieMeta, graphMetrics[i][0], graphMetrics[i][1], graphMetrics[i][2]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(graphMetrics[i][3]);
            configChannel1[e] = std::stoi(graphMetrics[i].back());
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_interface_tile_metrics "
              << "are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 2

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for
     * a metric separated with ';' Single or all tiles
     * tile_based_interface_tile_metrics =
     * [[<column|all>:<off|ports|input_ports|input_ports_stalls|output_ports|output_ports_stalls>[:<channel 1>][:<channel 2>]]
     * Range of tiles
     * tile_based_interface_tile_metrics =
     * [<mincolumn>:<maxcolumn>:<off|ports|input_ports|input_ports_stalls|output_ports|output_ports_stalls>[:<channel 1>][:<channel 2>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (metrics[i][0].compare("all") != 0)
        continue;

      int16_t channelId = (metrics[i].size() < 3) ? -1 : static_cast<uint16_t>(std::stoul(metrics[i][2]));
      auto tiles = aie::getInterfaceTiles(aieMeta, metrics[i][0], "all", metrics[i][1], channelId);

      for (auto& e : tiles) {
        configMetrics[e] = metrics[i][1];
      }
    }  // Pass 1

    // Pass 2 : process only range of tiles metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      if ((metrics[i][0].compare("all") == 0) || (metrics[i].size() < 3))
        continue;

      uint32_t maxCol = 0;
      try {
        maxCol = std::stoi(metrics[i][1]);
      }
      catch (std::invalid_argument const&) {
        // maxColumn is not an integer i.e either 1st style or wrong format, skip for now
        continue;
      }
      uint32_t minCol = 0;
      try {
        minCol = std::stoi(metrics[i][0]);
      }
      catch (std::invalid_argument const&) {
        // 2nd style but expected min column is not an integer, give warning and skip
        xrt_core::message::send(severity_level::warning, "XRT",
                                "Minimum column specification in "
                                "tile_based_interface_tile_metrics is not "
                                "an integer and hence skipped.");
        continue;
      }

      int16_t channelId = 0;
      if (metrics[i].size() == 4) {
        try {
          channelId = static_cast<uint16_t>(std::stoul(metrics[i][3]));
        }
        catch (std::invalid_argument const&) {
          // Expected channel Id is not an integer, give warning and
          // ignore channelId
          xrt_core::message::send(severity_level::warning, "XRT",
                                  "Channel ID specification in "
                                  "tile_based_interface_tile_metrics is "
                                  "not an integer and hence ignored.");
          channelId = -1;
        }
      }

      auto tiles = aie::getInterfaceTiles(aieMeta, metrics[i][0], "all", metrics[i][2], 
                                          channelId, true, minCol, maxCol);

      for (auto& t : tiles) {
        configMetrics[t] = metrics[i][2];
      }
    }  // Pass 2

    // Pass 3 : process only single tile metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Skip range specification, invalid format, or already processed
      if ((metrics[i].size() == 4) || (metrics[i].size() < 2) || (metrics[i][0].compare("all") == 0))
        continue;

      uint32_t col = 0;
      try {
        col = std::stoi(metrics[i][1]);
      }
      catch (std::invalid_argument const&) {
        // max column is not a number, so the expected single column specification. Handle this here
        try {
          col = std::stoi(metrics[i][0]);
        }
        catch (std::invalid_argument const&) {
          // Expected column specification is not a number. Give warning and skip
          xrt_core::message::send(severity_level::warning, "XRT",
                                  "Column specification in tile_based_interface_tile_metrics "
                                  "is not an integer and hence skipped.");
          continue;
        }

        int16_t channelId = -1;
        if (metrics[i].size() == 3) {
          try {
            channelId = static_cast<uint16_t>(std::stoul(metrics[i][2]));
          }
          catch (std::invalid_argument const&) {
            // Expected channel Id is not an integer, give warning and ignore channelId
            xrt_core::message::send(severity_level::warning, "XRT",
                                    "Channel ID specification in "
                                    "tile_based_interface_tile_metrics is not an integer "
                                    "and hence ignored.");
            channelId = -1;
          }
        }

        auto tiles = aie::getInterfaceTiles(aieMeta, metrics[i][0], "all", metrics[i][1], 
                                            channelId, true, col, col);

        for (auto& t : tiles) {
          configMetrics[t] = metrics[i][1];
        }
      }
    }  // Pass 3

    // Set default, check validity, and remove "off" tiles
    bool showWarning = true;
    std::vector<tile_type> offTiles;
    auto defaultSet = defaultSets[module_type::shim];
    auto metricVec = metricSets[module_type::shim];

    for (auto& tileMetric : configMetrics) {
      // Ignore other types of tiles
      if (tileMetric.first.row != 0)
        continue;
      // Save list of "off" tiles
      if (tileMetric.second.empty() || (tileMetric.second.compare("off") == 0)) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      if (std::find(metricVec.begin(), metricVec.end(), tileMetric.second) == metricVec.end()) {
        if (showWarning) {
          std::string msg = "Unable to find interface_tile metric set " + tileMetric.second 
                          + ". Using default of " + defaultSet + ". ";
          xrt_core::message::send(severity_level::warning, "XRT", msg);
          showWarning = false;
        }
        tileMetric.second = defaultSet;
      }
    }

    // Remove all the "off" tiles
    for (auto& t : offTiles) {
      configMetrics.erase(t);
    }
  }
  
}
