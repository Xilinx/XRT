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

#define XDP_CORE_SOURCE

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

  /****************************************************************************
   * Compare two tiles (used for sorting)
   ***************************************************************************/
  bool tileCompare(xdp::tile_type tile1, xdp::tile_type tile2) 
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }

  /****************************************************************************
   * Throw runtime error (only if err is set)
   ***************************************************************************/
  void throwIfError(bool err, const char* msg)
  {
    if (err)
      throw std::runtime_error(msg);
  }

  /****************************************************************************
   * Determine type of metadata file
   ***************************************************************************/
  std::unique_ptr<xdp::aie::BaseFiletypeImpl>
  determineFileType(boost::property_tree::ptree& aie_project)
  {
    //
    // Check if it is the known compiler_report.json format
    //
    try {
      std::string schema;
      schema = aie_project.get_child("schema").get_value<std::string>();
      if (schema == "MEGraphSchema-0.4")
        return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
    }
    catch (...) {
      // Something went wrong, so it most likely is not a "compiler_report.json"
    }

    //
    // Check if it is the known aie_control_config.json format
    //
    try {
      std::string major;
      major = aie_project.get_child("schema_version.major").get_value<std::string>();
      if (major == "1")
        return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
    }
    catch(...) {
      // Something went wrong, so it most likely is not an aie_control_config
    }

    //
    // Check if it is the known handwritten format
    //
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

  /****************************************************************************
   * Get metadata required to configure driver
   ***************************************************************************/
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

    // For backward compatability, look for both the old and new fields
    bool found = false;
    try {
      config.mem_row_start =
        meta_config.get_child("mem_tile_row_start").get_value<uint8_t>();
      config.mem_num_rows =
        meta_config.get_child("mem_tile_num_rows").get_value<uint8_t>();
      found = true;
    }
    catch (std::exception& /*e*/) {
      // For older xclbins, it is not an error if we don't find the
      // mem_tile entries, so just catch the exception and ignore it.
    }
    if (!found) {
      config.mem_row_start =
        meta_config.get_child("reserved_row_start").get_value<uint8_t>();
      config.mem_num_rows =
        meta_config.get_child("reserved_num_rows").get_value<uint8_t>();
    }

    config.aie_tile_row_start =
      meta_config.get_child("aie_tile_row_start").get_value<uint8_t>();
    config.aie_tile_num_rows =
      meta_config.get_child("aie_tile_num_rows").get_value<uint8_t>();
    return config;
  }

  /****************************************************************************
   * Get first row offset of AIE tiles in array
   ***************************************************************************/
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

  /****************************************************************************
   * Get all valid graph names from metadata
   ***************************************************************************/
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

  /****************************************************************************
   * Read AIE metadata from axlf section
   ***************************************************************************/
  std::unique_ptr<xdp::aie::BaseFiletypeImpl>
  readAIEMetadata(const char* data, size_t size, pt::ptree& aie_project)
  {
    std::stringstream aie_stream;
    aie_stream.write(data,size);
    pt::read_json(aie_stream, aie_project);

    return determineFileType(aie_project);
  }

  /****************************************************************************
   * Read AIE metadata from file
   ***************************************************************************/
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

  /****************************************************************************
   * Check if verbosity is at least info level
   ***************************************************************************/
  bool isInfoVerbosity()
  {
    return (xrt_core::config::get_verbosity() >= 
            static_cast<uint32_t>(severity_level::info));
  }

  /****************************************************************************
   * Check if verbosity is at least debug level
   ***************************************************************************/
  bool isDebugVerbosity()
  {
    return (xrt_core::config::get_verbosity() >= 
            static_cast<uint32_t>(severity_level::debug));
  }

  /****************************************************************************
   * Check if input-based metric set
   ***************************************************************************/
  bool isInputSet(const module_type type, const std::string metricSet)
  {
    // Catch memory tile sets
    if (type == module_type::mem_tile) {
      if ((metricSet.find("input") != std::string::npos)
          || (metricSet.find("s2mm") != std::string::npos))
        return true;
      else
        return false;
    }

    // Remaining covers all other tile types (i.e., AIE, interface)
    if ((metricSet.find("input") != std::string::npos)
        || (metricSet.find("mm2s") != std::string::npos))
      return true;
    else
      return false;
  }

  /****************************************************************************
   * Get relative row of given tile
   ***************************************************************************/
  uint16_t getRelativeRow(uint16_t absRow, uint16_t rowOffset)
  {
    if (absRow == 0)
      return 0;
    if (absRow < rowOffset)
      return (absRow - 1);
    return (absRow - rowOffset);
  }

  /****************************************************************************
   * Get module type
   ***************************************************************************/
  module_type getModuleType(uint16_t absRow, uint16_t rowOffset)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < rowOffset)
      return module_type::mem_tile;
    return module_type::core;
  }

  /****************************************************************************
   * Convert broadcast ID to event ID
   ***************************************************************************/
  uint32_t bcIdToEvent(int bcId)
  {
    return bcId + CORE_BROADCAST_EVENT_BASE;
  }
  
  /****************************************************************************
   * Get module name
   ***************************************************************************/
  std::string getModuleName(module_type mod)
  {
    static std::map<module_type, std::string> modNames {
      {module_type::core,     "AIE modules"},
      {module_type::dma,      "AIE tile memory modules"},
      {module_type::shim,     "interface tiles"},
      {module_type::mem_tile, "memory tiles"}
    };

    return modNames[mod];
  }

} // namespace xdp::aie
