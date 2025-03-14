/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include "aie_trace_metadata.h"

#include <cstdint>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <regex>

#include "core/common/device.h"
#include "core/common/message.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/database/static_info/aie_util.h"

namespace {
  static bool tileCompare(xdp::tile_type tile1, xdp::tile_type tile2)
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }
} // end anonymous namespace

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
    continuousTrace = xrt_core::config::get_aie_trace_settings_periodic_offload();
    // AIE trace is now supported for HW only
#ifdef XDP_CLIENT_BUILD
    // Default return is flipped on client
    bool isPeriodicOffloadPresent = false;
    auto tree1 = xrt_core::config::detail::get_ptree_value("AIE_trace_settings");
    for (pt::ptree::iterator pos = tree1.begin(); pos != tree1.end(); pos++) {
      if (pos->first == "periodic_offload") {
        isPeriodicOffloadPresent = true;
        break;
      }
    }
    if( !isPeriodicOffloadPresent)
      continuousTrace = false;
#endif

    if (continuousTrace)
      offloadIntervalUs = xrt_core::config::get_aie_trace_settings_buffer_offload_interval_us();

    //Process the file dump interval
    aie_trace_file_dump_int_s = xrt_core::config::get_aie_trace_settings_file_dump_interval_s();
    
    if (aie_trace_file_dump_int_s < MIN_TRACE_DUMP_INTERVAL_S) {
      aie_trace_file_dump_int_s = MIN_TRACE_DUMP_INTERVAL_S;
      xrt_core::message::send(severity_level::warning, "XRT", AIE_TRACE_DUMP_INTERVAL_WARN_MSG);
    }

    metadataReader = (VPDatabase::Instance()->getStaticInfo()).getAIEmetadataReader();
    if (!metadataReader)
      return;
    
    // Make sure compiler trace option is available as runtime
    auto compilerOptions = metadataReader->getAIECompilerOptions();
    setRuntimeMetrics(compilerOptions.event_trace == "runtime");
    if (!getRuntimeMetrics()) {
      std::stringstream msg;
      msg << "AIE trace will not be configured since design was not compiled with --event-trace=runtime."
          << " If runtime configuration is desired, please use --event-trace=runtime.";
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      return;
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
      // Use DMA type here to include both core-active tiles and DMA-only tiles
      getConfigMetricsForTiles(aieTileMetricsSettings, aieGraphMetricsSettings, module_type::dma);
      getConfigMetricsForTiles(memTileMetricsSettings, memGraphMetricsSettings, module_type::mem_tile);
      getConfigMetricsForInterfaceTiles(shimTileMetricsSettings, shimGraphMetricsSettings);
      setTraceStartControl(compilerOptions.graph_iterator_event);
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
      "enable_system_timeline", "poll_timers_interval_us"
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
        msg << "The setting Debug." << pos->first << " is no longer supported. "
            << "Please instead use " << iter->second << ".";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      }
    }
  }

  // Parse trace start time or events
  void AieTraceMetadata::setTraceStartControl(bool graphIteratorEvent)
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
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

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
      // Verify AIE was compiled with the proper setting
      if (!graphIteratorEvent) {
        std::string msg = "Unable to use graph iteration as trace start type. ";
        msg.append("Please re-compile AI Engine with --graph-iterator-event=true.");
        xrt_core::message::send(severity_level::warning, "XRT", msg);
      }
      else {
        // Start trace when graph iterator reaches a threshold
        iterationCount = xrt_core::config::get_aie_trace_settings_start_iteration();
        useGraphIterator = (iterationCount != 0);
      }
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
      return static_cast<uint8_t>(std::distance(metricSets[module_type::core].begin(), aieIter));

    auto memIter =
        std::find(metricSets[module_type::mem_tile].begin(), metricSets[module_type::mem_tile].end(), metricString);
    if (memIter != metricSets[module_type::mem_tile].cend())
      return static_cast<uint8_t>(std::distance(metricSets[module_type::mem_tile].begin(), memIter));

    auto shimIter =
        std::find(metricSets[module_type::shim].begin(), metricSets[module_type::shim].end(), metricString);
    if (shimIter != metricSets[module_type::shim].cend())
      return static_cast<uint8_t>(std::distance(metricSets[module_type::shim].begin(), shimIter));

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
      
    uint8_t rowOffset = (type == module_type::mem_tile) ? 1 : getRowOffset();
    auto tileName = (type == module_type::mem_tile) ? "memory" : "aie";

    auto allValidGraphs = metadataReader->getValidGraphs();
    auto allValidKernels = metadataReader->getValidKernels();

    std::set<tile_type> allValidTiles;
    auto validTilesVec = metadataReader->getTiles("all", type, "all");
    std::unique_copy(validTilesVec.begin(), validTilesVec.end(), 
                     std::inserter(allValidTiles, allValidTiles.end()), tileCompare);

    // STEP 1 : Parse per-graph and/or per-kernel settings

    /* 
     * AIE_trace_settings config format
     * NOTE: Multiple values can be separated with ';'
     *
     * AI Engine Tiles
     * graph_based_aie_tile_metrics = <graph name|all>:<kernel name|all>:<metric set>
     *
     * Memory Tiles (AIE2 and beyond)
     * graph_based_memory_tile_metrics = <graph name|all>:<buffer name|all>:<metric set>[:<channel 1>][:<channel 2>]
     */

    std::set<size_t> processed;
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
        processed.insert(i);
        continue;
      }

      processed.insert(i);
      auto tiles = metadataReader->getTiles(graphMetrics[i][0], type, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = aie::convertStringToUint8(graphMetrics[i][3]);
            configChannel1[e] = aie::convertStringToUint8(graphMetrics[i].back());
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
      // Check if already processed or if invalid kernel
      if ((processed.find(i) != processed.end()) || (graphMetrics[i].size() < 3))
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

      auto tiles = metadataReader->getTiles(graphMetrics[i][0], type, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = aie::convertStringToUint8(graphMetrics[i][3]);
            configChannel1[e] = aie::convertStringToUint8(graphMetrics[i].back());
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 2

    processed.clear();

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /*
     * AIE_trace_settings config format
     * NOTE: Multiple values can be separated with ';' 
     * 
     * AI Engine Tiles
     * Single or all tiles
     * tile_based_aie_tile_metrics = <{<column>,<row>}|all>:<metric set>
     * Range of tiles
     * tile_based_aie_tile_metrics = <mincolumn,<minrow>:<maxcolumn>,<maxrow>:<metric set>
     *  
     * Memory Tiles (AIE2 and beyond)
     * Single or all tiles
     * tile_based_memory_tile_metrics = <column>,<row>|all>:<metric set>[:<channel 1>][:<channel 2>]
     * Range of tiles
     * tile_based_memory_tile_metrics = <mincolumn,<minrow>:<maxcolumn>,<maxrow>:<metric set>[:<channel 1>][:<channel 2>]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if ((metrics[i][0].compare("all") != 0) || (metrics[i].size() < 2))
        continue;

      processed.insert(i);
      auto tiles = metadataReader->getTiles(metrics[i][0], type, "all");
      for (auto &e : tiles) {
        configMetrics[e] = metrics[i][1];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (metrics[i].size() > 2) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = aie::convertStringToUint8(metrics[i][2]);
            configChannel1[e] = aie::convertStringToUint8(metrics[i].back());
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
      if ((processed.find(i) != processed.end()) || (metrics[i].size() < 3))
        continue;
      
      uint8_t minCol = 0, minRow = 0;
      uint8_t maxCol = 0, maxRow = 0;

      try {
        for (size_t j = 0; j < metrics[i].size(); ++j) {
          boost::replace_all(metrics[i][j], "{", "");
          boost::replace_all(metrics[i][j], "}", "");
        }

        std::vector<std::string> minTile;
        boost::split(minTile, metrics[i][0], boost::is_any_of(","));
        minCol = aie::convertStringToUint8(minTile[0]);
        minRow = aie::convertStringToUint8(minTile[1]) + rowOffset;

        std::vector<std::string> maxTile;
        boost::split(maxTile, metrics[i][1], boost::is_any_of(","));
        maxCol = aie::convertStringToUint8(maxTile[0]);
        maxRow = aie::convertStringToUint8(maxTile[1]) + rowOffset;
      } catch (...) {
        std::stringstream msg;
        msg << "Valid Tile range specification in tile_based_" << tileName
            << "_tile_metrics is not met, it will re-processed for single-tile specification.";
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
        continue;       
      }

      processed.insert(i);

      // Ensure range is valid 
      if ((minCol > maxCol) || (minRow > maxRow)) {
        std::stringstream msg;
        msg << "Tile range specification in tile_based_" << tileName
            << "_tile_metrics is not of valid range ({col1,row1}<={col2,row2}) and hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      uint8_t channel0 = 0;
      uint8_t channel1 = 1;
      if (metrics[i].size() > 3) {
        try {
          channel0 = aie::convertStringToUint8(metrics[i][3]);
          channel1 = aie::convertStringToUint8(metrics[i].back());
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }

      for (uint8_t col = minCol; col <= maxCol; ++col) {
        for (uint8_t row = minRow; row <= maxRow; ++row) {
          tile_type tile;
          tile.col = col;
          tile.row = row;
          tile.active_core   = true;
          tile.active_memory = true;

          // Make sure tile is used
          auto it = std::find_if(allValidTiles.begin(), allValidTiles.end(),
                                 compareTileByLoc(tile));
          if (it == allValidTiles.end()) {
            std::stringstream msg;
            msg << "Specified Tile {" << std::to_string(tile.col) << ","
                << std::to_string(tile.row) << "} is not active. Hence skipped.";
            xrt_core::message::send(severity_level::warning, "XRT", msg.str());
            continue;
          }
          
          configMetrics[tile] = metrics[i][2];

          // Grab channel numbers (if specified; memory .tiles only)
          if (metrics[i].size() > 3) {
            configChannel0[tile] = channel0;
            configChannel1[tile] = channel1;
          }
        }
      }
    } // Pass 2

    // Pass 3 : process only single tile metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Check if already processed or invalid format
      if ((processed.find(i) != processed.end()) || (metrics[i].size() < 2))
        continue;

      uint8_t col = 0;
      uint8_t row = 0;

      try {
        boost::replace_all(metrics[i][0], "{", "");
        boost::replace_all(metrics[i][0], "}", "");

        std::vector<std::string> tilePos;
        boost::split(tilePos, metrics[i][0], boost::is_any_of(","));
        col = aie::convertStringToUint8(tilePos[0]);
        row = aie::convertStringToUint8(tilePos[1]) + rowOffset;
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
      tile.active_core   = true;
      tile.active_memory = true;

      // Make sure tile is used
      auto it = std::find_if(allValidTiles.begin(), allValidTiles.end(),
                             compareTileByLoc(tile));
      if (it == allValidTiles.end()) {
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
          configChannel0[tile] = aie::convertStringToUint8(metrics[i][2]);
          configChannel1[tile] = aie::convertStringToUint8(metrics[i].back());
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << tileName
              << "_tile_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Pass 3 

    processed.clear();

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
  }

  // Resolve metrics for interface tiles
  void AieTraceMetadata::getConfigMetricsForInterfaceTiles(const std::vector<std::string>& metricsSettings,
                                                           const std::vector<std::string> graphMetricsSettings)
  {
    if ((metricsSettings.empty()) && (graphMetricsSettings.empty()))
      return;

    auto allValidGraphs = metadataReader->getValidGraphs();
    auto allValidPorts  = metadataReader->getValidPorts();
    
    // STEP 1 : Parse per-graph or per-kernel settings

    /* 
     * AIE_trace_settings config format
     * NOTE: Multiple values can be separated with ';'
     * 
     * Interface Tiles
     * graph_based_interface_tile_metrics = <graph name|all>:<port name|all>:<metric set>[:<channel 1>][:<channel 2>]
     */

    std::set<size_t> processed;
    std::vector<std::vector<std::string>> graphMetrics(graphMetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(graphMetrics[i], graphMetricsSettings[i], boost::is_any_of(":"));

      // Check if graph is not all, invalid format, or invalid port
      if ((graphMetrics[i][0].compare("all") != 0) || (graphMetrics[i].size() < 3))
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
        processed.insert(i);
        continue;
      }

      auto tiles = metadataReader->getInterfaceTiles(graphMetrics[i][0], graphMetrics[i][1], graphMetrics[i][2]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = aie::convertStringToUint8(graphMetrics[i][3]);
            configChannel1[e] = aie::convertStringToUint8(graphMetrics[i].back());
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
      // Check if already processed, invalid format, or invalid port
      if ((processed.find(i) != processed.end()) || (graphMetrics[i].size() < 3))
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

      auto tiles = metadataReader->getInterfaceTiles(graphMetrics[i][0], graphMetrics[i][1], graphMetrics[i][2]);
      for (auto &e : tiles) {
        configMetrics[e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = aie::convertStringToUint8(graphMetrics[i][3]);
            configChannel1[e] = aie::convertStringToUint8(graphMetrics[i].back());
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_interface_tile_metrics "
              << "are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 2

    processed.clear();

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* 
     * AIE_trace_settings config format
     * NOTE: Multiple values can be separated with ';' 
     * 
     * Single or all tiles
     * tile_based_interface_tile_metrics = <column|all>:<metric set>[:<channel>]
     * 
     * Range of tiles
     * tile_based_interface_tile_metrics = <mincolumn>:<maxcolumn>:<metric set>[:<channel>]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting
    // all:<metric>[:<channel0>[:<channel1>]]
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      // Check if not all tiles or invalid format
      if ((metrics[i][0].compare("all") != 0) || (metrics[i].size() < 2))
        continue;

      processed.insert(i);
      
      // By default, select all channels
      bool foundChannels = false;
      uint8_t channelId0 = 0;
      uint8_t channelId1 = 1;
      if (metrics[i].size() > 2) {
        foundChannels = true;
        channelId0 = aie::convertStringToUint8(metrics[i][2]);
        channelId1 = (metrics[i].size() < 4) ? channelId0 : aie::convertStringToUint8(metrics[i][3]);
      }

      int16_t channelNum = (foundChannels) ? channelId0 : -1;
      auto tiles = metadataReader->getInterfaceTiles(metrics[i][0], "all", metrics[i][1], channelNum);
      
      for (auto& t : tiles) {
        configMetrics[t] = metrics[i][1];
        configChannel0[t] = channelId0;
        configChannel1[t] = channelId1;
      }
    }  // Pass 1

    // Pass 2 : process only range of tiles metric setting
    // <minclumn>:<maxcolumn>:<metric>[:<channel0>[:<channel1>]]
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      if ((processed.find(i) != processed.end()) || (metrics[i].size() < 3))
        continue;

      uint8_t maxCol = 0;
      try {
        maxCol = aie::convertStringToUint8(metrics[i][1]);
      }
      catch (std::invalid_argument const&) {
        // Max column is not an integer, so either first style or wrong format. Skip for now.
        continue;
      }

      uint8_t minCol = 0;
      try {
        minCol = aie::convertStringToUint8(metrics[i][0]);
      }
      catch (std::invalid_argument const&) {
        // Second style but expected min column is not an integer. Give warning and skip.
        xrt_core::message::send(severity_level::warning, "XRT",
                                "Minimum column specification in "
                                "tile_based_interface_tile_metrics is not "
                                "an integer and hence skipped.");
        continue;
      }

      // By-default select both the channels
      bool foundChannels = false;
      uint8_t channelId0 = 0;
      uint8_t channelId1 = 1;
      if (metrics[i].size() >= 4) {
        try {
          foundChannels = true;
          channelId0 = aie::convertStringToUint8(metrics[i][3]);
          channelId1 = (metrics[i].size() == 4) ? channelId0 : aie::convertStringToUint8(metrics[i][4]);
        }
        catch (std::invalid_argument const&) {
          // Expected channel ID is not an integer. Give warning and ignore.
          foundChannels = false;
          xrt_core::message::send(severity_level::warning, "XRT",
                                  "Channel ID specification in "
                                  "tile_based_interface_tile_metrics is "
                                  "not an integer and hence ignored.");
        }
      }

      processed.insert(i);
      int16_t channelNum = (foundChannels) ? channelId0 : -1;
      auto tiles = metadataReader->getInterfaceTiles(metrics[i][0], "all", metrics[i][2],
                                                     channelNum, true, minCol, maxCol);

      for (auto& t : tiles) {
        configMetrics[t] = metrics[i][2];
        configChannel0[t] = channelId0;
        configChannel1[t] = channelId1;
      }
    }  // Pass 2

    // Pass 3 : process only single tile metric setting
    // <singleColumn>:<metric>[:<channel0>[:<channel1>]]
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Skip if already processed or invalid format
      if ((processed.find(i) != processed.end()) || (metrics[i].size() < 2))
        continue;

      uint8_t col = 0;
      try {
        col = aie::convertStringToUint8(metrics[i][1]);
      }
      catch (std::invalid_argument const&) {
        // Max column is not an integer, so expected single column specification. Handle this here.
        try {
          col = aie::convertStringToUint8(metrics[i][0]);
        }
        catch (std::invalid_argument const&) {
          // Expected column specification is not an integer. Give warning and skip.
          xrt_core::message::send(severity_level::warning, "XRT",
                                  "Column specification in tile_based_interface_tile_metrics "
                                  "is not an integer and hence skipped.");
          continue;
        }
 
        // By-default select both the channels
        bool foundChannels = false;
        uint8_t channelId0 = 0;
        uint8_t channelId1 = 1;
        if (metrics[i].size() >= 3) {
          try {
            foundChannels = true;
            channelId0 = aie::convertStringToUint8(metrics[i][2]);
            channelId1 = (metrics[i].size() == 3) ? channelId0 : aie::convertStringToUint8(metrics[i][3]);
          }
          catch (std::invalid_argument const&) {
            // Expected channel ID is not an integer. Give warning and ignore.
            foundChannels = false;
            xrt_core::message::send(severity_level::warning, "XRT",
                                    "Channel ID specification in "
                                    "tile_based_interface_tile_metrics is not an integer "
                                    "and hence ignored.");
          }
        }

        int16_t channelNum = (foundChannels) ? channelId0 : -1;
        auto tiles = metadataReader->getInterfaceTiles(metrics[i][0], "all", metrics[i][1],
                                                       channelNum, true, col, col);

        for (auto& t : tiles) {
          configMetrics[t] = metrics[i][1];
          configChannel0[t] = channelId0;
          configChannel1[t] = channelId1;
        }
      }
    }  // Pass 3

    processed.clear();

    // Set default, check validity, and remove "off" tiles
    bool showWarning = true;
    bool showWarningGMIOMetric = true;
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

      // Check for PLIO tiles and it's compatible metric settings
      if ((tileMetric.first.subtype == io_type::PLIO) && isGMIOMetric(tileMetric.second)) {
        if (showWarningGMIOMetric) {
          std::string msg = "Configured interface_tile metric set " + tileMetric.second 
                          + " is only applicable for GMIO type tiles.";
          xrt_core::message::send(severity_level::warning, "XRT", msg);
          showWarningGMIOMetric = false;
        }

        std::stringstream msg;
        msg << "Configured interface_tile metric set metric set " << tileMetric.second;
        msg << " skipped for tile (" << +tileMetric.first.col << ", " << +tileMetric.first.row << ").";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
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

  aie::driver_config 
  AieTraceMetadata::getAIEConfigMetadata() 
  {
    if (metadataReader)
      return metadataReader->getDriverConfig();
    return {};
  }
  
}
