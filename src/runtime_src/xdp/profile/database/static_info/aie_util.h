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
#include <set>
#include <map>
#include <vector>

#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "core/common/device.h"

namespace xdp {
namespace aie {
  bool tileCompare(tile_type tile1, tile_type tile2);
  inline void throwIfError(bool err, const char* msg);
  void readAIEMetadata(const char* data, size_t size, 
                       boost::property_tree::ptree& aie_project);

  int getHardwareGeneration(const boost::property_tree::ptree& aie_meta);
  uint16_t getAIETileRowOffset(const boost::property_tree::ptree& aie_meta);
  aiecompiler_options getAIECompilerOptions(const boost::property_tree::ptree& aie_meta);
  std::vector<std::string> getValidGraphs(const boost::property_tree::ptree& aie_meta);
  std::vector<std::string> getValidKernels(const boost::property_tree::ptree& aie_meta);
  std::vector<std::string> getValidPorts(const boost::property_tree::ptree& aie_meta);

  std::unordered_map<std::string, io_config> getPLIOs(const boost::property_tree::ptree& aie_meta);
  std::unordered_map<std::string, io_config> getGMIOs(const boost::property_tree::ptree& aie_meta);
  std::unordered_map<std::string, io_config> getTraceGMIOs(const boost::property_tree::ptree& aie_meta);
  std::unordered_map<std::string, io_config> getChildGMIOs(const boost::property_tree::ptree& aie_meta,
                                                           const std::string& childStr);
  std::unordered_map<std::string, io_config> getAllIOs(const boost::property_tree::ptree& aie_meta);
  std::vector<tile_type> getInterfaceTiles(const boost::property_tree::ptree& aie_meta,
                                           const std::string& graphName,
                                           const std::string& portName = "all",
                                           const std::string& metricStr = "channels",
                                           int16_t channelId = -1,
                                           bool useColumn = false, 
                                           uint32_t minCol = 0, 
                                           uint32_t maxCol = 0);

  std::vector<tile_type> getMemoryTiles(const boost::property_tree::ptree& aie_meta, 
                                        const std::string& graph_name,
                                        const std::string& kernel_name = "all");

  std::vector<tile_type> getAIETiles(const boost::property_tree::ptree& aie_meta,
                                     const std::string& graph_name);
  std::vector<tile_type> getEventTiles(const boost::property_tree::ptree& aie_meta, 
                                       const std::string& graph_name,
                                       module_type type);
  std::vector<tile_type> getTiles(const boost::property_tree::ptree& aie_meta, 
                                  const std::string& graph_name,
                                  module_type type, 
                                  const std::string& kernel_name = "all");
} // namespace aie
} // namespace xdp

#endif
