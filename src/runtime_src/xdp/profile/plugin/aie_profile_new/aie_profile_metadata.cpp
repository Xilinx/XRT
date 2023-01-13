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

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "aie_profile_metadata.h"
#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;

  AieProfileMetadata::AieProfileMetadata(uint64_t deviceID, void* handle)
    : deviceID(deviceID), handle(handle)
  {
    configMetrics.resize(NUM_MODULES);
    // Get polling interval (in usec)
    pollingInterval = xrt_core::config::get_aie_profile_settings_interval_us();

    // Setup Config Metrics
    // Get AIE clock frequency
    VPDatabase* db = VPDatabase::Instance();
    clockFreqMhz = (db->getStaticInfo()).getClockRateMHz(deviceID,false);

    // Tile-based metrics settings
    std::vector<std::string> metricsConfig;
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_aie_memory_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_interface_tile_metrics());
    metricsConfig.push_back(xrt_core::config::get_aie_profile_settings_tile_based_mem_tile_metrics());

    // Graph-based metrics settings
    std::vector<std::string> graphMetricsConfig;
    graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_metrics());
    graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_aie_memory_metrics());
    // Uncomment to support graph-based metrics for Interface Tiles
    //graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_interface_tile_metrics());
    graphMetricsConfig.push_back("");
    graphMetricsConfig.push_back(xrt_core::config::get_aie_profile_settings_graph_based_mem_tile_metrics());

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

  std::vector<std::string>
  AieProfileMetadata::getSettingsVector(std::string settingsString) 
  {
    if (settingsString.empty())
      return {};

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;
    boost::replace_all(settingsString, " ", "");
    boost::split(settingsVector, settingsString, boost::is_any_of(";"));
    return settingsVector;
  }

  std::vector<tile_type>
  AieProfileMetadata::get_interface_tiles(const xrt_core::device* device, const std::string &metricStr,
                                          int16_t channelId, bool useColumn, uint32_t minCol, 
                                          uint32_t maxCol)
  {
    std::vector<tile_type> tiles;

    int plioCount = 0;
    auto plios = get_plios(device);
    for (auto& plio : plios) {
      auto isMaster = plio.second.slaveOrMaster;
      auto streamId = plio.second.streamId;
      auto shimCol  = plio.second.shimColumn;

      // If looking for specific ID, make sure it matches
      if ((channelId >= 0) && (channelId != streamId))
        continue;

      // Make sure it's desired polarity
      // NOTE: input = slave (data flowing from PLIO)
      //       output = master (data flowing to PLIO)
      if ((isMaster && (metricStr == "input_bandwidths"))
          || (!isMaster && (metricStr == "output_bandwidths")))
        continue;

      plioCount++;

      if (useColumn 
          && !( (minCol <= (uint32_t)shimCol) && ((uint32_t)shimCol <= maxCol) )) {
        // shimCol is not within minCol:maxCol range. So skip.
        continue;
      }

      tile_type tile = {0};
      tile.col = shimCol;
      tile.row = 0;
      // Grab stream ID and slave/master (used in configStreamSwitchPorts())
      tile.itr_mem_col = isMaster;
      tile.itr_mem_row = streamId;
      tiles.push_back(tile);
    }
          
    if ((plioCount == 0) && (channelId >= 0)) {
      std::string msg = "No tiles used channel ID " + std::to_string(channelId)
                        + ". Please specify a valid channel ID.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }
    return tiles;
  }

  std::vector<tile_type> 
  AieProfileMetadata::get_mem_tiles(const xrt_core::device* device, const std::string& graph_name)
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
  AieProfileMetadata::get_aie_tiles(const xrt_core::device* device,
                                    const std::string& graph_name, module_type type)
  {
    std::vector<tile_type> tiles;
    tiles = get_event_tiles(device, graph_name, module_type::core);
    if (type == module_type::dma) {
      auto dmaTiles = get_event_tiles(device, graph_name, module_type::dma);
      std::move(dmaTiles.begin(), dmaTiles.end(), back_inserter(tiles));
    }
    return tiles;
  }

  std::vector<tile_type> 
  AieProfileMetadata::get_tiles(const xrt_core::device* device, const std::string& graph_name,
                                module_type type, const std::string& kernel_name)
  {
    if (kernel_name.empty() || (kernel_name.compare("all") == 0)) {
      if (type == module_type::mem_tile)
        return get_mem_tiles(device, graph_name);
      return get_aie_tiles(device, graph_name, type);
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
      boost::split(names, mapping.second.get<std::string>("function"), boost::is_any_of("."));
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

  // Resolve metrics for AIE or MEM tiles
  void
  AieProfileMetadata::getConfigMetricsForTiles(int moduleIdx,
                                               const std::vector<std::string>& metricsSettings,
                                               const std::vector<std::string>& graphMetricsSettings,
                                               const module_type mod)
  {
    if ((metricsSettings.empty()) && (graphMetricsSettings.empty())) 
      return;
    if ((getHardwareGen() == 1) && (mod == module_type::mem_tile)) {
      xrt_core::message::send(severity_level::warning, "XRT",
        "MEM tiles are not available in AIE1. Profile settings will be ignored.");
      return;
    }

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);
    uint16_t rowOffset = (mod == module_type::mem_tile) ? 1 : getAIETileRowOffset();
    auto modName = (mod == module_type::core) ? "aie" 
                 : ((mod == module_type::dma) ? "aie_memory"
                 : "mem_tile");

    // STEP 1 : Parse per-graph or per-kernel settings

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * AI Engine Tiles
     * graph_based_aie_metrics = <graph name|all>:<kernel name|all>:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>
     * graph_based_aie_memory_metrics = <graph name|all>:<kernel name|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths>
     * MEM Tiles
     * graph_based_mem_tile_metrics = <graph name|all>:<kernel name|all>:<off|input_channels|output_channels|memory_stats>[:<channel>]
     */

    std::vector<std::vector<std::string>> graphMetrics(graphMetricsSettings.size());

    std::set<tile_type> allValidTiles;
    auto graphs = get_graphs(device.get());
    for (auto& graph : graphs) {
      std::vector<tile_type> currTiles = get_tiles(device.get(), graph, mod);
      std::copy(currTiles.begin(), currTiles.end(), std::inserter(allValidTiles, allValidTiles.end()));
    }

    // Graph Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(graphMetrics[i], graphMetricsSettings[i], boost::is_any_of(":"));

      if (graphMetrics[i][0].compare("all") != 0)
        continue;

      auto tiles = get_tiles(device.get(), graphMetrics[i][0], mod, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
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
          msg << "Channel specifications in graph_based_" << modName 
              << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    }  // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting 
    for (size_t i = 0; i < graphMetricsSettings.size(); ++i) {
      // Check if already processed
      if (graphMetrics[i][0].compare("all") == 0)
        continue;

      // Capture all tiles in given graph
      auto tiles = get_tiles(device.get(), graphMetrics[i][0], mod, graphMetrics[i][1]);
      for (auto &e : tiles) {
        configMetrics[moduleIdx][e] = graphMetrics[i][2];
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
          msg << "Channel specifications in graph_based_" << modName
              << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    }  // Graph Pass 2

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * AI Engine Tiles
     * Single or all tiles
     * tile_based_aie_metrics = [[{<column>,<row>}|all>:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>]
     * tile_based_aie_memory_metrics = [[<{<column>,<row>}|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths>]
     * Range of tiles
     * tile_based_aie_metrics = [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>]]
     * tile_based_aie_memory_metrics = [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths>]]
     * 
     * MEM Tiles (AIE2 and beyond)
     * Single or all tiles
     * tile_based_mem_tile_metrics = [[<{<column>,<row>}|all>:<off|input_channels|input_channels_details|output_channels|output_channels_details|memory_stats>[:<channel>]]
     * Range of tiles
     * tile_based_mem_tile_metrics = [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|input_channels|output_channels|memory_stats>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if ((metrics[i][0].compare("all") != 0) || (metrics[i].size() < 2))
        continue;

      auto tiles = get_tiles(device.get(), metrics[i][0], mod);
      for (auto &e : tiles) {
        configMetrics[moduleIdx][e] = metrics[i][1];
      }

      // Grab channel numbers (if specified; MEM tiles only)
      if (metrics[i].size() == 4) {
        try {
          for (auto &e : tiles) {
            configChannel0[e] = std::stoi(metrics[i][2]);
            configChannel1[e] = std::stoi(metrics[i][3]);
          }
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << modName
              << "_metrics are not valid and hence ignored.";
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
        minCol = std::stoi(minTile[0]);
        minRow = std::stoi(minTile[1]) + rowOffset;

        std::vector<std::string> maxTile;
        boost::split(maxTile, metrics[i][1], boost::is_any_of(","));
        maxCol = std::stoi(maxTile[0]);
        maxRow = std::stoi(maxTile[1]) + rowOffset;
      } catch (...) {
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Tile range specification in tile_based_aie_[memory}_metrics is not of valid format and hence skipped.");
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
          channel0 = std::stoi(metrics[i][3]);
          channel1 = std::stoi(metrics[i][4]);
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << modName
              << "_metrics are not valid and hence ignored.";
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
            msg << "Specified Tile {" << std::to_string(tile.col) << ","
                << std::to_string(tile.row) << "} is not active. Hence skipped.";
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
        msg << "Tile specification in tile_based_" << modName
            << "_metrics is not valid format and hence skipped.";
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

      configMetrics[moduleIdx][tile] = metrics[i][2];
      
      // Grab channel numbers (if specified; MEM tiles only)
      if (metrics[i].size() == 4) {
        try {
          configChannel0[tile] = std::stoi(metrics[i][2]);
          configChannel1[tile] = std::stoi(metrics[i][3]);
        } catch (...) {
          std::stringstream msg;
          msg << "Channel specifications in tile_based_" << modName
              << "_metrics are not valid and hence ignored.";
          xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        }
      }
    } // Pass 3 

    // Check validity, set default and remove "off" tiles
    for (auto &e : allValidTiles) {
      if (configMetrics[moduleIdx].find(e) == configMetrics[moduleIdx].end())
        configMetrics[moduleIdx][e] = defaultSets[moduleIdx];
    }

    std::vector<tile_type> offTiles;

    for (auto &tileMetric : configMetrics[moduleIdx]) {
      // Save list of "off" tiles
      if (tileMetric.second.empty() || (tileMetric.second.compare("off") == 0)) {
        offTiles.push_back(tileMetric.first);
        continue;
      }
       
      // Ensure requested metric set is supported (if not, use default)
      if (std::find(metricStrings[mod].begin(), metricStrings[mod].end(), tileMetric.second) == metricStrings[mod].end()) {
        std::stringstream msg;
        msg << "Unable to find " << moduleNames[moduleIdx] << " metric set " << tileMetric.second
            << ". Using default of " << defaultSets[moduleIdx] << "."
            << " As new AIE_profile_settings section is given, old style metric configurations, if any, are ignored.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        tileMetric.second = defaultSets[moduleIdx];
      } 
    }

    // Remove all the "off" tiles
    for (auto &t : offTiles) {
      configMetrics[moduleIdx].erase(t);
    }
  }

  // Resolve Interface metrics 
  void
  AieProfileMetadata::getConfigMetricsForInterfaceTiles(int moduleIdx,
                                                        const std::vector<std::string>& metricsSettings,
                                                        const std::vector<std::string> graphMetricsSettings)
  {
    if ((metricsSettings.empty()) && (graphMetricsSettings.empty())) 
      return;

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    // TODO: Add support for graph metrics
    
    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * Single or all tiles
     * tile_based_interface_tile_metrics = [[<column|all>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]]
     * Range of tiles
     * tile_based_interface_tile_metrics = [<mincolumn>:<maxcolumn>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (metrics[i][0].compare("all") != 0)
       continue;

      int16_t channelId = (metrics[i].size() < 3) ? -1 : std::stoi(metrics[i][2]);
      auto tiles = get_interface_tiles(device.get(), metrics[i][1], channelId);

      for (auto &e : tiles) {
        configMetrics[moduleIdx][e] = metrics[i][1];
      }
    } // Pass 1 

    // Pass 2 : process only range of tiles metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      if ((metrics[i][0].compare("all") == 0) || (metrics[i].size() < 3))
        continue;
     
      uint32_t maxCol = 0;
      try {
        maxCol = std::stoi(metrics[i][1]);
      } catch (std::invalid_argument const &e) {
        // maxColumn is not an integer i.e either 1st style or wrong format, skip for now
        continue;
      }
      uint32_t minCol = 0;
      try {
        minCol = std::stoi(metrics[i][0]);
      } catch (std::invalid_argument const &e) {
        // 2nd style but expected min column is not an integer, give warning and skip 
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Minimum column specification in tile_based_interface_tile_metrics is not an integer and hence skipped.");
        continue;
      }

      int16_t channelId = 0;
      if (metrics[i].size() == 4) {
        try {
          channelId = std::stoi(metrics[i][3]);
        } catch (std::invalid_argument const &e) {
          // Expected channel Id is not an integer, give warning and ignore channelId
          xrt_core::message::send(severity_level::warning, "XRT", 
             "Channel ID specification in tile_based_interface_tile_metrics is not an integer and hence ignored.");
          channelId = -1;
        }
      }
      
      auto tiles = get_interface_tiles(device.get(), metrics[i][2], channelId,
                                       true, minCol, maxCol);

      for (auto &t : tiles) {
        configMetrics[moduleIdx][t] = metrics[i][2];
      }
    } // Pass 2 

    // Pass 3 : process only single tile metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // Skip range specification, invalid format, or already processed
      if ((metrics[i].size() == 4) || (metrics[i].size() < 2)
          || (metrics[i][0].compare("all") == 0))
        continue;
      
      uint32_t col = 0;
      try {
        col = std::stoi(metrics[i][1]);
      } catch (std::invalid_argument const &e) {
        // max column is not a number, so the expected single column specification. Handle this here
        try {
          col = std::stoi(metrics[i][0]);
        } catch (std::invalid_argument const &e) {
          // Expected column specification is not a number. Give warning and skip
          xrt_core::message::send(severity_level::warning, "XRT", 
             "Column specification in tile_based_interface_tile_metrics is not an integer and hence skipped.");
          continue;
        }

        int16_t channelId = -1;
        if (metrics[i].size() == 3) {
          try {
            channelId = std::stoi(metrics[i][2]);
          } catch (std::invalid_argument const &e) {
            // Expected channel Id is not an integer, give warning and ignore channelId
            xrt_core::message::send(severity_level::warning, "XRT", 
               "Channel ID specification in tile_based_interface_tile_metrics is not an integer and hence ignored.");
            channelId = -1;
          }
        }

        auto tiles = get_interface_tiles(device.get(), metrics[i][1], channelId,
                                         true, col, col);

        for (auto &t : tiles) {
          configMetrics[moduleIdx][t] = metrics[i][1];
        }
      }
    } // Pass 3 

    // check validity, set default and remove "off" tiles
    std::vector<tile_type> offTiles;
    
    // Default any unspecified to the default metric sets
    auto defaultSet = defaultSets[moduleIdx];
    auto totalTiles = get_interface_tiles(device.get(), defaultSet, -1);
    for (auto &e : totalTiles) {
      if (configMetrics[moduleIdx].find(e) == configMetrics[moduleIdx].end()) {
        configMetrics[moduleIdx][e] = defaultSet;
      }
    }

    for (auto &tileMetric : configMetrics[moduleIdx]) {
      // Save list of "off" tiles
      if (tileMetric.second.empty() || (tileMetric.second.compare("off") == 0)) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      auto metricVec = metricStrings[module_type::shim];
      if (std::find(metricVec.begin(), metricVec.end(), tileMetric.second) == metricVec.end()) {
        std::string msg = "Unable to find interface_tile metric set " + tileMetric.second
                          + ". Using default of " + defaultSet + ". ";
        xrt_core::message::send(severity_level::warning, "XRT", msg);
        tileMetric.second = defaultSet;
      }
    }

    // Remove all the "off" tiles
    for (auto &t : offTiles) {
      configMetrics[moduleIdx].erase(t);
    }
  }

  void AieProfileMetadata::read_aie_metadata(const char* data, size_t size, pt::ptree& aie_project)
  {
    std::stringstream aie_stream;
    aie_stream.write(data,size);
    pt::read_json(aie_stream,aie_project);
  }

  std::vector<std::string> AieProfileMetadata::get_graphs(const xrt_core::device* device)
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

  std::unordered_map<std::string, plio_config> 
  AieProfileMetadata::get_plios(const xrt_core::device* device)
  {
    auto data = device->get_axlf_section(AIE_METADATA);
    if (!data.first || !data.second)
      return {};

    pt::ptree aie_meta;
    read_aie_metadata(data.first, data.second, aie_meta);
    std::unordered_map<std::string, plio_config> plios;

    for (auto& plio_node : aie_meta.get_child("aie_metadata.PLIOs")) {
      plio_config plio;

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
 
  inline void throw_if_error(bool err, const char* msg)
  {
    if (err)
      throw std::runtime_error(msg);
  }

  std::vector<tile_type>
  AieProfileMetadata::get_event_tiles(const xrt_core::device* device, const std::string& graph_name, module_type type)
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
    const char* row_name = (type == module_type::core) ?    "core_rows" :    "dma_rows";

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset();

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
        tiles.at(count++).row = std::stoul(node.second.data()) + rowOffset;
      throw_if_error(count < num_tiles, "rows < num_tiles");
    }

    return tiles;
  }

  uint8_t AieProfileMetadata::getMetricSetIndex(std::string metricString, module_type mod){
    auto stringVector = metricStrings[mod];
    
    auto itr = std::find(stringVector.begin(), stringVector.end(), metricString);
    if (itr != stringVector.cend()){
      return 0;
    } else {
      return std::distance(stringVector.begin(), itr);
    }

  }
}
