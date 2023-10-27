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

#ifndef AIE_UTIL_DOT_H
#define AIE_UTIL_DOT_H

#include <boost/property_tree/ptree.hpp>
#include <cstdint>
#include <map>
#include <vector>

#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp::aie {

  // The file in AIE_METADATA can be one of several different JSON
  // file types.  We have to get the same information from all of them.
  enum MetadataFileType {
    COMPILER_REPORT    = 0,
    AIE_CONTROL_CONFIG = 1,
    HANDWRITTEN        = 2,
    UNKNOWN_FILE       = 3
  };

  // A function to read the JSON from an axlf section inside the xclbin and
  // return the type of the file
  XDP_EXPORT
  MetadataFileType
  readAIEMetadata(const char* data, size_t size,
                  boost::property_tree::ptree& aie_project);

  // A function to read the JSON from a file on disk and return the type of
  // the file
  XDP_EXPORT
  MetadataFileType
  readAIEMetadata(const char* filename,
                  boost::property_tree::ptree& aie_project);

  // Top level interface used for both file type formats
  XDP_EXPORT
  driver_config
  getDriverConfig(const boost::property_tree::ptree& aie_meta,
                  MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  int getHardwareGeneration(const boost::property_tree::ptree& aie_meta,
                            MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  aiecompiler_options
  getAIECompilerOptions(const boost::property_tree::ptree& aie_meta,
                        MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  uint16_t getAIETileRowOffset(const boost::property_tree::ptree& aie_meta,
                               MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  std::vector<std::string>
  getValidGraphs(const boost::property_tree::ptree& aie_meta,
                 MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  std::vector<std::string>
  getValidPorts(const boost::property_tree::ptree& aie_meta,
                MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  std::vector<std::string>
  getValidKernels(const boost::property_tree::ptree& aie_meta,
                  MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  std::unordered_map<std::string, io_config>
  getTraceGMIOs(const boost::property_tree::ptree& aie_meta,
                MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  std::vector<tile_type>
  getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
                    const std::string& graphName,
                    const std::string& portName = "all",
                    const std::string& metricStr = "channels",
                    MetadataFileType type = AIE_CONTROL_CONFIG,
                    int16_t channelId = -1,
                    bool useColumn = false, 
                    uint32_t minCol = 0, 
                    uint32_t maxCol = 0);

  XDP_EXPORT
  std::vector<tile_type>
  getMemoryTiles(const boost::property_tree::ptree& aie_meta, 
                 const std::string& graphName,
                 const std::string& bufferName = "all",
                 MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  std::vector<tile_type>
  getAIETiles(const boost::property_tree::ptree& aie_meta,
              const std::string& graphName,
              MetadataFileType type = AIE_CONTROL_CONFIG);

  XDP_EXPORT
  std::vector<tile_type>
  getEventTiles(const boost::property_tree::ptree& aie_meta, 
                const std::string& graph_name,
                module_type type,
                MetadataFileType t = AIE_CONTROL_CONFIG);


  XDP_EXPORT
  std::vector<tile_type>
  getTiles(const boost::property_tree::ptree& aie_meta, 
           const std::string& graph_name,
           module_type type, 
           const std::string& kernel_name = "all",
           MetadataFileType t = AIE_CONTROL_CONFIG);
} // namespace xdp::aie

#endif
