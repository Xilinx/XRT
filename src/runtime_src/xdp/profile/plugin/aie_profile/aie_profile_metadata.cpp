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

#include "aie_profile_metadata.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;

  AieProfileMetadata::AieProfileMetadata(uint64_t deviceID, void* handle) : deviceID(deviceID), handle(handle)
  {
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
  }

  bool tileCompare(tile_type tile1, tile_type tile2)
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }

  void AieProfileMetadata::checkSettings()
  {
    using boost::property_tree::ptree;
    const std::set<std::string> validSettings {
        "graph_based_aie_metrics",         "graph_based_aie_memory_metrics",
        "graph_based_memory_tile_metrics", "graph_based_interface_tile_metrics",
        "tile_based_aie_metrics",          "tile_based_aie_memory_metrics",     
        "tile_based_memory_tile_metrics",  "tile_based_interface_tile_metrics", 
        "interval_us"};
    const std::map<std::string, std::string> deprecatedSettings {
        {"aie_profile_core_metrics",      "AIE_profile_settings.graph_based_aie_metrics or tile_based_aie_metrics"},
        {"aie_profile_memory_metrics",    "AIE_profile_settings.graph_based_aie_memory_metrics or tile_based_aie_memory_metrics"},
        {"aie_profile_interface_metrics", "AIE_profile_settings.tile_based_interface_tile_metrics"},
        {"aie_profile_interval_us",       "AIE_profile_settings.interval_us"}};

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
        msg << "The setting Debug." << pos->first << " is deprecated. "
            << "Please instead use " << iter->second << ".";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      }
    }
  }

  int AieProfileMetadata::getHardwareGen()
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

  uint16_t AieProfileMetadata::getAIETileRowOffset()
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

  std::vector<std::string> AieProfileMetadata::getSettingsVector(std::string settingsString)
  {
    if (settingsString.empty())
      return {};

    // For 2023.1 only: support both *_bandwidths and *_throughputs
    if (settingsString.find("bandwidths") != std::string::npos) {
      xrt_core::message::send(severity_level::warning, "XRT",
                              "All metric sets named *_bandwidths will be renamed *_throughputs "
                              "in 2023.2. Please use the new settings.");
      boost::replace_all(settingsString, "bandwidths", "throughputs");
    }

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;
    boost::replace_all(settingsString, " ", "");
    boost::split(settingsVector, settingsString, boost::is_any_of(";"));
    return settingsVector;
  }

  // Find all MEM tiles associated with a graph and kernel
  //   kernel_name = all      : all tiles in graph
  //   kernel_name = <kernel> : only tiles used by that specific kernel
  std::vector<tile_type> AieProfileMetadata::get_mem_tiles(const xrt_core::device* device,
                                                           const std::string& graph_name,
                                                           const std::string& kernel_name)
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
    for (auto const& shared_buffer : sharedBufferTree.get()) {
      auto currGraph = shared_buffer.second.get<std::string>("graph");
      if ((currGraph.find(graph_name) == std::string::npos) && (graph_name.compare("all") != 0))
        continue;
      if (kernel_name.compare("all") != 0) {
        std::vector<std::string> names;
        std::string functionStr = shared_buffer.second.get<std::string>("function");
        boost::split(names, functionStr, boost::is_any_of("."));
        if (std::find(names.begin(), names.end(), kernel_name) == names.end())
          continue;
      }

      tile_type tile;
      tile.col = shared_buffer.second.get<uint16_t>("column");
      tile.row = shared_buffer.second.get<uint16_t>("row") + rowOffset;
      allTiles.emplace_back(std::move(tile));
    }

    std::unique_copy(allTiles.begin(), allTiles.end(), std::back_inserter(memTiles), tileCompare);
    return memTiles;
  }

  inline void throw_if_error(bool err, const char* msg)
  {
    if (err)
      throw std::runtime_error(msg);
  }

  std::vector<tile_type> AieProfileMetadata::get_event_tiles(const xrt_core::device* device,
                                                             const std::string& graph_name, module_type type)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    // Interface tiles use different method
    if (type == module_type::shim)
      return {};

    const char* col_name = (type == module_type::core) ? "core_columns" : "dma_columns";
    const char* row_name = (type == module_type::core) ? "core_rows" : "dma_rows";

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();
    int startCount = 0;

    for (auto& graph : aie_meta.get_child("aie_metadata.EventGraphs")) {
      auto currGraph = graph.second.get<std::string>("name");
      if ((currGraph.find(graph_name) == std::string::npos) && (graph_name.compare("all") != 0))
        continue;

      int count = startCount;
      for (auto& node : graph.second.get_child(col_name)) {
        tiles.push_back(tile_type());
        auto& t = tiles.at(count++);
        t.col = static_cast<uint16_t>(std::stoul(node.second.data()));
      }

      int num_tiles = count;
      count = startCount;
      for (auto& node : graph.second.get_child(row_name))
        tiles.at(count++).row = static_cast<uint16_t>(std::stoul(node.second.data())) + rowOffset;
      throw_if_error(count < num_tiles, "rows < num_tiles");
      startCount = count;
    }

    return tiles;
  }

  // Find all AIE tiles associated with a graph and module type (kernel_name = all)
  std::vector<tile_type> AieProfileMetadata::get_aie_tiles(const xrt_core::device* device,
                                                           const std::string& graph_name, module_type type)
  {
    std::vector<tile_type> tiles;
    tiles = get_event_tiles(device, graph_name, module_type::core);
    if (type == module_type::dma) {
      auto dmaTiles = get_event_tiles(device, graph_name, module_type::dma);
      std::unique_copy(dmaTiles.begin(), dmaTiles.end(), back_inserter(tiles), tileCompare);
    }
    return tiles;
  }

  std::vector<tile_type> AieProfileMetadata::get_tiles(const xrt_core::device* device, const std::string& graph_name,
                                                       module_type type, const std::string& kernel_name)
  {
    if (type == module_type::mem_tile)
      return get_mem_tiles(device, graph_name, kernel_name);
    if (kernel_name.compare("all") == 0)
      return get_aie_tiles(device, graph_name, type);

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

    for (auto const& mapping : kernelToTileMapping.get()) {
      auto currGraph = mapping.second.get<std::string>("graph");
      if ((currGraph.find(graph_name) == std::string::npos) && (graph_name.compare("all") != 0))
        continue;
      if (kernel_name.compare("all") != 0) {
        std::vector<std::string> names;
        std::string functionStr = mapping.second.get<std::string>("function");
        boost::split(names, functionStr, boost::is_any_of("."));
        if (std::find(names.begin(), names.end(), kernel_name) == names.end())
          continue;
      }

      tile_type tile;
      tile.col = mapping.second.get<uint16_t>("column");
      tile.row = mapping.second.get<uint16_t>("row") + rowOffset;
      tiles.emplace_back(std::move(tile));
    }
    return tiles;
  }

  // Resolve metrics for AIE or MEM tiles
  void AieProfileMetadata::getConfigMetricsForTiles(int moduleIdx, const std::vector<std::string>& metricsSettings,
                                                    const std::vector<std::string>& graphMetricsSettings,
                                                    const module_type mod)
  {
    if ((metricsSettings.empty()) && (graphMetricsSettings.empty()))
      return;
    if ((getHardwareGen() == 1) && (mod == module_type::mem_tile)) {
      xrt_core::message::send(severity_level::warning, "XRT",
                              "MEM tiles are not available in AIE1. Profile "
                              "settings will be ignored.");
      return;
    }

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto data = device->get_axlf_section(AIE_METADATA);
    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    
    uint16_t rowOffset = (mod == module_type::mem_tile) ? 1 : getAIETileRowOffset();
    auto modName = (mod == module_type::core) ? "aie" : ((mod == module_type::dma) ? "aie_memory" : "memory_tile");

    auto allValidGraphs = aie::getValidGraphs(aie_meta);
    auto allValidKernels = aie::getValidKernels(aie_meta);

    std::set<tile_type> allValidTiles;
    auto validTilesVec = get_tiles(device.get(), "all", mod);
    std::unique_copy(validTilesVec.begin(), validTilesVec.end(), std::inserter(allValidTiles, allValidTiles.end()),
                     tileCompare);

    // STEP 1 : Parse per-graph or per-kernel settings

    /* AIE_profile_settings config format ; Multiple values can be specified for
     * a metric separated with ';' AI Engine Tiles graph_based_aie_metrics = <graph name|all>:<kernel
     * name|all>:<off|heat_map|stalls|execution|floating_point|write_throughputs|read_throughputs|aie_trace>
     * graph_based_aie_memory_metrics = <graph name|all>:<kernel
     * name|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_throughputs|read_throughputs> MEM Tiles
     * graph_based_memory_tile_metrics = <graph name|all>:<kernel
     * name|all>:<off|input_channels|output_channels|memory_stats>[:<channel>]
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

      auto tiles = get_tiles(device.get(), graphMetrics[i][0], mod, graphMetrics[i][1]);
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
    }  // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Check if already processed or if invalid
      if (graphMetrics[i][0].compare("all") == 0)
        continue;
      if (std::find(allValidGraphs.begin(), allValidGraphs.end(), graphMetrics[i][0]) == allValidGraphs.end()) {
        std::stringstream msg;
        msg << "Could not find graph " << graphMetrics[i][0] 
            << " as specified in graph_based_" << modName << "_metrics setting."
            << " The following graphs are valid : " << allValidGraphs[0];
        for (size_t j = 1; j < allValidGraphs.size(); j++)
          msg << ", " << allValidGraphs[j];
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        continue;
      }
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
      auto tiles = get_tiles(device.get(), graphMetrics[i][0], mod, graphMetrics[i][1]);
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
    }  // Graph Pass 2

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for
     * a metric separated with ';' AI Engine Tiles Single or all tiles
     * tile_based_aie_metrics =
     * [[{<column>,<row>}|all>:<off|heat_map|stalls|execution|floating_point|write_throughputs|read_throughputs|aie_trace>]
     * tile_based_aie_memory_metrics =
     * [[<{<column>,<row>}|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_throughputs|read_throughputs>]
     * Range of tiles
     * tile_based_aie_metrics =
     * [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|heat_map|stalls|execution|floating_point|write_throughputs|read_throughputs|aie_trace>]]
     * tile_based_aie_memory_metrics =
     * [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_throughputs|read_throughputs>]]
     *
     * MEM Tiles (AIE2 and beyond)
     * Single or all tiles
     * tile_based_memory_tile_metrics =
     * [[<{<column>,<row>}|all>:<off|input_channels|input_channels_details|output_channels|output_channels_details|memory_stats>[:<channel>]]
     * Range of tiles
     * tile_based_memory_tile_metrics =
     * [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|input_channels|output_channels|memory_stats>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if ((metrics[i][0].compare("all") != 0) || (metrics[i].size() < 2))
        continue;

      auto tiles = get_tiles(device.get(), metrics[i][0], mod);
      for (auto& e : tiles) {
        configMetrics[moduleIdx][e] = metrics[i][1];
      }

      // Grab channel numbers (if specified; MEM tiles only)
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
    }  // Pass 1

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
                                "Tile range specification in tile_based_aie_[memory}_metrics "
                                "is not of valid format and hence skipped.");
        continue;
      }

      // Ensure range is valid
      if ((minCol > maxCol) || (minRow > maxRow)) {
        std::stringstream msg;
        msg << "Tile range specification in tile_based_" << modName
            << "_metrics is not of valid format and hence skipped.";
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
    }  // Pass 2

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
    }  // Pass 3

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

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    auto data = device->get_axlf_section(AIE_METADATA);
    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);

    auto allValidGraphs = aie::getValidGraphs(aie_meta);
    auto allValidPorts = aie::getValidPorts(aie_meta);

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

      auto tiles = aie::getInterfaceTiles(aie_meta, graphMetrics[i][0], graphMetrics[i][1], 
                                          graphMetrics[i][2]);
      for (auto &e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
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

      auto tiles = aie::getInterfaceTiles(aie_meta, graphMetrics[i][0], graphMetrics[i][1],
                                          graphMetrics[i][2]);
      for (auto &e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
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
     * [[<column|all>:<off|input_throughputs|output_throughputs|packets>[:<channel>]]
     * Range of tiles
     * tile_based_interface_tile_metrics =
     * [<mincolumn>:<maxcolumn>:<off|input_throughputs|output_throughputs|packets>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (metrics[i][0].compare("all") != 0)
        continue;

      int16_t channelId = (metrics[i].size() < 3) ? -1 : static_cast<uint16_t>(std::stoul(metrics[i][2]));
      auto tiles = aie::getInterfaceTiles(aie_meta, "all", "all", metrics[i][1], channelId);

      for (auto& e : tiles) {
        configMetrics[moduleIdx][e] = metrics[i][1];
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
          // Expected channel Id is not an integer, give warning and ignore channelId
          xrt_core::message::send(severity_level::warning, "XRT",
                                  "Channel ID specification in "
                                  "tile_based_interface_tile_metrics is "
                                  "not an integer and hence ignored.");
          channelId = -1;
        }
      }

      auto tiles = aie::getInterfaceTiles(aie_meta, "all", "all", metrics[i][2],
                                          channelId, true, minCol, maxCol);

      for (auto& t : tiles) {
        configMetrics[moduleIdx][t] = metrics[i][2];
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
            // Expected channel Id is not an integer, give warning and
            // ignore channelId
            xrt_core::message::send(severity_level::warning, "XRT",
                                    "Channel ID specification in "
                                    "tile_based_interface_tile_metrics is not an integer "
                                    "and hence ignored.");
            channelId = -1;
          }
        }

        auto tiles = aie::getInterfaceTiles(aie_meta, "all", "all", metrics[i][1], 
                                            channelId, true, col, col);

        for (auto& t : tiles) {
          configMetrics[moduleIdx][t] = metrics[i][1];
        }
      }
    }  // Pass 3

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

  void AieProfileMetadata::read_aie_metadata(const char* data, size_t size, pt::ptree& aie_project)
  {
    std::stringstream aie_stream;
    aie_stream.write(data, size);
    pt::read_json(aie_stream, aie_project);
  }

  uint8_t AieProfileMetadata::getMetricSetIndex(std::string metricString, module_type mod)
  {
    auto stringVector = metricStrings[mod];

    auto itr = std::find(stringVector.begin(), stringVector.end(), metricString);
    if (itr != stringVector.cend()) {
      return 0;
    } else {
      return static_cast<uint8_t>(std::distance(stringVector.begin(), itr));
    }
  }
}  // namespace xdp
