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
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <optional>
#include <set>
#include <memory>

#include "filetypes/aie_control_config_filetype.h"
#include "aie_util.h"
#include "core/common/message.h"

// ***************************************************************
// Anonymous namespace for helper functions local to this file
// ***************************************************************
namespace xdp::aie {
  namespace pt = boost::property_tree;
  using severity_level = xrt_core::message::severity_level;

  bool tileCompare(xdp::tile_type tile1, xdp::tile_type tile2) 
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }

  void throwIfError(bool err, const char* msg)
  {
    if (err)
      throw std::runtime_error(msg);
  }

std::unique_ptr<xdp::aie::BaseFiletypeImpl>
determineFileType(boost::property_tree::ptree& aie_project)
{
  // ****************************************************
  // Check if it is the known compiler_report.json format
  // ****************************************************
  try {
    std::string schema;
    schema = aie_project.get_child("schema").get_value<std::string>();
    if (schema == "MEGraphSchema-0.4")
      return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
  }
  catch (...) {
    // Something went wrong, so it most likely is not a "compiler_report.json"
  }

  // *******************************************************
  // Check if it is the known aie_control_config.json format
  // *******************************************************
  try {
    std::string major;
    major = aie_project.get_child("schema_version.major").get_value<std::string>();
    if (major == "1")
      return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
  }
  catch(...) {
    // Something went wrong, so it most likely is not an aie_control_config
  }

  // *******************************************************
  // Check if it is the known handwritten format
  // *******************************************************
  try {
    auto schema = aie_project.get_child("schema").get_value<std::string>();
    if (schema == "handwritten")
      return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
  }
  catch(...) {
    // Something went wrong, so it most likely is not the handwritten format
  }

  // We could not determine the type
  return nullptr;
}

// *****************************************************************
// Parsing functions that are the same for all formats, or just have
// different roots.
// *****************************************************************
// Hardware generation shouldn't change once execution has started.
// The physical devices will only have one version of the AIE silicon
int getHardwareGeneration(const boost::property_tree::ptree& aie_meta,
                          const std::string& root)
{
  static std::optional<int> hwGen;
  if (!hwGen.has_value()) {
    hwGen = aie_meta.get_child(root).get_value<int>();
  }
  return *hwGen;
}

xdp::aie::driver_config
getDriverConfig(const boost::property_tree::ptree& aie_meta,
                const std::string& root)
{
  xdp::aie::driver_config config;
  auto meta_config = aie_meta.get_child(root);

  config.hw_gen =
    meta_config.get_child("hw_gen").get_value<uint8_t>();
  config.base_address =
    meta_config.get_child("base_address").get_value<uint64_t>();
  config.column_shift =
    meta_config.get_child("column_shift").get_value<uint8_t>();
  config.row_shift =
    meta_config.get_child("row_shift").get_value<uint8_t>();
  config.num_rows =
    meta_config.get_child("num_rows").get_value<uint8_t>();
  config.num_columns =
    meta_config.get_child("num_columns").get_value<uint8_t>();
  config.shim_row =
    meta_config.get_child("shim_row").get_value<uint8_t>();
  config.mem_row_start =
    meta_config.get_child("mem_row_start").get_value<uint8_t>();
  config.mem_num_rows =
    meta_config.get_child("mem_num_rows").get_value<uint8_t>();
  config.aie_tile_row_start =
    meta_config.get_child("aie_tile_row_start").get_value<uint8_t>();
  config.aie_tile_num_rows =
    meta_config.get_child("aie_tile_num_rows").get_value<uint8_t>();
  return config;
}

uint16_t
getAIETileRowOffset(const boost::property_tree::ptree& aie_meta,
                    const std::string& location)
{
  static std::optional<uint16_t> rowOffset;
  if (!rowOffset.has_value()) {
    rowOffset = aie_meta.get_child(location).get_value<uint16_t>();
  }
  return *rowOffset;
}

std::vector<std::string>
getValidGraphs(const boost::property_tree::ptree& aie_meta,
                const std::string& root)
{
  std::vector<std::string> graphs;
  for (auto& graph : aie_meta.get_child(root)) {
    std::string graphName = graph.second.get<std::string>("name");
    graphs.push_back(graphName);
  }
  return graphs;
}


// // ***************************************************************
// // The implementation specific to our handwritten JSON file format
// // ***************************************************************
// namespace xdp::aie::handwritten {

//   aiecompiler_options
//   getAIECompilerOptions(const boost::property_tree::ptree& aie_meta)
//   {
//     aiecompiler_options aiecompiler_options;
//     aiecompiler_options.broadcast_enable_core = 
//         aie_meta.get("aiecompiler_options.broadcast_enable_core", false);
//     aiecompiler_options.graph_iterator_event = 
//         aie_meta.get("aiecompiler_options.graph_iterator_event", false);
//     aiecompiler_options.event_trace = 
//         aie_meta.get("aiecompiler_options.event_trace", "runtime");
//     return aiecompiler_options;
//   }

//   std::vector<std::string>
//   getValidPorts(const boost::property_tree::ptree& aie_meta)
//   {
//     std::vector<std::string> built;

//     // Get the part of the name before the "." and then the logical name
//     for (auto& plio : aie_meta.get_child("PLIOs")) {
//       std::vector<std::string> nameVec;
//       auto name = plio.second.get<std::string>("name");
//       boost::split(nameVec, name, boost::is_any_of("."));
//       built.emplace_back(nameVec.back());
//       auto logicalName = plio.second.get<std::string>("logical_name");
//       built.emplace_back(logicalName);
//     }

//     // Get the part of the name before the "." and then the logical name
//     for (auto& gmio : aie_meta.get_child("GMIOs")) {
//       std::vector<std::string> nameVec;
//       auto name = gmio.second.get<std::string>("name");
//       boost::split(nameVec, name, boost::is_any_of("."));
//       built.emplace_back(nameVec.back());
//       built.emplace_back(gmio.second.get<std::string>("logical_name"));
//     }

//     return built;
//   }

//   std::vector<std::string>
//   getValidKernels(const boost::property_tree::ptree& aie_meta)
//   {
//     std::vector<std::string> kernels;
//     std::set<std::string> uniqueKernelNames;

//     for (auto& graph : aie_meta.get_child("graphs")) {
//       for (auto& tile : graph.second.get_child("tiles")) {
//         uniqueKernelNames.emplace(tile.second.get_value<std::string>("kernel"));
//       }
//     }

//     for (auto& name : uniqueKernelNames) {
//       kernels.push_back(name);
//     }
//     return kernels;
//   }

//   std::unordered_map<std::string, io_config>
//   getTraceGMIOs(const boost::property_tree::ptree& aie_meta)
//   {
//     std::unordered_map<std::string, io_config> traceGMIOs;
//     for (auto& gmio : aie_meta.get_child("GMIOs")) {
//       bool trace = gmio.second.get<bool>("trace");
//       if (trace) {
//         io_config config;
//         config.id          = gmio.second.get<uint32_t>("id");
//         config.name        = gmio.second.get<std::string>("name");
//         config.logicalName = gmio.second.get<std::string>("logical_name");
//         config.shimColumn  = gmio.second.get<uint16_t>("shim_column");
//         config.channelNum  = gmio.second.get<uint16_t>("channel_number");
//         config.streamId    = gmio.second.get<uint16_t>("stream_id");
//         config.burstLength = gmio.second.get<uint16_t>("burst_length_in_16_byte");
//         config.type        = gmio.second.get<uint16_t>("type");

//         traceGMIOs[config.name] = config;
//       }
//     }
//     return traceGMIOs;
//   }

//   std::vector<tile_type>
//   getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
//                     const std::string& graphName,
//                     const std::string& portName,
//                     const std::string& metricStr,
//                     int16_t channelId,
//                     bool useColumn,
//                     uint32_t minCol,
//                     uint32_t maxCol)
//   {
//     std::vector<tile_type> tiles;

//     for (auto& plio : aie_meta.get_child("PLIOs")) {
//       tile_type tile;
//       tile.col = plio.second.get<uint16_t>("shim_column");
//       tile.row = 0;
//       tile.itr_mem_col = plio.second.get<bool>("master_port") ? 1 : 0 ;
//       tile.itr_mem_row = plio.second.get<uint16_t>("stream_id");
//       tiles.emplace_back(tile);
//     }

//     for (auto& gmio : aie_meta.get_child("GMIOs")) {
//       tile_type tile;
//       tile.col = gmio.second.get<uint16_t>("shim_column");
//       tile.row = 0;
//       tile.itr_mem_col = gmio.second.get<uint8_t>("type");
//       tile.itr_mem_row = gmio.second.get<uint16_t>("stream_id");
//       tiles.emplace_back(tile);
//     }

//     return tiles;
//   }

//   std::vector<tile_type>
//   getMemoryTiles(const boost::property_tree::ptree& aie_meta,
//                  const std::string& graph_name,
//                  const std::string& buffer_name)
//   {
//     return {}; // TBD
//   }

//   std::vector<tile_type>
//   getAIETiles(const boost::property_tree::ptree& aie_meta,
//               const std::string& graph_name)
//   {
//     return {};
//   }

//   std::vector<tile_type>
//   getEventTiles(const boost::property_tree::ptree& aie_meta,
//                 const std::string& graph_name,
//                 module_type type)
//   {
//     return {};
//   }

//   std::vector<tile_type>
//   getTiles(const boost::property_tree::ptree& aie_meta,
//            const std::string& graph_name,
//            module_type type,
//            const std::string& kernel_name)
//   {
//     return {};
//   }
// } // end namespace xdp::aie::handwritten

// // ***************************************************************
// // The implementation specific to the compiler_report.json file
// // ***************************************************************
// namespace xdp::aie::compiler_report {

//   // Not currently available in compiler_report.json so just return
//   // the defaults.
//   aiecompiler_options
//   getAIECompilerOptions(const boost::property_tree::ptree& aie_meta)
//   {
//     aiecompiler_options aiecompiler_options;
//     aiecompiler_options.broadcast_enable_core = false;
//     aiecompiler_options.graph_iterator_event = false;
//     aiecompiler_options.event_trace = "runtime";
//     return aiecompiler_options;
//   }

//   std::unordered_map<std::string, io_config>
//   getTraceGMIOs(const boost::property_tree::ptree& aie_meta)
//   {
//     return {};
//   }

//   std::vector<std::string>
//   getValidPorts(const boost::property_tree::ptree& aie_meta)
//   {
//     return {};
//   }

//   std::vector<std::string>
//   getValidKernels(const boost::property_tree::ptree& aie_meta)
//   {
//     return {};
//   }

//   std::vector<tile_type>
//   getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
//                     const std::string& graphName,
//                     const std::string& portName,
//                     const std::string& metricStr,
//                     int16_t channelId,
//                     bool useColumn,
//                     uint32_t minCol,
//                     uint32_t maxCol)
//   {
//     return {};
//   }

//   std::vector<tile_type>
//   getMemoryTiles(const boost::property_tree::ptree& aie_meta,
//                  const std::string& graph_name,
//                  const std::string& buffer_name)
//   {
//     return {};
//   }

//   std::vector<tile_type>
//   getAIETiles(const boost::property_tree::ptree& aie_meta,
//               const std::string& graph_name)
//   {
//     return {};
//   }

//   std::vector<tile_type>
//   getEventTiles(const boost::property_tree::ptree& aie_meta,
//                 const std::string& graph_name,
//                 module_type type)
//   {
//     return {};
//   }

//   std::vector<tile_type>
//   getTiles(const boost::property_tree::ptree& aie_meta,
//            const std::string& graph_name,
//            module_type type,
//            const std::string& kernel_name)
//   {
//     return {};
//   }
// } // end namespace xdp::aie::compiler_report

// // ***************************************************************
// // The implementation specific to the aie_control_config.json file
// // ***************************************************************
// namespace xdp::aie::aie_control_config {

//   using severity_level = xrt_core::message::severity_level;

//   std::unordered_map<std::string, io_config> 
//   getPLIOs(const boost::property_tree::ptree& aie_meta)
//   {
//     std::unordered_map<std::string, io_config> plios;

//     for (auto& plio_node : aie_meta.get_child("aie_metadata.PLIOs")) {
//       io_config plio;

//       plio.type = 0;
//       plio.id = plio_node.second.get<uint32_t>("id");
//       plio.name = plio_node.second.get<std::string>("name");
//       plio.logicalName = plio_node.second.get<std::string>("logical_name");
//       plio.shimColumn = plio_node.second.get<uint16_t>("shim_column");
//       plio.streamId = plio_node.second.get<uint16_t>("stream_id");
//       plio.slaveOrMaster = plio_node.second.get<bool>("slaveOrMaster");
//       plio.channelNum = 0;
//       plio.burstLength = 0;

//       plios[plio.name] = plio;
//     }

//     return plios;
//   }

//   std::unordered_map<std::string, io_config>
//   getChildGMIOs(const boost::property_tree::ptree& aie_meta, const std::string& childStr)
//   {
//     auto gmiosMetadata = aie_meta.get_child_optional(childStr);
//     if (!gmiosMetadata)
//       return {};

//     std::unordered_map<std::string, io_config> gmios;

//     for (auto& gmio_node : gmiosMetadata.get()) {
//       io_config gmio;

//       gmio.type = 1;
//       gmio.id = gmio_node.second.get<uint32_t>("id");
//       gmio.name = gmio_node.second.get<std::string>("name");
//       gmio.logicalName = gmio_node.second.get<std::string>("logical_name");
//       gmio.slaveOrMaster = gmio_node.second.get<uint16_t>("type");
//       gmio.shimColumn = gmio_node.second.get<uint16_t>("shim_column");
//       gmio.channelNum = gmio_node.second.get<uint16_t>("channel_number");
//       gmio.streamId = gmio_node.second.get<uint16_t>("stream_id");
//       gmio.burstLength = gmio_node.second.get<uint16_t>("burst_length_in_16byte");

//       gmios[gmio.name] = gmio;
//     }

//     return gmios;
//   }

//   std::unordered_map<std::string, io_config>
//   getGMIOs(const boost::property_tree::ptree& aie_meta)
//   {
//     return getChildGMIOs(aie_meta, "aie_metadata.GMIOs");
//   }

//   std::unordered_map<std::string, io_config>
//   getTraceGMIOs(const boost::property_tree::ptree& aie_meta)
//   {
//     return getChildGMIOs(aie_meta, "aie_metadata.TraceGMIOs");
//   }

//   std::unordered_map<std::string, io_config>
//   getAllIOs(const boost::property_tree::ptree& aie_meta)
//   {
//     auto ios = getPLIOs(aie_meta);
//     auto gmios = getGMIOs(aie_meta);
//     ios.merge(gmios);
//     return ios;
//   }

//   aiecompiler_options
//   getAIECompilerOptions(const boost::property_tree::ptree& aie_meta)
//   {
//     aiecompiler_options aiecompiler_options;
//     aiecompiler_options.broadcast_enable_core = 
//         aie_meta.get("aie_metadata.aiecompiler_options.broadcast_enable_core", false);
//     aiecompiler_options.graph_iterator_event = 
//         aie_meta.get("aie_metadata.aiecompiler_options.graph_iterator_event", false);
//     aiecompiler_options.event_trace = 
//         aie_meta.get("aie_metadata.aiecompiler_options.event_trace", "runtime");
//     return aiecompiler_options;
//   }

//   std::vector<std::string>
//   getValidPorts(const boost::property_tree::ptree& aie_meta)
//   {
//     auto ios = getAllIOs(aie_meta);
//     if (ios.empty())
//       return {};

//     std::vector<std::string> ports;

//     // Traverse all I/O and include logical and port names
//     for (auto &io : ios) {
//       std::vector<std::string> nameVec;
//       boost::split(nameVec, io.second.name, boost::is_any_of("."));
//       ports.emplace_back(nameVec.back());
//       ports.emplace_back(io.second.logicalName);
//     }
//     return ports;
//   }

//   std::vector<std::string>
//   getValidKernels(const boost::property_tree::ptree& aie_meta)
//   {
//     std::vector<std::string> kernels;

//     // Grab all kernel to tile mappings
//     auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
//     if (!kernelToTileMapping)
//       return {};

//     for (auto const &mapping : kernelToTileMapping.get()) {
//       std::vector<std::string> names;
//       std::string functionStr = mapping.second.get<std::string>("function");
//       boost::split(names, functionStr, boost::is_any_of("."));
//       std::unique_copy(names.begin(), names.end(), std::back_inserter(kernels));
//     }

//     return kernels;
//   }

//   std::vector<tile_type>
//   getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
//                     const std::string& graphName,
//                     const std::string& portName,
//                     const std::string& metricStr,
//                     int16_t channelId,
//                     bool useColumn,
//                     uint32_t minCol,
//                     uint32_t maxCol)
//   {
//     std::vector<tile_type> tiles;

//     #ifdef XDP_MINIMAL_BUILD
//       auto ios = getGMIOs(aie_meta);
//     #else
//       auto ios = getAllIOs(aie_meta);
//     #endif

//     for (auto& io : ios) {
//       auto isMaster    = io.second.slaveOrMaster;
//       auto streamId    = io.second.streamId;
//       auto shimCol     = io.second.shimColumn;
//       auto logicalName = io.second.logicalName;
//       auto name        = io.second.name;

//       auto namePos     = name.find_last_of(".");
//       auto currGraph   = name.substr(0, namePos);
//       auto currPort    = name.substr(namePos+1);

//       // Make sure this matches what we're looking for
//       //if ((channelId >= 0) && (channelId != streamId))
//       //  continue;
//       if ((portName.compare("all") != 0)
//            && (portName.compare(currPort) != 0)
//            && (portName.compare(logicalName) != 0))
//         continue;
//       if ((graphName.compare("all") != 0)
//            && (graphName.compare(currGraph) != 0))
//         continue;

//       // Make sure it's desired polarity
//       // NOTE: input = slave (data flowing from PLIO)
//       //       output = master (data flowing to PLIO)
//       if ((metricStr != "ports")
//           && ((isMaster && (metricStr.find("input") != std::string::npos
//               || metricStr.find("mm2s") != std::string::npos))
//           || (!isMaster && (metricStr.find("output") != std::string::npos
//               || metricStr.find("s2mm") != std::string::npos))))
//         continue;
//       // Make sure column is within specified range (if specified)
//       if (useColumn && !((minCol <= (uint32_t)shimCol) && ((uint32_t)shimCol <= maxCol)))
//         continue;

//       if ((channelId >= 0) && (channelId != io.second.channelNum)) 
//         continue;

//       tile_type tile = {0};
//       tile.col = shimCol;
//       tile.row = 0;
//       // Grab stream ID and slave/master (used in configStreamSwitchPorts())
//       tile.itr_mem_col = isMaster;
//       tile.itr_mem_row = streamId;
  
//       tiles.emplace_back(std::move(tile));
//     }

//     if (tiles.empty() && (channelId >= 0)) {
//       std::string msg =
//           "No tiles used channel ID " + std::to_string(channelId) + ". Please specify a valid channel ID.";
//       xrt_core::message::send(severity_level::warning, "XRT", msg);
//     }

//     return tiles;
//   }

//   // Find all memory tiles associated with a graph and buffer
//   //   buffer_name = all      : all tiles in graph
//   //   buffer_name = <buffer> : only tiles used by that specific buffer
//   std::vector<tile_type>
//   getMemoryTiles(const boost::property_tree::ptree& aie_meta,
//                  const std::string& graph_name,
//                  const std::string& buffer_name)
//   {
//     if (getHardwareGeneration(aie_meta) == 1) 
//       return {};

//     // Grab all shared buffers
//     auto sharedBufferTree = aie_meta.get_child_optional("aie_metadata.TileMapping.SharedBufferToTileMapping");
//     if (!sharedBufferTree)
//       return {};

//     std::vector<tile_type> allTiles;
//     std::vector<tile_type> memTiles;
//     // Always one row of interface tiles
//     uint16_t rowOffset = 1;

//     // Now parse all shared buffers
//     for (auto const &shared_buffer : sharedBufferTree.get()) {
//       auto currGraph = shared_buffer.second.get<std::string>("graph");
//       if ((currGraph.find(graph_name) == std::string::npos)
//            && (graph_name.compare("all") != 0))
//         continue;
//       auto currBuffer = shared_buffer.second.get<std::string>("bufferName");
//       if ((currBuffer.find(buffer_name) == std::string::npos)
//            && (buffer_name.compare("all") != 0))
//         continue;

//       tile_type tile;
//       tile.col = shared_buffer.second.get<uint16_t>("column");
//       tile.row = shared_buffer.second.get<uint16_t>("row") + rowOffset;
//       allTiles.emplace_back(std::move(tile));
//     }

//     std::unique_copy(allTiles.begin(), allTiles.end(), std::back_inserter(memTiles), tileCompare);
//     return memTiles;
//   }

//   // Find all AIE tiles associated with a graph (kernel_name = all)
//   std::vector<tile_type> getAIETiles(const boost::property_tree::ptree& aie_meta, const std::string& graph_name)
//   {
//     std::vector<tile_type> tiles;
//     auto rowOffset = getAIETileRowOffset(aie_meta, AIE_CONTROL_CONFIG);
//     int startCount = 0;

//     for (auto& graph : aie_meta.get_child("aie_metadata.graphs")) {
//       if ((graph.second.get<std::string>("name") != graph_name)
//            && (graph_name.compare("all") != 0))
//         continue;

//       int count = startCount;
//       for (auto& node : graph.second.get_child("core_columns")) {
//         tiles.push_back(tile_type());
//         auto& t = tiles.at(count++);
//         t.col = static_cast<uint16_t>(std::stoul(node.second.data()));
//       }

//       int num_tiles = count;
//       count = startCount;
//       for (auto& node : graph.second.get_child("core_rows"))
//         tiles.at(count++).row = static_cast<uint16_t>(std::stoul(node.second.data())) + rowOffset;
//       throwIfError(count < num_tiles,"core_rows < num_tiles");

//       count = startCount;
//       for (auto& node : graph.second.get_child("iteration_memory_columns"))
//         tiles.at(count++).itr_mem_col = static_cast<uint16_t>(std::stoul(node.second.data()));
//       throwIfError(count < num_tiles,"iteration_memory_columns < num_tiles");

//       count = startCount;
//       for (auto& node : graph.second.get_child("iteration_memory_rows"))
//         tiles.at(count++).itr_mem_row = static_cast<uint16_t>(std::stoul(node.second.data()));
//       throwIfError(count < num_tiles,"iteration_memory_rows < num_tiles");

//       count = startCount;
//       for (auto& node : graph.second.get_child("iteration_memory_addresses"))
//         tiles.at(count++).itr_mem_addr = std::stoul(node.second.data());
//       throwIfError(count < num_tiles,"iteration_memory_addresses < num_tiles");

//       count = startCount;
//       for (auto& node : graph.second.get_child("multirate_triggers"))
//         tiles.at(count++).is_trigger = (node.second.data() == "true");
//       throwIfError(count < num_tiles,"multirate_triggers < num_tiles");

//       startCount = count;
//     }

//     return tiles;
//   }

//   std::vector<tile_type>
//   getEventTiles(const boost::property_tree::ptree& aie_meta,
//                 const std::string& graph_name,
//                 module_type type)
//   {
//     if (type == module_type::shim)
//       return {};

//     const char* col_name = (type == module_type::core) ? "core_columns" : "dma_columns";
//     const char* row_name = (type == module_type::core) ?    "core_rows" :    "dma_rows";

//     std::vector<tile_type> tiles;
  
//     for (auto& graph : aie_meta.get_child("aie_metadata.EventGraphs")) {
//       if (graph.second.get<std::string>("name") != graph_name)
//         continue;

//       int count = 0;
//         for (auto& node : graph.second.get_child(col_name)) {
//           tiles.push_back(tile_type());
//           auto& t = tiles.at(count++);
//           t.col = static_cast<uint16_t>(std::stoul(node.second.data()));
//         }

//         int num_tiles = count;
//         count = 0;
//         for (auto& node : graph.second.get_child(row_name))
//           tiles.at(count++).row = static_cast<uint16_t>(std::stoul(node.second.data()));
//         throwIfError(count < num_tiles,"rows < num_tiles");
//     }

//     return tiles;
//   }
  
//   // Find all AIE or memory tiles associated with a graph and kernel/buffer
//   //   kernel_name = all      : all tiles in graph
//   //   kernel_name = <kernel> : only tiles used by that specific kernel
//   std::vector<tile_type>
//   getTiles(const boost::property_tree::ptree& aie_meta,
//            const std::string& graph_name,
//            module_type type,
//            const std::string& kernel_name)
//   {
//     if (type == module_type::mem_tile)
//       return getMemoryTiles(aie_meta, graph_name, kernel_name);

//     // Now search by graph-kernel pairs
//     auto kernelToTileMapping = aie_meta.get_child_optional("aie_metadata.TileMapping.AIEKernelToTileMapping");
//     if (!kernelToTileMapping && kernel_name.compare("all") == 0)
//       return getAIETiles(aie_meta, graph_name);
//     if (!kernelToTileMapping)
//       return {};

//     std::vector<tile_type> tiles;
//     auto rowOffset = getAIETileRowOffset(aie_meta, AIE_CONTROL_CONFIG);

//     for (auto const &mapping : kernelToTileMapping.get()) {
//       auto currGraph = mapping.second.get<std::string>("graph");
//       if ((currGraph.find(graph_name) == std::string::npos)
//            && (graph_name.compare("all") != 0))
//         continue;
//       if (kernel_name.compare("all") != 0) {
//         std::vector<std::string> names;
//         std::string functionStr = mapping.second.get<std::string>("function");
//         boost::split(names, functionStr, boost::is_any_of("."));
//         if (std::find(names.begin(), names.end(), kernel_name) == names.end())
//             continue;
//       }

//       tile_type tile;
//       tile.col = mapping.second.get<uint16_t>("column");
//       tile.row = mapping.second.get<uint16_t>("row") + rowOffset;
//       tiles.emplace_back(std::move(tile));
//     }
//     return tiles;
//   }

// } // end namespace xdp::aie::aie_control_config

// The implementation of the general interface


  std::unique_ptr<xdp::aie::BaseFiletypeImpl>
  readAIEMetadata(const char* data, size_t size, pt::ptree& aie_project)
  {
    std::stringstream aie_stream;
    aie_stream.write(data,size);
    pt::read_json(aie_stream, aie_project);

    return determineFileType(aie_project);
  }

  std::unique_ptr<xdp::aie::BaseFiletypeImpl>
  readAIEMetadata(const char* filename, pt::ptree& aie_project)
  {
    try {
      pt::read_json(filename, aie_project);
    }
    catch (...) {
      std::stringstream msg;
      msg << "The AIE metadata JSON file is required in the same directory"
          << " as the host executable to run AIE Profile.";
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return nullptr;
    }

    return determineFileType(aie_project);
  }

//   // **************************************************************************
//   // General Interface to the Metadata
//   // **************************************************************************

//   driver_config
//   getDriverConfig(const boost::property_tree::ptree& aie_meta,
//                   MetadataFileType type)
//  {
//     switch (type) {
//     case COMPILER_REPORT:
//       return ::getDriverConfig(aie_meta, "aie_driver_config");
//     case AIE_CONTROL_CONFIG:
//       return ::getDriverConfig(aie_meta, "aie_metadata.driver_config");
//     case HANDWRITTEN:
//       return ::getDriverConfig(aie_meta, "driver_config");
//     case UNKNOWN_FILE: // fallthrough
//     default:
//       return {};
//     }
//   }

//   int getHardwareGeneration(const boost::property_tree::ptree& aie_meta,
//                             MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return ::getHardwareGeneration(aie_meta, "aie_driver_config.hw_gen");
//     case AIE_CONTROL_CONFIG:
//       return ::getHardwareGeneration(aie_meta, "aie_metadata.driver_config.hw_gen");
//     case HANDWRITTEN:
//       return ::getHardwareGeneration(aie_meta, "driver_config.hw_gen");
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return 1;
//     }
//   }

//   aiecompiler_options
//   getAIECompilerOptions(const boost::property_tree::ptree& aie_meta,
//                         MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return compiler_report::getAIECompilerOptions(aie_meta);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getAIECompilerOptions(aie_meta);
//     case HANDWRITTEN:
//       return handwritten::getAIECompilerOptions(aie_meta);
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return { false, false, "runtime" };
//     }
//   }

//   uint16_t getAIETileRowOffset(const boost::property_tree::ptree& aie_meta,
//                                MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return ::getAIETileRowOffset(aie_meta, "aie_driver_config.aie_tile_row_start");
//     case AIE_CONTROL_CONFIG:
//       return ::getAIETileRowOffset(aie_meta, "aie_metadata.driver_config.aie_tile_row_start");
//     case HANDWRITTEN:
//       return ::getAIETileRowOffset(aie_meta, "driver_config.aie_tile_row_start");
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return 0;
//     }
//   }

//   std::vector<std::string>
//   getValidGraphs(const boost::property_tree::ptree& aie_meta,
//                   MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return {}; // TBD
//     case AIE_CONTROL_CONFIG:
//       return ::getValidGraphs(aie_meta, "aie_metadata.graphs");
//     case HANDWRITTEN:
//       return ::getValidGraphs(aie_meta, "graphs");
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return {};
//     }
//   }

//   std::vector<std::string>
//   getValidPorts(const boost::property_tree::ptree& aie_meta,
//                 MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return compiler_report::getValidPorts(aie_meta);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getValidPorts(aie_meta);
//     case HANDWRITTEN:
//       return handwritten::getValidPorts(aie_meta);
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return {};
//     }
//   }

//   std::vector<std::string>
//   getValidKernels(const boost::property_tree::ptree& aie_meta,
//                   MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return compiler_report::getValidKernels(aie_meta);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getValidKernels(aie_meta);
//     case HANDWRITTEN:
//       return handwritten::getValidKernels(aie_meta);
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return {};
//     }
//   }

//   std::unordered_map<std::string, io_config>
//   getTraceGMIOs(const boost::property_tree::ptree& aie_meta,
//                 MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return compiler_report::getTraceGMIOs(aie_meta);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getTraceGMIOs(aie_meta);
//     case HANDWRITTEN:
//       return handwritten::getTraceGMIOs(aie_meta);
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return {};
//     }
//   }

//   std::vector<tile_type>
//   getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
//                     const std::string& graphName,
//                     const std::string& portName,
//                     const std::string& metricStr,
//                     MetadataFileType type,
//                     int16_t channelId,
//                     bool useColumn,
//                     uint32_t minCol,
//                     uint32_t maxCol)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return compiler_report::getInterfaceTiles(aie_meta,
//                                                 graphName,
//                                                 portName,
//                                                 metricStr,
//                                                 channelId,
//                                                 useColumn,
//                                                 minCol,
//                                                 maxCol);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getInterfaceTiles(aie_meta,
//                                                    graphName,
//                                                    portName,
//                                                    metricStr,
//                                                    channelId,
//                                                    useColumn,
//                                                    minCol,
//                                                    maxCol);
//     case HANDWRITTEN:
//       return handwritten::getInterfaceTiles(aie_meta,
//                                             graphName,
//                                             portName,
//                                             metricStr,
//                                             channelId,
//                                             useColumn,
//                                             minCol,
//                                             maxCol);
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return {};
//     }
//   }

//   std::vector<tile_type>
//   getMemoryTiles(const boost::property_tree::ptree& aie_meta,
//                  const std::string& graphName,
//                  const std::string& bufferName,
//                  MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return compiler_report::getMemoryTiles(aie_meta, graphName, bufferName);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getMemoryTiles(aie_meta, graphName, bufferName);
//     case HANDWRITTEN:
//       return handwritten::getMemoryTiles(aie_meta, graphName, bufferName);
//     case UNKNOWN_FILE: // Fallthrough
//     default:
//       return {};
//     }
//   }

//   std::vector<tile_type>
//   getAIETiles(const boost::property_tree::ptree& aie_meta,
//               const std::string& graphName,
//               MetadataFileType type)
//   {
//     switch (type) {
//     case COMPILER_REPORT:
//       return compiler_report::getAIETiles(aie_meta, graphName);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getAIETiles(aie_meta, graphName);
//     case HANDWRITTEN:
//       return handwritten::getAIETiles(aie_meta, graphName);
//     case UNKNOWN_FILE: 
//     default:
//       return {};
//     }
//   }
  
//   std::vector<tile_type>
//   getEventTiles(const boost::property_tree::ptree& aie_meta,
//                 const std::string& graphName,
//                 module_type type,
//                 MetadataFileType t)
//   {
//     switch (t) {
//     case COMPILER_REPORT:
//       return compiler_report::getEventTiles(aie_meta, graphName, type);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getEventTiles(aie_meta, graphName, type);
//     case HANDWRITTEN:
//       return handwritten::getEventTiles(aie_meta, graphName, type);
//     case UNKNOWN_FILE: 
//     default:
//       return {};
//     }
//   }

//   std::vector<tile_type>
//   getTiles(const boost::property_tree::ptree& aie_meta,
//            const std::string& graphName,
//            module_type type,
//            const std::string& kernelName,
//            MetadataFileType t)
//   {
//     switch (t) {
//     case COMPILER_REPORT:
//       return compiler_report::getTiles(aie_meta, graphName, type, kernelName);
//     case AIE_CONTROL_CONFIG:
//       return aie_control_config::getTiles(aie_meta, graphName, type, kernelName);
//     case HANDWRITTEN:
//       return handwritten::getTiles(aie_meta, graphName, type, kernelName);
//     case UNKNOWN_FILE: 
//     default:
//       return {};
//     }
//   }

} // namespace xdp::aie
