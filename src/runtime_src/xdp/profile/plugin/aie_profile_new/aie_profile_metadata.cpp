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
#define XDP_SOURCE

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "core/common/xrt_profiling.h"
#include "core/common/device.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "aie_profile_metadata.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;
    AieProfileMetadata::AieProfileMetadata(uint64_t deviceID, void* handle)
  : deviceID(deviceID)
  , handle(handle)
  {
    mConfigMetrics.resize(3);
  // Get polling interval (in usec; minimum is 100)
    mPollingInterval = xrt_core::config::get_aie_profile_settings_interval_us();
    if (1000 == mPollingInterval) {
      // If set to default value, then check for old style config 
      mPollingInterval = xrt_core::config::get_aie_profile_interval_us();
      if (1000 != mPollingInterval) {
        xrt_core::message::send(severity_level::warning, "XRT", 
          "The xrt.ini flag \"aie_profile_interval_us\" is deprecated and will be removed in future release. Please use \"interval_us\" under \"AIE_profile_settings\" section.");
      }
    }

  }

  std::vector<tile_type>
  AieProfileMetadata::getAllTilesForCoreMemoryProfiling(const module_type mod,
                                                        const std::string &graph,
                                                        void* handle)
  {
    std::vector<tile_type> tiles;
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    tiles = get_event_tiles(device.get(), graph,
                                 module_type::core);
    if (mod == module_type::dma) {
      auto dmaTiles = get_event_tiles(device.get(), graph,
          module_type::dma);
      std::move(dmaTiles.begin(), dmaTiles.end(), back_inserter(tiles));
    }
    return tiles;
  }

  std::vector<tile_type>
  AieProfileMetadata::getAllTilesForInterfaceProfiling(void* handle, 
                        const std::string &metricsStr, 
                        int16_t channelId,
                        bool useColumn, uint32_t minCol, uint32_t maxCol)
  {
    std::vector<tile_type> tiles;

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    int plioCount = 0;
    auto plios = get_plios(device.get());
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
      if ((isMaster && (metricsStr == "input_bandwidths"))
          || (!isMaster && (metricsStr == "output_bandwidths")))
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
          
    if ((0 == plioCount) && (0 <= channelId)) {
      std::string msg = "No tiles used channel ID " + std::to_string(channelId)
                        + ". Please specify a valid channel ID.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }
    return tiles;
  }

  // Resolve Processor and Memory metrics on all tiles
  // Mem tile metrics is not supported now.
  void
  AieProfileMetadata::getConfigMetricsForTiles(int moduleIdx,
                                               const std::vector<std::string>& metricsSettings,
                                               const std::vector<std::string>& graphmetricsSettings,
                                               const module_type mod,
                                               void* handle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

    bool allGraphsDone = false;

    // STEP 1 : Parse per-graph or per-kernel settings

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * "graphmetricsSettings" contains each metric value
     * graph_based_aie_metrics = <graph name|all>:<kernel name|all>:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>
     * graph_based_aie_memory_metrics = <graph name|all>:<kernel name|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths>
     * graph_based_mem_tile_metrics = <graph name|all>:<kernel name|all>:<off|input_channels|output_channels|memory_stats>[:<channel>]
     */

    std::vector<std::vector<std::string>> graphmetrics(graphmetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < graphmetricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(graphmetrics[i], graphmetricsSettings[i], boost::is_any_of(":"));
      if (3 != graphmetrics[i].size()) {
        /* Note : only graph_mem_tile_metrics can have more than 3 items in a metric value.
         * But it is not supported now.
         */
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Expected three \":\" separated fields for graph_based_aie_[memory_]metrics not found. Hence ignored.");
        continue;
      }

      std::vector<tile_type> tiles;
      /*
       * Core profiling uses all unique core tiles in aie control
       * Memory profiling uses all unique core + dma tiles in aie control
       */
      if (module_type::core == mod || module_type::dma == mod) {
        if (0 == graphmetrics[i][0].compare("all")) {

          // Check kernel-name field
          if (0 != graphmetrics[i][1].compare("all")) {
            xrt_core::message::send(severity_level::warning, "XRT", 
              "Only \"all\" is supported in kernel-name field for graph_based_aie_[memory_]metrics. Any other specification is replaced with \"all\".");
          }
          // Capture all tiles across all graphs
          auto graphs = get_graphs(device.get());
          for (auto& graph : graphs) {
            std::vector<tile_type> nwTiles = getAllTilesForCoreMemoryProfiling(mod, graph, handle);
            tiles.insert(tiles.end(), nwTiles.begin(), nwTiles.end());
          } 
          allGraphsDone = true;
        } // "all" 
      }
      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = graphmetrics[i][2];
      }
    }  // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting 
    for (size_t i = 0; i < graphmetricsSettings.size(); ++i) {
      if (3 != graphmetrics[i].size()) {
        // Warning must be already generated in Graph Pass 1. So continue silently here.
        continue;
      }
      std::vector<tile_type> tiles;
      /*
       * Core profiling uses all unique core tiles in aie control
       * Memory profiling uses all unique core + dma tiles in aie control
       */
      if (module_type::core == mod || module_type::dma == mod) {
        if (0 != graphmetrics[i][0].compare("all")) {
          // Check kernel-name field
          if (0 != graphmetrics[i][1].compare("all")) {
            xrt_core::message::send(severity_level::warning, "XRT", 
              "Only \"all\" is supported in kernel-name field for graph_based_aie_[memory_]metrics. Any other specification is replaced with \"all\".");
          }
          // Capture all tiles in the given graph
          tiles = getAllTilesForCoreMemoryProfiling(mod, graphmetrics[i][0] /*graph name*/, handle);
        }
      }
      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = graphmetrics[i][2];
      }
    }  // Graph Pass 2

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * tile_based_aie_metrics = [[{<column>,<row>}|all>:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>]; [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|heat_map|stalls|execution|floating_point|write_bandwidths|read_bandwidths|aie_trace>]]
     *
     * tile_based_aie_memory_metrics = [[<{<column>,<row>}|all>:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths> ]; [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|conflicts|dma_locks|dma_stalls_s2mm|dma_stalls_mm2s|write_bandwidths|read_bandwidths>]]
     *
     * tile_based_mem_tile_metrics = [[<{<column>,<row>}|all>:<off|input_channels|output_channels|memory_stats>[:<channel>]] ; [{<mincolumn,<minrow>}:{<maxcolumn>,<maxrow>}:<off|input_channels|output_channels|memory_stats>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (0 == metrics[i][0].compare("all")) {
        std::vector<tile_type> tiles;
        if (!allGraphsDone) {
          if (module_type::core == mod || module_type::dma == mod) {
            // Capture all tiles across all graphs
            auto graphs = get_graphs(device.get());
            for (auto& graph : graphs) {
              std::vector<tile_type> nwTiles = getAllTilesForCoreMemoryProfiling(mod, graph, handle);
              tiles.insert(tiles.end(), nwTiles.begin(), nwTiles.end());
            } 
            allGraphsDone = true;
          }
        } // allGraphsDone
        for (auto &e : tiles) {
          mConfigMetrics[moduleIdx][e] = metrics[i][1];
        }
      }
    } // Pass 1 

    // Pass 2 : process only range of tiles metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      std::vector<tile_type> tiles;

      if (3 != metrics[i].size()) {
        continue;
      }

      for (size_t j = 0; j < metrics[i].size(); ++j) {
        boost::replace_all(metrics[i][j], "{", "");
        boost::replace_all(metrics[i][j], "}", "");
      }

      uint32_t minRow = 0, minCol = 0;
      uint32_t maxRow = 0, maxCol = 0;

      std::vector<std::string> minTile, maxTile;

      try {
        boost::split(minTile, metrics[i][0], boost::is_any_of(","));
        minCol = std::stoi(minTile[0]);
        minRow = std::stoi(minTile[1]);

        std::vector<std::string> maxTile;
        boost::split(maxTile, metrics[i][1], boost::is_any_of(","));
        maxCol = std::stoi(maxTile[0]);
        maxRow = std::stoi(maxTile[1]);
      } catch (...) {
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Tile range specification in tile_based_aie_[memory}_metrics is not of valid format and hence skipped.");
        continue;
      }

      for (uint32_t col = minCol; col <= maxCol; ++col) {
        for (uint32_t row = minRow; row <= maxRow; ++row) {
          tile_type tile = {0};
          tile.col = col;
          tile.row = row;
          tiles.push_back(tile);
        }
      }
      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = metrics[i][2];
      }
    } // Pass 2 

    // Pass 3 : process only single tile metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {

      std::vector<tile_type> tiles;
      if (2 != metrics[i].size()) {
        continue;
      }
      if (0 == metrics[i][0].compare("all")) {
        continue;
      }
      tile_type tile = {0};
      std::vector<std::string> tilePos;

      try {
        boost::replace_all(metrics[i][0], "{", "");
        boost::replace_all(metrics[i][0], "}", "");

        boost::split(tilePos, metrics[i][0], boost::is_any_of(","));

        tile.col = std::stoi(tilePos[0]);
        tile.row = std::stoi(tilePos[1]);
        tiles.push_back(tile);
      } catch (...) {
        xrt_core::message::send(severity_level::warning, "XRT", 
           "Tile specification in tile_based_aie_[memory}_metrics is not of valid format and hence skipped.");
        continue;
      }

      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = metrics[i][1];
      }
    } // Pass 3 

    // check validity, set default and remove "off" tiles
    std::string moduleName = (mod == module_type::core) ? "aie" : "aie_memory";

    std::vector<tile_type> offTiles;

    // Default any unspecified to the default metric sets.
    std::vector<tile_type> totalTiles;
    // Capture all tiles across all graphs
    auto graphs = get_graphs(device.get());
    for (auto& graph : graphs) {
      std::vector<tile_type> nwTiles = getAllTilesForCoreMemoryProfiling(mod, graph, handle);
      totalTiles.insert(totalTiles.end(), nwTiles.begin(), nwTiles.end());
    } 

    for (auto &e : totalTiles) {
      if (mConfigMetrics[moduleIdx].find(e) == mConfigMetrics[moduleIdx].end()) {
        std::string defaultSet = (mod == module_type::core) ? "heat_map" : "conflicts";
        mConfigMetrics[moduleIdx][e] = defaultSet;
      }
    }

    for (auto &tileMetric : mConfigMetrics[moduleIdx]) {
    
      // save list of "off" tiles
      if (tileMetric.second.empty() || 0 == tileMetric.second.compare("off")) {
        offTiles.push_back(tileMetric.first);
        continue;
      }
       
      // Ensure requested metric set is supported (if not, use default)
      if (((mod == module_type::core) && std::find(metricStrings[mod].begin(), metricStrings[mod].end(), tileMetric.second) == metricStrings[mod].end())
          || ((mod == module_type::dma) && std::find(metricStrings[mod].begin(), metricStrings[mod].end(), tileMetric.second) == metricStrings[mod].end())) {
        std::string defaultSet = (mod == module_type::core) ? "heat_map" : "conflicts";
        std::stringstream msg;
        msg << "Unable to find " << moduleName << " metric set " << tileMetric.second
            << ". Using default of " << defaultSet << "."
            << " As new AIE_profile_settings section is given, old style metric configurations, if any, are ignored.";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        tileMetric.second = defaultSet;
      } 
    }

    // remove all the "off" tiles
    for (auto &t : offTiles) {
      mConfigMetrics[moduleIdx].erase(t);
    }
  }


   // Resolve Interface metrics 
  void
  AieProfileMetadata::getInterfaceConfigMetricsForTiles(int moduleIdx,
                                               const std::vector<std::string>& metricsSettings,
                                               /* std::vector<std::string> graphmetricsSettings, */
                                               void* handle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(handle);

#if 0
    // graph_based_interface_tile_metrics is not supported in XRT in 2022.2
    bool allGraphsDone = false;

    // STEP 1 : Parse per-graph settings

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * "graphmetricsSettings" contains each metric value
     * graph_based_interface_tile_metrics = <graph name|all>:<port name|all>:<off|input_bandwidths|output_bandwidths|packets>
     */

    std::vector<std::vector<std::string>> graphmetrics(graphmetricsSettings.size());

    // Graph Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < graphmetricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(graphmetrics[i], graphmetricsSettings[i], boost::is_any_of(":"));
      if (3 > graphmetrics[i].size()) {
        // Add unexpected format warning
        continue;
      }
      if (0 != graphmetrics[i][0].compare("all")) {
        continue;
      }

      if (0 != graphmetrics[i][1].compare("all")) {
        xrt_core::message::send(severity_level::warning, "XRT",
           "Specific port name is not yet supported in \"graph_based_interface_tile_metrics\" configuration. This will be ignored. Please use \"all\" in port name field.");
      }
      /*
       * Shim profiling uses all tiles utilized by PLIOs
       */
      std::vector<tile_type> tiles;
      tiles = getAllTilesForInterfaceProfiling(handle, graphmetrics[i][2]);
      allGraphsDone = true;

      for (auto &e : tiles) {
        mConfigMetrics[moduleIdx][e] = graphmetrics[i][2];
      }
    }  // Graph Pass 1

    // Graph Pass 2 : process per graph metric setting 
    /* Currently interfaces cannot be tied to graphs.
     * graph_based_interface_tile_metrics = <graph name>:<port name|all>:<off|input_bandwidths|output_bandwidths|packets>
     * is not supported yet.
     */
#endif

    // STEP 2 : Parse per-tile settings: all, bounding box, and/or single tiles

    /* AIE_profile_settings config format ; Multiple values can be specified for a metric separated with ';'
     * tile_based_interface_tile_metrics = [[<column|all>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]] ; [<mincolumn>:<maxcolumn>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]]]
     */

    std::vector<std::vector<std::string>> metrics(metricsSettings.size());

    // Pass 1 : process only "all" metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      // split done only in Pass 1
      boost::split(metrics[i], metricsSettings[i], boost::is_any_of(":"));

      if (0 == metrics[i][0].compare("all")) {

        int16_t channelId = -1;
        if (3 == metrics[i].size()) {
          channelId = std::stoi(metrics[i][2]);
        }

        std::vector<tile_type> tiles;
        tiles = getAllTilesForInterfaceProfiling(handle, metrics[i][1], channelId);

        for (auto &e : tiles) {
          mConfigMetrics[moduleIdx][e] = metrics[i][1];
        }
      }
    } // Pass 1 

    // Pass 2 : process only range of tiles metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      std::vector<tile_type> tiles;

      if (3 > metrics[i].size()) {
        continue;
      }
     /* The following two styles are applicable here 
      * tile_based_interface_tile_metrics = <column|all>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>] 
      * OR
      * tile_based_interface_tile_metrics = <mincolumn>:<maxcolumn>:<off|input_bandwidths|output_bandwidths|packets>[:<channel>]]
      * Handle only the 2nd style here.
      */

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
      if (4 == metrics[i].size()) {
        try {
          channelId = std::stoi(metrics[i][3]);
        } catch (std::invalid_argument const &e) {
          // Expected channel Id is not an integer, give warning and ignore channelId
          xrt_core::message::send(severity_level::warning, "XRT", 
             "Channel ID specification in tile_based_interface_tile_metrics is not an integer and hence ignored.");
          channelId = -1;
        }
      }
      tiles = getAllTilesForInterfaceProfiling(handle, metrics[i][2], channelId,
                                          true, minCol, maxCol);

      for (auto &t : tiles) {
        mConfigMetrics[moduleIdx][t] = metrics[i][2];
      }
    } // Pass 2 

    // Pass 3 : process only single tile metric setting 
    for (size_t i = 0; i < metricsSettings.size(); ++i) {
      std::vector<tile_type> tiles;

      if (4 == metrics[i].size() /* skip column range specification with channel */
            || 2 > metrics[i].size() /* invalid format */) {
        continue;
      }
      if (0 == metrics[i][0].compare("all")) {
        continue;
      }
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
        if (3 == metrics[i].size()) {
          try {
            channelId = std::stoi(metrics[i][2]);
          } catch (std::invalid_argument const &e) {
            // Expected channel Id is not an integer, give warning and ignore channelId
            xrt_core::message::send(severity_level::warning, "XRT", 
               "Channel ID specification in tile_based_interface_tile_metrics is not an integer and hence ignored.");
            channelId = -1;
          }
        }
        tiles = getAllTilesForInterfaceProfiling(handle, metrics[i][1], channelId,
                                            true, col, col);

        for (auto &t : tiles) {
          mConfigMetrics[moduleIdx][t] = metrics[i][1];
        }
      }
    } // Pass 3 

    // check validity, set default and remove "off" tiles
    std::vector<tile_type> offTiles;
    
    // Default any unspecified to the default metric sets.
    std::vector<tile_type> totalTiles;
    totalTiles = getAllTilesForInterfaceProfiling(handle, "input_bandwidths", -1);

    for (auto &e : totalTiles) {
      if (mConfigMetrics[moduleIdx].find(e) == mConfigMetrics[moduleIdx].end()) {
        mConfigMetrics[moduleIdx][e] = "input_bandwidths";
      }
    }

    for (auto &tileMetric : mConfigMetrics[moduleIdx]) {

      // save list of "off" tiles
      if (tileMetric.second.empty() || 0 == tileMetric.second.compare("off")) {
        offTiles.push_back(tileMetric.first);
        continue;
      }

      // Ensure requested metric set is supported (if not, use default)
      if (std::find(metricStrings[module_type::shim].begin(), metricStrings[module_type::shim].end(), tileMetric.second) == metricStrings[module_type::shim].end()) {
        std::string msg = "Unable to find interface_tile metric set " + tileMetric.second
                          + ". Using default of input_bandwidths. "
                          + "As new AIE_profile_settings section is given, old style metric configurations, if any, are ignored.";
        xrt_core::message::send(severity_level::warning, "XRT", msg);
        tileMetric.second = "input_bandwidths" ;
      }
    }

    // remove all the "off" tiles
    for (auto &t : offTiles) {
      mConfigMetrics[moduleIdx].erase(t);
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
