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
#include "filetypes/base_filetype_impl.h"

namespace xdp::aie {

  XDP_EXPORT
  bool 
  tileCompare(xdp::tile_type tile1, xdp::tile_type tile2); 

  XDP_EXPORT 
  void 
  throwIfError(bool err, const char* msg);

  // A function to read the JSON from an axlf section inside the xclbin and
  // return the type of the file
  XDP_EXPORT
  std::unique_ptr<BaseFiletypeImpl>
  readAIEMetadata(const char* data, size_t size,
                  boost::property_tree::ptree& aie_project);

  // A function to read the JSON from a file on disk and return the type of
  // the file
  XDP_EXPORT
  std::unique_ptr<BaseFiletypeImpl>
  readAIEMetadata(const char* filename,
                  boost::property_tree::ptree& aie_project);

  XDP_EXPORT
  int getHardwareGeneration(const boost::property_tree::ptree& aie_meta,
                          const std::string& root);

  XDP_EXPORT
  xdp::aie::driver_config
  getDriverConfig(const boost::property_tree::ptree& aie_meta,
                const std::string& root);

  XDP_EXPORT
  uint16_t
  getAIETileRowOffset(const boost::property_tree::ptree& aie_meta,
                    const std::string& location);
  
  XDP_EXPORT
  std::vector<std::string>
  getValidGraphs(const boost::property_tree::ptree& aie_meta,
                const std::string& root);

  XDP_EXPORT
  bool isInfoVerbosity();

  XDP_EXPORT
  bool isDebugVerbosity();

  XDP_EXPORT
  bool isInputSet(const module_type type, const std::string metricSet);
  
  XDP_EXPORT
  uint16_t getRelativeRow(uint16_t absRow, uint16_t rowOffset);
  
  XDP_EXPORT
  module_type getModuleType(uint16_t absRow, uint16_t rowOffset);
  
  XDP_EXPORT
  uint32_t bcIdToEvent(int bcId);
  
  XDP_EXPORT
  std::string getModuleName(module_type mod);

} // namespace xdp::aie

#endif
