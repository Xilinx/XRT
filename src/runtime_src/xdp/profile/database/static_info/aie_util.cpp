/**
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "aie_util.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/query.h"
#include "core/common/query_requests.h"
#include "filetypes/aie_control_config_filetype.h"
#include "filetypes/aie_trace_config_filetype.h"

#include "core/common/api/xclbin_int.h"
#include "core/include/xrt/detail/xclbin.h"

#include <algorithm>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cctype> 
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>

// ***************************************************************
// Anonymous namespace for helper functions local to this file
// ***************************************************************
namespace xdp::aie {
    
  namespace pt = boost::property_tree;
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Compare two tiles (used for sorting)
   ***************************************************************************/
  bool 
  tileCompare(xdp::tile_type tile1, xdp::tile_type tile2) 
  {
    return ((tile1.col == tile2.col) && (tile1.row == tile2.row));
  }

  /****************************************************************************
   * Throw runtime error (only if err is set)
   ***************************************************************************/
  void
  throwIfError(bool err, const char* msg)
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
    // aie_trace_config.json format
    try {
      int majorVersion = aie_project.get("schema_version.major", 1);
      if (majorVersion == 2)
        return std::make_unique<xdp::aie::AIETraceConfigFiletype>(aie_project);
    }
    catch(...) {
      // Most likely not an aie_trace_config
    }

    // aie_control_config.json format
    try {
      auto c = aie_project.get_child_optional("aie_metadata.aiecompiler_options");
      if (c)
        return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
    }
    catch(...) {
      // Most likely not an aie_control_config
    }

    try {
      auto c = aie_project.get_child_optional("schema");
      if (c) {
        auto schema = c.get().get_value<std::string>();
        // compiler_report.json format
        if (schema == "MEGraphSchema-0.4")
          return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
        // Known handwritten format
        if (schema == "handwritten")
          return std::make_unique<xdp::aie::AIEControlConfigFiletype>(aie_project);
      }
    }
    catch (...) {
      // Most likely an invalid format
    }

    std::stringstream msg;
    msg << "Unable to determine AIE Metadata file type. "
        << "Profiling and trace features might not work.";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());

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

  // On Edge devices, AIE clock frequency shouldn't change once execution has started.
  // On Client devices, this static information from metadata may not be correct.
  double getAIEClockFreqMHz(const boost::property_tree::ptree& aie_meta,
                            const std::string& root)
  {
    static std::optional<double> clockFreqMHz;
    if (!clockFreqMHz.has_value()) {
      clockFreqMHz = aie_meta.get_child(root).get_value<double>();
    }
    return *clockFreqMHz;
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

  uint8_t
  getNumRows(const boost::property_tree::ptree& aie_meta,
            const std::string& location)
  {
    static std::optional<uint8_t> numRows;
    if (!numRows.has_value()) {
      numRows = aie_meta.get_child(location).get_value<uint8_t>();
    }
    return *numRows;
  }

  /****************************************************************************
   * Get first row offset of AIE tiles in array
   ***************************************************************************/
  uint8_t
  getAIETileRowOffset(const boost::property_tree::ptree& aie_meta,
                    const std::string& location)
  {
    static std::optional<uint8_t> rowOffset;
    if (!rowOffset.has_value()) {
      rowOffset = aie_meta.get_child(location).get_value<uint8_t>();
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
    try {
      pt::read_json(aie_stream, aie_project);
    } catch (const std::exception& e) {
      std::string msg("AIE Metadata could not be read : ");
      msg += e.what();
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return nullptr;
    }

    return determineFileType(aie_project);
  }

  /****************************************************************************
   * Read AIE metadata from file
   ***************************************************************************/
  std::unique_ptr<xdp::aie::BaseFiletypeImpl>
  readAIEMetadata(const char* filename, pt::ptree& aie_project)
  {
    if (!std::filesystem::exists(filename)) {
      std::stringstream msg;
      msg << "The AIE metadata JSON file is required in the same directory"
          << " as the run directory to run AIE Profile.";
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return nullptr;
    }

    try {
      pt::read_json(filename, aie_project);
    }
    catch(const std::exception& e)
    {
      std::stringstream msg;
      msg << "Exception occurred while reading the aie_control_config.json: "<< std::string(e.what()) ;
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return nullptr;
    }

    return determineFileType(aie_project);
  }

  /****************************************************************************
   * Check if verbosity is at least info level
   ***************************************************************************/
  bool 
  isInfoVerbosity()
  {
    return (xrt_core::config::get_verbosity() >= 
            static_cast<uint32_t>(severity_level::info));
  }

  /****************************************************************************
   * Check if verbosity is at least debug level
   ***************************************************************************/
  bool 
  isDebugVerbosity()
  {
    return (xrt_core::config::get_verbosity() >= 
            static_cast<uint32_t>(severity_level::debug));
  }

  /****************************************************************************
   * Check if input-based metric set
   ***************************************************************************/
  bool 
  isInputSet(const module_type type, const std::string metricSet)
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
  uint8_t 
  getRelativeRow(uint8_t absRow, uint8_t rowOffset)
  {
    if (absRow == 0)
      return 0;
    if (absRow < rowOffset)
      return (absRow - 1);
    return (absRow - rowOffset);
  }

  /****************************************************************************
   * Get string representation of relative row of given tile
   ***************************************************************************/
  std::string
  getRelativeRowStr(uint8_t absRow, uint8_t rowOffset)
  {
    uint8_t relativeRow = aie::getRelativeRow(absRow, rowOffset);

    return std::to_string(+relativeRow);
  }

  /****************************************************************************
   * Get module type
   ***************************************************************************/
  module_type 
  getModuleType(uint8_t absRow, uint8_t rowOffset)
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
  uint32_t 
  bcIdToEvent(int bcId)
  {
    return bcId + CORE_BROADCAST_EVENT_BASE;
  }
  
  /****************************************************************************
   * Get module name
   ***************************************************************************/
  std::string 
  getModuleName(module_type mod)
  {
    static std::map<module_type, std::string> modNames {
      {module_type::core,     "AIE modules"},
      {module_type::dma,      "AIE tile memory modules"},
      {module_type::shim,     "interface tiles"},
      {module_type::mem_tile, "memory tiles"}
    };

    return modNames[mod];
  }

  /****************************************************************************
   * Convert string to uint8
   ***************************************************************************/
  uint8_t
  convertStringToUint8(const std::string& input) {
    return static_cast<uint8_t>(std::stoi(input));
  }

  std::string
  uint8ToStr(const uint8_t& value) {
    return std::to_string(static_cast<int>(value));
  }

  bool isDigitString(const std::string& str)
  {
    return std::all_of(str.begin(), str.end(), ::isdigit);
  }

  /****************************************************************************
   * Get AIE partition information
   ****************************************************************************/

  boost::property_tree::ptree
  getAIEPartitionInfo(void* handle, bool isHwCtxImpl)
  {
    boost::property_tree::ptree infoPt;
    std::shared_ptr<xrt_core::device> device;
    try {
      if (isHwCtxImpl) {
        xrt::hw_context context = xrt_core::hw_context_int::create_hw_context_from_implementation(handle);
        device = xrt_core::hw_context_int::get_core_device(context);
      } else {
        device = xrt_core::get_userpf_device(handle);
      }
  
      auto info = xrt_core::device_query_default<xrt_core::query::aie_partition_info>(device.get(), {});
      for(const auto& e : info) {
        boost::property_tree::ptree pt;
        pt.put("start_col", e.start_col);
        pt.put("num_cols", e.num_cols);
        infoPt.push_back(std::make_pair("", pt));
      }
    }
    catch(...) {
      xrt_core::message::send(severity_level::info, "XRT", "Could not retrieve AIE Partition Info.");
      return infoPt;
    }
    return infoPt;
  }

  void displayColShiftInfo(uint8_t colShift)
  {
    static bool displayed = false;
    if (colShift>0 && !displayed) {
      std::stringstream msg;
      msg << "Partition start column shift of " << +colShift << " was found."
          << " Tile locations are adjusted by this column shift.";
      xrt_core::message::send(severity_level::info, "XRT", msg.str());
      displayed = true;
    }
  }

  /****************************************************************************
   * Get the stream width in bits for specified hw_gen
   ***************************************************************************/
  uint32_t getStreamWidth(uint8_t hw_gen)
  {
    // Stream width in bytes
    static const std::unordered_map<uint8_t, uint8_t> streamWidthMap = {
      {static_cast<uint8_t>(XDP_DEV_GEN_AIE),     static_cast<uint8_t>(4)},
      {static_cast<uint8_t>(XDP_DEV_GEN_AIEML),   static_cast<uint8_t>(4)}
    };
    uint32_t default_width = 32;
    
    if (streamWidthMap.find(hw_gen) == streamWidthMap.end())
      return default_width;
    
    return streamWidthMap.at(hw_gen);
  }

} // namespace xdp::aie
