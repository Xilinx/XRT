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
