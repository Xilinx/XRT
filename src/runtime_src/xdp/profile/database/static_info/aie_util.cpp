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

#include <cstdint>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <regex>

#include "aie_util.h"

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
#include "core/edge/common/aie_parser.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {
namespace aie {
  namespace pt = boost::property_tree;
  using severity_level = xrt_core::message::severity_level;
  
  // **************************************************************************
  // Private Helpers
  // **************************************************************************

  bool tileCompare(tile_type tile1, tile_type tile2) 
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }

  inline void throwIfError(bool err, const char* msg)
  {
    if (err)
      throw std::runtime_error(msg);
  }

  void readAIEMetadata(const char* data, size_t size, pt::ptree& aie_project)
  {
    std::stringstream aie_stream;
    aie_stream.write(data,size);
    pt::read_json(aie_stream, aie_project);
  }

  // **************************************************************************
  // General Metadata
  // **************************************************************************

  int getHardwareGeneration(const boost::property_tree::ptree& aie_meta)
  {
    static int hwGen = 1;
    static bool gotValue = false;
    if (!gotValue) {
      hwGen = aie_meta.get_child("aie_metadata.driver_config.hw_gen").get_value<int>();
      gotValue = true;
    }
    return hwGen;
  }

  uint16_t getAIETileRowOffset(const boost::property_tree::ptree& aie_meta)
  {
    static uint16_t rowOffset = 1;
    static bool gotValue = false;
    if (!gotValue) {
      rowOffset = aie_meta.get_child("aie_metadata.driver_config.aie_tile_row_start").get_value<uint16_t>();
      gotValue = true;
    }
    return rowOffset;
  }

  aiecompiler_options getAIECompilerOptions(const boost::property_tree::ptree& aie_meta)
  {
    aiecompiler_options aiecompiler_options;
    aiecompiler_options.broadcast_enable_core = aie_meta.get("aie_metadata.aiecompiler_options.broadcast_enable_core", false);
    aiecompiler_options.event_trace = aie_meta.get("aie_metadata.aiecompiler_options.event_trace", "runtime");
    return aiecompiler_options;
  }

  std::vector<std::string> getValidGraphs(const boost::property_tree::ptree& aie_meta)
  {
    std::vector<std::string> graphs;
    for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
      std::string graphName = graph.second.get<std::string>("name");
      graphs.push_back(graphName);
    }
    return graphs;
  }

  std::vector<std::string> getValidKernels(const boost::property_tree::ptree& aie_meta)
  {
    std::vector<std::string> kernels;

    // Grab all kernel to tile mappings
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping)
      return {};

    for (auto const &mapping : kernelToTileMapping.get()) {
      std::vector<std::string> names;
      std::string functionStr = mapping.second.get<std::string>("function");
      boost::split(names, functionStr, boost::is_any_of("."));
      std::unique_copy(names.begin(), names.end(), std::back_inserter(kernels));
    }

    return kernels;
  }

  // **************************************************************************
  // Interface Tiles (PLIO/GMIO)
  // **************************************************************************
  
  std::vector<std::string> getValidPorts(const boost::property_tree::ptree& aie_meta)
  {
    auto ios = getAllIOs(aie_meta);
    if (ios.empty())
      return {};

    std::vector<std::string> ports;

    // Traverse all I/O and include logical and port names
    for (auto &io : ios) {
      std::vector<std::string> nameVec;
      boost::split(nameVec, io.second.name, boost::is_any_of("."));
      ports.emplace_back(nameVec.back());
      ports.emplace_back(io.second.logicalName);
    }
    return ports;
  }

  std::unordered_map<std::string, io_config> 
  getPLIOs(const boost::property_tree::ptree& aie_meta)
  {
    std::unordered_map<std::string, io_config> plios;

    for (auto& plio_node : aie_meta.get_child("aie_metadata.PLIOs")) {
      io_config plio;

      plio.type = 0;
      plio.id = plio_node.second.get<uint32_t>("id");
      plio.name = plio_node.second.get<std::string>("name");
      plio.logicalName = plio_node.second.get<std::string>("logical_name");
      plio.shimColumn = plio_node.second.get<uint16_t>("shim_column");
      plio.streamId = plio_node.second.get<uint16_t>("stream_id");
      plio.slaveOrMaster = plio_node.second.get<bool>("slaveOrMaster");
      plio.channelNum = 0;
      plio.burstLength = 0;

      plios[plio.name] = plio;
    }

    return plios;
  }

  std::unordered_map<std::string, io_config>
  getGMIOs(const boost::property_tree::ptree& aie_meta)
  {
    return getChildGMIOs(aie_meta, "aie_metadata.GMIOs");
  }

  std::unordered_map<std::string, io_config>
  getTraceGMIOs(const boost::property_tree::ptree& aie_meta)
  {
    return getChildGMIOs(aie_meta, "aie_metadata.TraceGMIOs");
  }

  std::unordered_map<std::string, io_config>
  getChildGMIOs(const boost::property_tree::ptree& aie_meta, const std::string& childStr)
  {
    auto gmiosMetadata = aie_meta.get_child_optional(childStr);
    if (!gmiosMetadata)
      return {};

    std::unordered_map<std::string, io_config> gmios;

    for (auto& gmio_node : gmiosMetadata.get()) {
      io_config gmio;

      gmio.type = 1;
      gmio.id = gmio_node.second.get<uint32_t>("id");
      gmio.name = gmio_node.second.get<std::string>("name");
      gmio.logicalName = gmio_node.second.get<std::string>("logical_name");
      gmio.slaveOrMaster = gmio_node.second.get<uint16_t>("type");
      gmio.shimColumn = gmio_node.second.get<uint16_t>("shim_column");
      gmio.channelNum = gmio_node.second.get<uint16_t>("channel_number");
      gmio.streamId = gmio_node.second.get<uint16_t>("stream_id");
      gmio.burstLength = gmio_node.second.get<uint16_t>("burst_length_in_16byte");

      gmios[gmio.name] = gmio;
    }

    return gmios;
  }

  std::unordered_map<std::string, io_config>
  getAllIOs(const boost::property_tree::ptree& aie_meta)
  {
    auto ios = getPLIOs(aie_meta);
    auto gmios = getGMIOs(aie_meta);
    ios.merge(gmios);
    return ios;
  }

  std::vector<tile_type> getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
                                           const std::string& graphName, const std::string& portName,
                                           const std::string& metricStr, int16_t channelId,
                                           bool useColumn, uint32_t minCol, uint32_t maxCol)
  {
    std::vector<tile_type> tiles;

    auto ios = getAllIOs(aie_meta);
    for (auto& io : ios) {
      auto isMaster    = io.second.slaveOrMaster;
      auto streamId    = io.second.streamId;
      auto shimCol     = io.second.shimColumn;
      auto logicalName = io.second.logicalName;
      auto name        = io.second.name;

      auto namePos     = name.find_last_of(".");
      auto currGraph   = name.substr(0, namePos);
      auto currPort    = name.substr(namePos+1);

      // Make sure this matches what we're looking for
      if ((channelId >= 0) && (channelId != streamId))
        continue;
      if ((portName.compare("all") != 0)
           && (portName.compare(currPort) != 0)
           && (portName.compare(logicalName) != 0))
        continue;
      if ((graphName.compare("all") != 0)
           && (graphName.compare(currGraph) != 0))
        continue;

      // Make sure it's desired polarity
      // NOTE: input = slave (data flowing from PLIO)
      //       output = master (data flowing to PLIO)
      if ((metricStr != "ports")
          && ((isMaster && (metricStr.find("input") != std::string::npos))
          || (!isMaster && (metricStr.find("output") != std::string::npos))))
        continue;
      // Make sure column is within specified range (if specified)
      if (useColumn && !((minCol <= (uint32_t)shimCol) && ((uint32_t)shimCol <= maxCol)))
        continue;

      tile_type tile = {0};
      tile.col = shimCol;
      tile.row = 0;
      // Grab stream ID and slave/master (used in configStreamSwitchPorts())
      tile.itr_mem_col = isMaster;
      tile.itr_mem_row = streamId;
      tiles.emplace_back(std::move(tile));
    }

    if (tiles.empty() && (channelId >= 0)) {
      std::string msg =
          "No tiles used channel ID " + std::to_string(channelId) + ". Please specify a valid channel ID.";
      xrt_core::message::send(severity_level::warning, "XRT", msg);
    }
    return tiles;
  }

  // **************************************************************************
  // Memory Tiles
  // **************************************************************************

  // Find all memory tiles associated with a graph and kernel
  //   kernel_name = all      : all tiles in graph
  //   kernel_name = <kernel> : only tiles used by that specific kernel
  std::vector<tile_type> getMemoryTiles(const boost::property_tree::ptree& aie_meta, const std::string& graph_name,
                                        const std::string& kernel_name)
  {
    if (getHardwareGeneration(aie_meta) == 1) 
      return {};

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
      auto currGraph = shared_buffer.second.get<std::string>("graph");
      if ((currGraph.find(graph_name) == std::string::npos)
           && (graph_name.compare("all") != 0))
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

  // **************************************************************************
  // AIE Tiles
  // **************************************************************************
  
  // Find all AIE tiles associated with a graph (kernel_name = all)
  std::vector<tile_type> getAIETiles(const boost::property_tree::ptree& aie_meta, const std::string& graph_name)
  {
    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset(aie_meta);
    int startCount = 0;

    for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
      if ((graph.second.get<std::string>("name") != graph_name)
           && (graph_name.compare("all") != 0))
        continue;

      int count = startCount;
      for (auto& node : graph.second.get_child("core_columns")) {
        tiles.push_back(tile_type());
        auto& t = tiles.at(count++);
        t.col = static_cast<uint16_t>(std::stoul(node.second.data()));
      }

      int num_tiles = count;
      count = startCount;
      for (auto& node : graph.second.get_child("core_rows"))
        tiles.at(count++).row = static_cast<uint16_t>(std::stoul(node.second.data())) + rowOffset;
      throwIfError(count < num_tiles,"core_rows < num_tiles");

      count = startCount;
      for (auto& node : graph.second.get_child("iteration_memory_columns"))
        tiles.at(count++).itr_mem_col = static_cast<uint16_t>(std::stoul(node.second.data()));
      throwIfError(count < num_tiles,"iteration_memory_columns < num_tiles");

      count = startCount;
      for (auto& node : graph.second.get_child("iteration_memory_rows"))
        tiles.at(count++).itr_mem_row = static_cast<uint16_t>(std::stoul(node.second.data()));
      throwIfError(count < num_tiles,"iteration_memory_rows < num_tiles");

      count = startCount;
      for (auto& node : graph.second.get_child("iteration_memory_addresses"))
        tiles.at(count++).itr_mem_addr = std::stoul(node.second.data());
      throwIfError(count < num_tiles,"iteration_memory_addresses < num_tiles");

      count = startCount;
      for (auto& node : graph.second.get_child("multirate_triggers"))
        tiles.at(count++).is_trigger = (node.second.data() == "true");
      throwIfError(count < num_tiles,"multirate_triggers < num_tiles");

      startCount = count;
    }

    return tiles;    
  }

  std::vector<tile_type> getEventTiles(const boost::property_tree::ptree& aie_meta, const std::string& graph_name,
                                       module_type type)
  {
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
          t.col = static_cast<uint16_t>(std::stoul(node.second.data()));
        }

        int num_tiles = count;
        count = 0;
        for (auto& node : graph.second.get_child(row_name))
          tiles.at(count++).row = static_cast<uint16_t>(std::stoul(node.second.data()));
        throwIfError(count < num_tiles,"rows < num_tiles");
    }

    return tiles;
  }
  
  // Find all AIE or memory tiles associated with a graph and kernel
  //   kernel_name = all      : all tiles in graph
  //   kernel_name = <kernel> : only tiles used by that specific kernel
  std::vector<tile_type> getTiles(const boost::property_tree::ptree& aie_meta, const std::string& graph_name,
                                  module_type type, const std::string& kernel_name)
  {
    if (type == module_type::mem_tile)
      return getMemoryTiles(aie_meta, graph_name, kernel_name);
    if (kernel_name.compare("all") == 0)
      return getAIETiles(aie_meta, graph_name);

    // Now search by graph-kernel pairs
    auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
    if (!kernelToTileMapping)
      return {};

    std::vector<tile_type> tiles;
    auto rowOffset = getAIETileRowOffset(aie_meta);

    for (auto const &mapping : kernelToTileMapping.get()) {
      auto currGraph = mapping.second.get<std::string>("graph");
      if ((currGraph.find(graph_name) == std::string::npos)
           && (graph_name.compare("all") != 0))
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
} // namespace aie
} // namespace xdp
