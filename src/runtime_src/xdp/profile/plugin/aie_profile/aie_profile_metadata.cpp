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

#define XDP_PLUGIN_SOURCE

#include "aie_profile_metadata.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;

  AieProfileMetadata::AieProfileMetadata(uint64_t deviceID, void* handle) :
      deviceID(deviceID)
    , handle(handle)
  {
    xrt_core::message::send(severity_level::info,
                            "XRT", "Parsing AIE Profile Metadata.");

    #ifdef XDP_MINIMAL_BUILD
      metadataReader = aie::readAIEMetadata("aie_control_config.json", aie_meta);
      xrt_core::message::send(severity_level::info,
                            "XRT", "Successfully Read AIE Profile Metadata.");
    #else
      auto device = xrt_core::get_userpf_device(handle);
      auto data = device->get_axlf_section(AIE_METADATA);

      metadataReader = aie::readAIEMetadata(data.first, data.second, aie_meta);
    #endif

    if (metadataReader == nullptr) {
      xrt_core::message::send(severity_level::error,
                            "XRT", "Error parsing AIE Profiling Metadata.");
      return;
    }

    // Verify settings from xrt.ini
    checkSettings();

    configMetrics.resize(NUM_MODULES);
    // Get polling interval (in usec)
    pollingInterval = xrt_core::config::get_aie_profile_settings_interval_us();

    // Setup Config Metrics
    // Get AIE clock frequency
    VPDatabase* db = VPDatabase::Instance();
    clockFreqMhz = (db->getStaticInfo()).getClockRateMHz(deviceID, false);

    // Tile-based metrics settings
    std::vector<std::string> metricsConfig;
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_memory_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_interface_tile_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_memory_tile_metrics());

    // Graph-based metrics settings
    std::vector<std::string> graphMetricsConfig;
    graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_metrics());
    graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_memory_metrics());
    graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_interface_tile_metrics());
    graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_memory_tile_metrics());

    // Process all module types
    for (int module = 0; module < NUM_MODULES; ++module) {
      auto type = moduleTypes[module];
      auto metricsSettings = getSettingsVector(metricsConfig[module]);
      auto graphMetricsSettings = getSettingsVector(graphMetricsConfig[module]);

      if (type == module_type::shim)
        getConfigMetricsForInterfaceTiles(module, metricsSettings, graphMetricsSettings);
      else
        getConfigMetricsForTiles(module, metricsSettings, graphMetricsSettings, type);
    }

    xrt_core::message::send(severity_level::info,
                            "XRT", "Finished Parsing AIE Profile Metadata."); 
  }

  bool tileCompare(tile_type tile1, tile_type tile2)
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }

  void AieProfileMetadata::checkSettings()
  {
    using boost::property_tree::ptree;
    const std::set<std::string> validSettings {
      "graph_based_aie_metrics", "graph_based_aie_memory_metrics",
      "graph_based_memory_tile_metrics", "graph_based_interface_tile_metrics",
      "tile_based_aie_metrics", "tile_based_aie_memory_metrics",
      "tile_based_memory_tile_metrics", "tile_based_interface_tile_metrics",
      "interval_us"};
    const std::map<std::string, std::string> deprecatedSettings {
      {"aie_profile_core_metrics", "AIE_profile_settings.graph_based_aie_metrics or tile_based_aie_metrics"},
      {"aie_profile_memory_metrics", "AIE_profile_settings.graph_based_aie_memory_metrics or tile_based_aie_memory_metrics"},
      {"aie_profile_interface_metrics", "AIE_profile_settings.tile_based_interface_tile_metrics"},
      {"aie_profile_interval_us", "AIE_profile_settings.interval_us"}};

    // Verify settings in AIE_profile_settings section
    auto tree1 = xrt_core::config::detail::get_ptree_value("AIE_profile_settings");

    for (ptree::iterator pos = tree1.begin(); pos != tree1.end(); pos++) {
      if (validSettings.find(pos->first) == validSettings.end()) {
        std::stringstream msg;
        msg << "The setting AIE_profile_settings." << pos->first << " is not recognized. "
            << "Please check the spelling and compare to supported list:";

        for (auto it = validSettings.cbegin(); it != validSettings.cend(); it++)
          msg << ((it == validSettings.cbegin()) ? " " : ", ") << *it;

        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      }
    }

    // Check for usage of deprecated settings
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

  std::vector<std::string> AieProfileMetadata::getSettingsVector(std::string settingsString)
  {
    if (settingsString.empty())
      return {};

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;

    boost::replace_all(settingsString, " ", "");

    boost::split(settingsVector, settingsString, boost::is_any_of(";"));

    return settingsVector;
  }

  inline void throw_if_error(bool err, const char* msg)
  {
    if (err)
      throw std::runtime_error(msg);
  }

  // Resolve metrics for AIE or MEM tiles
  void AieProfileMetadata::getConfigMetricsForTiles(int moduleIdx, const std::vector<std::string>& metricsSettings,
      const std::vector<std::string>& graphMetricsSettings, const module_type mod)
  {
    if ((metricsSettings.empty()) && (graphMetricsSettings.empty()))
      return;

    if ((metadataReader->getHardwareGeneration() == 1) && (mod == module_type::mem_tile)) {
      xrt_core::message::send(severity_level::warning, "XRT",
                              "MEM tiles are not available in AIE1. Profile "
                              "settings will be ignored.");
      return;
    }
    
    uint16_t rowOffset  = (mod == module_type::mem_tile) ? 1 : metadataReader->getAIETileRowOffset();
    std::string modName = (mod == module_type::core) ? "aie" 
                        : ((mod == module_type::dma) ? "aie_memory" : "memory_tile");

    auto allValidGraphs = metadataReader->getValidGraphs();
    auto allValidKernels = metadataReader->getValidKernels();

    std::set<tile_type> allValidTiles;
    auto validTilesVec = metadataReader->getTiles("all", mod, "all");
    std::unique_copy(validTilesVec.begin(), validTilesVec.end(), std::inserter(allValidTiles, allValidTiles.end()),
                     tileCompare);

    // STEP 1 : Parse per-graph or per-kernel settings

    /* AIE_profile_settings config format
     * Multiple values can be specified separated with ';'
     *
     * AI Engine Tiles
     * graph_based_aie_metrics = <graph name|all>:<kernel name|all>
     *   :<off|heat_map|stalls|execution|floating_point|write_throughputs|read_throughputs|aie_trace>
     * graph_based_aie_memory_metrics = <graph name|all>:<kernel name|all>
     *   :<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_throughputs|read_throughputs> MEM Tiles
     *
     * Memory tiles (AIE2 and beyond)
     * graph_based_memory_tile_metrics = <graph name|all>:<buffer name|all>
     *   :<off|input_channels|input_channels_details|output_channels|output_channels_details|memory_stats|mem_trace>[:<channel>]
     */

    std::vector<std::vector<std::string>> graphMetrics(graphMetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(graphMetrics[i], graphMetricsSettings[i], boost::is_any_of(":"));

      // Check if graph is not all or if invalid kernel
      if (graphMetrics[i][0].compare("all") != 0)
        continue;

      if ((graphMetrics[i][1].compare("all") != 0) &&
          (std::find(allValidKernels.begin(), allValidKernels.end(), graphMetrics[i][1]) == allValidKernels.end())) {
        std::stringstream msg;
        msg << "Could not find kernel " << graphMetrics[i][1]
            << " as specified in graph_based_" << modName << "_metrics setting."
            << " The following kernels are valid : " << allValidKernels[0];

        for (size_t j = 1; j < allValidKernels.size(); j++)
          msg << ", " << allValidKernels[j];

        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = metadataReader->getTiles(graphMetrics[i][0], mod, graphMetrics[i][1]);
      for (auto& e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; MEM tiles only)
      if (graphMetrics[i].size() == 5) {
        try {
          for (auto& e : tiles) {
            configChannel0[e] = static_cast<uint8_t>(std::stoul(graphMetrics[i][3]));
            configChannel1[e] = static_cast<uint8_t>(std::stoul(graphMetrics[i][4]));
          }
        }
        catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_" << modName << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Check if already processed or if invalid
      if (graphMetrics[i][0].compare("all") == 0)
        continue;

      if ((graphMetrics[i][1].compare("all") != 0) &&
          (std::find(allValidKernels.begin(), allValidKernels.end(), graphMetrics[i][1]) == allValidKernels.end())) {
        std::stringstream msg;
        msg << "Could not find kernel " << graphMetrics[i][1]
            << " as specified in graph_based_" << modName << "_metrics setting."
            << " The following kernels are valid : " << allValidKernels[0];

        for (size_t j = 1; j < allValidKernels.size(); j++)
          msg << ", " << allValidKernels[j];

        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      // Capture all tiles in given graph
      auto tiles = metadataReader->getTiles(graphMetrics[i][0], mod, graphMetrics[i][1]);
      for (auto& e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; MEM tiles only)
      if (graphMetrics[i].size() == 5) {
        try {
          for (auto& e : tiles) {
            configChannel0[e] = static_cast<uint8_t>(std::stoul(graphMetrics[i][3]));
            configChannel1[e] = static_cast<uint8_t>(std::stoul(graphMetrics[i][4]));
          }
        }
        catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in graph_based_" << modName << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Graph Pass 2

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format
     * Multiple values can be specified separated with ';'
     *
     * AI Engine Tiles
     * Single or all tiles
     * tile_based_aie_metrics = [[{<column>,<row>}|all>
     *     :<off|heat_map|stalls|execution|floating_point|write_throughputs|read_throughputs|aie_trace>]
     * tile_based_aie_memory_metrics = [[<{<column>,<row>}|all>
     *     :<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_throughputs|read_throughputs>]
     * Range of tiles
     * tile_based_aie_metrics = [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}
     *     :<off|heat_map|stalls|execution|floating_point|write_throughputs|read_throughputs|aie_trace>]]
     * tile_based_aie_memory_metrics = [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}
     *     :<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_throughputs|read_throughputs>]]
     *
     * Memory Tiles (AIE2 and beyond)
     * Single or all tiles
     * tile_based_memory_tile_metrics = [[<{<column>,<row>}|all>
     *     :<off|input_channels|input_channels_details|output_channels|output_channels_details|memory_stats|mem_trace>[:<channel>]]
     * Range of tiles
     * tile_based_memory_tile_metrics = [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}
     *     :<off|input_channels|input_channels_details|output_channels|output_channels_details|memory_stats|mem_trace>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if ((metrics[i][0].compare("all") != 0) || (metrics[i].size() < 2))
        continue;

      auto tiles = metadataReader->getTiles(metrics[i][0], mod, "all");
      for (auto& e : tiles) {
        configMetrics[moduleIdx][e] = metrics[i][1];
      }

      // Grab channel numbers (if specified; MEM tiles only)
      // One channel specified
      if (metrics[i].size() == 3) {
        try {
          for (auto& e : tiles) {
            configChannel0[e] = static_cast<uint8_t>(std::stoul(metrics[i][2]));
          }
        }
        catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << modName << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }

      // Both channel specified
      if (metrics[i].size() == 4) {
        try {
          for (auto& e : tiles) {
            configChannel0[e] = static_cast<uint8_t>(std::stoul(metrics[i][2]));
            configChannel1[e] = static_cast<uint8_t>(std::stoul(metrics[i][3]));
          }
        }
        catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << modName << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Pass 1

    // Pass 2 : process only range of tiles metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      if ((metrics[i].size() != 3) && (metrics[i].size() != 5))
        continue;

      uint16_t minRow = 0, minCol = 0;
      uint16_t maxRow = 0, maxCol = 0;

      try {
        for (size_t j = 0; j < metrics[i].size(); ++j) {
          boost::replace_all(metrics[i][j], "{", "");
          boost::replace_all(metrics[i][j], "}", "");
        }

        std::vector<std::string> minTile;
        boost::split(minTile, metrics[i][0], boost::is_any_of(","));
        minCol = static_cast<uint16_t>(std::stoul(minTile[0]));
        minRow = static_cast<uint16_t>(std::stoul(minTile[1])) + rowOffset;

        std::vector<std::string> maxTile;
        boost::split(maxTile, metrics[i][1], boost::is_any_of(","));
        maxCol = static_cast<uint16_t>(std::stoul(maxTile[0]));
        maxRow = static_cast<uint16_t>(std::stoul(maxTile[1])) + rowOffset;
      }
      catch (...) {
        xrt_core::message::send(severity_level::warning, "XRT",
                                "Tile range specification in tile_based_" + modName
                                + "_metrics is not valid format and hence skipped.");
        continue;
      }

      // Ensure range is valid
      if ((minCol > maxCol) || (minRow > maxRow)) {
        std::stringstream msg;
        msg << "Tile range specification in tile_based_" << modName
            << "_metrics is not valid format and hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      uint8_t channel0 = 0;
      uint8_t channel1 = 1;

      if (metrics[i].size() == 5) {
        try {
          channel0 = static_cast<uint8_t>(std::stoul(metrics[i][3]));
          channel1 = static_cast<uint8_t>(std::stoul(metrics[i][4]));
        }
        catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << modName << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }

      for (uint16_t col = minCol; col <= maxCol; ++col) {
        for (uint16_t row = minRow; row <= maxRow; ++row) {
          tile_type tile;
          tile.col = col;
          tile.row = row;

          // Make sure tile is used
          if (allValidTiles.find(tile) == allValidTiles.end()) {
            std::stringstream msg;
            msg << "Specified Tile {" << std::to_string(tile.col) << "," << std::to_string(tile.row)
                << "} is not active. Hence skipped.";
            xrt_core::message::send(severity_level::warning, "XRT", msg.str());
            continue;
          }

          configMetrics[moduleIdx][tile] = metrics[i][2];

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
      if ((metrics[i][0].compare("all") == 0) || (metrics[i].size() == 3) || (metrics[i].size() == 5))
        continue;

      uint16_t col = 0;
      uint16_t row = 0;

      try {
        boost::replace_all(metrics[i][0], "{", "");
        boost::replace_all(metrics[i][0], "}", "");

        std::vector<std::string> tilePos;
        boost::split(tilePos, metrics[i][0], boost::is_any_of(","));
        col = static_cast<uint16_t>(std::stoul(tilePos[0]));
        row = static_cast<uint16_t>(std::stoul(tilePos[1])) + rowOffset;
      }
      catch (...) {
        std::stringstream msg;
        msg << "Tile specification in tile_based_" << modName << "_metrics is not valid format and hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      tile_type tile;
      tile.col = col;
      tile.row = row;

      // Make sure tile is used
      if (allValidTiles.find(tile) == allValidTiles.end()) {
        std::stringstream msg;
        msg << "Specified Tile {" << std::to_string(tile.col) << "," << std::to_string(tile.row)
            << "} is not active. Hence skipped.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      configMetrics[moduleIdx][tile] = metrics[i][1];

      // Grab channel numbers (if specified; MEM tiles only)
      if (metrics[i].size() == 4) {
        try {
          configChannel0[tile] = static_cast<uint8_t>(std::stoul(metrics[i][2]));
          configChannel1[tile] = static_cast<uint8_t>(std::stoul(metrics[i][3]));
        }
        catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << modName << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Pass 3

    // Set default, check validity, and remove "off" tiles
    auto defaultSet = defaultSets[moduleIdx];
    bool showWarning = true;
    std::vector<tile_type> offTiles;

    for (auto& tileMetric : configMetrics[moduleIdx]) {
      // Save list of "off" tiles
      if (tileMetric.second.empty() || (tileMetric.second.compare("off") == 0)) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      if (std::find(metricStrings[mod].begin(), metricStrings[mod].end(), tileMetric.second) ==
          metricStrings[mod].end()) {
        if (showWarning) {
          std::stringstream msg;
          msg << "Unable to find " << moduleNames[moduleIdx] << " metric set " << tileMetric.second
              << ". Using default of " << defaultSet << ".";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
          showWarning = false;
        }

        tileMetric.second = defaultSet;
      }
    }

    // Remove all the "off" tiles
    for (auto& t : offTiles) {
      configMetrics[moduleIdx].erase(t);
    }
  }

  // Resolve Interface metrics
  void AieProfileMetadata::getConfigMetricsForInterfaceTiles(int moduleIdx,
      const std::vector<std::string>& metricsSettings,
      const std::vector<std::string> graphMetricsSettings)
  {
    if ((metricsSettings.empty()) && (graphMetricsSettings.empty()))
      return;

    auto allValidGraphs = metadataReader->getValidGraphs();
    auto allValidPorts = metadataReader->getValidPorts();

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
            << " as specified in graph_based_interface_tile_metrics setting."
            << " The following ports are valid : " << allValidPorts[0];

        for (size_t j = 1; j < allValidPorts.size(); j++)
          msg << ", " << allValidPorts[j];

        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = metadataReader->getInterfaceTiles(graphMetrics[i][0],
                                          graphMetrics[i][1],
                                          graphMetrics[i][2]);

      for (auto& e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto& e : tiles) {
            configChannel0[e] = static_cast<uint8_t>(std::stoi(graphMetrics[i][3]));
            configChannel1[e] = static_cast<uint8_t>(std::stoi(graphMetrics[i].back()));
          }
        }
        catch (...) {
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
          msg << ", " << allValidGraphs[j];

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
          msg << ", " << allValidPorts[j];

        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }

      auto tiles = metadataReader->getInterfaceTiles(graphMetrics[i][0],
                                          graphMetrics[i][1],
                                          graphMetrics[i][2]);

      for (auto& e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
      }

      // Grab channel numbers (if specified; memory tiles only)
      if (graphMetrics[i].size() > 3) {
        try {
          for (auto& e : tiles) {
            configChannel0[e] = static_cast<uint8_t>(std::stoi(graphMetrics[i][3]));
            configChannel1[e] = static_cast<uint8_t>(std::stoi(graphMetrics[i].back()));
          }
        }
        catch (...) {
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
     * [[<column|all>:<off|s2mm_throughputs|mm2s_throughputs|packets>[:<channel>]]
     * Range of tiles
     * tile_based_interface_tile_metrics =
     * [<mincolumn>:<maxcolumn>:<off|s2mm_throughputs|mm2s_throughputs|packets>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (metrics[i][0].compare("all") != 0)
        continue;

      uint8_t channelId = (metrics[i].size() < 3) ? 0 : static_cast<uint8_t>(std::stoul(metrics[i][2]));
      auto tiles = metadataReader->getInterfaceTiles("all", "all", metrics[i][1], channelId);

      for (auto& t : tiles) {
        configMetrics[moduleIdx][t] = metrics[i][1];
        configChannel0[t] = static_cast<uint8_t>(channelId);;
      }
    } // Pass 1

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

      uint8_t channelId = 0;

      if (metrics[i].size() == 4) {
        try {
          channelId = static_cast<uint8_t>(std::stoul(metrics[i][3]));
        }
        catch (std::invalid_argument const&) {
          // Expected channel Id is not an integer, give warning and ignore
          xrt_core::message::send(severity_level::warning, "XRT",
                                  "Channel ID specification in "
                                  "tile_based_interface_tile_metrics is "
                                  "not an integer and hence ignored.");
        }
      }

      auto tiles = metadataReader->getInterfaceTiles("all", "all", metrics[i][2], channelId, true, minCol, maxCol);

      for (auto& t : tiles) {
        configMetrics[moduleIdx][t] = metrics[i][2];
        configChannel0[t] = channelId;
      }
    } // Pass 2

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

        uint8_t channelId = 0;

        if (metrics[i].size() == 3) {
          try {
            channelId = static_cast<uint8_t>(std::stoul(metrics[i][2]));
          }
          catch (std::invalid_argument const&) {
            // Expected channel Id is not an integer, give warning and ignore
            xrt_core::message::send(severity_level::warning, "XRT",
                                    "Channel ID specification in "
                                    "tile_based_interface_tile_metrics is not an integer "
                                    "and hence ignored.");
          }
        }

        auto tiles = metadataReader->getInterfaceTiles("all", "all", metrics[i][1], channelId, true, col, col);

        for (auto& t : tiles) {
          configMetrics[moduleIdx][t] = metrics[i][1];
          configChannel0[t] = channelId;
        }
      }
    } // Pass 3

    // Set default, check validity, and remove "off" tiles
    auto defaultSet = defaultSets[moduleIdx];
    bool showWarning = true;
    std::vector<tile_type> offTiles;

    for (auto& tileMetric : configMetrics[moduleIdx]) {
      // Save list of "off" tiles
      if (tileMetric.second.empty() || (tileMetric.second.compare("off") == 0)) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      auto metricVec = metricStrings[module_type::shim];

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
      configMetrics[moduleIdx].erase(t);
    }
  }

  uint8_t AieProfileMetadata::getMetricSetIndex(std::string metricString, module_type mod)
  {
    auto stringVector = metricStrings[mod];
    auto itr = std::find(stringVector.begin(), stringVector.end(), metricString);

    if (itr != stringVector.cend()) {
      return 0;
    }
    else {
      return static_cast<uint8_t>(std::distance(stringVector.begin(), itr));
    }
  }

  aie::driver_config
  AieProfileMetadata::getAIEConfigMetadata()
  {
    return metadataReader->getDriverConfig();
  }

}  // namespace xdp
