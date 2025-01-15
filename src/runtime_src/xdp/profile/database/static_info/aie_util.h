/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#include <string>

#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "filetypes/base_filetype_impl.h"

namespace xdp::aie {

  XDP_CORE_EXPORT
  bool 
  tileCompare(xdp::tile_type tile1, xdp::tile_type tile2); 

  XDP_CORE_EXPORT 
  void 
  throwIfError(bool err, const char* msg);

  // A function to read the JSON from an axlf section inside the xclbin and
  // return the type of the file
  XDP_CORE_EXPORT
  std::unique_ptr<BaseFiletypeImpl>
  readAIEMetadata(const char* data, size_t size,
                  boost::property_tree::ptree& aie_project);

  // A function to read the JSON from a file on disk and return the type of
  // the file
  XDP_CORE_EXPORT
  std::unique_ptr<BaseFiletypeImpl>
  readAIEMetadata(const char* filename,
                  boost::property_tree::ptree& aie_project);

  XDP_CORE_EXPORT
  int getHardwareGeneration(const boost::property_tree::ptree& aie_meta,
                            const std::string& root);

  XDP_CORE_EXPORT
  double getAIEClockFreqMHz(const boost::property_tree::ptree& aie_meta,
                            const std::string& root);

  XDP_CORE_EXPORT
  xdp::aie::driver_config
  getDriverConfig(const boost::property_tree::ptree& aie_meta,
                  const std::string& root);
  
  XDP_CORE_EXPORT
  uint8_t
  getAIETileRowOffset(const boost::property_tree::ptree& aie_meta,
                      const std::string& location);
  
  XDP_CORE_EXPORT
  std::vector<std::string>
  getValidGraphs(const boost::property_tree::ptree& aie_meta,
                 const std::string& root);

  XDP_CORE_EXPORT
  bool isInfoVerbosity();

  XDP_CORE_EXPORT
  bool isDebugVerbosity();

  XDP_CORE_EXPORT
  bool isInputSet(const module_type type, const std::string metricSet);
  
  XDP_CORE_EXPORT
  uint8_t getRelativeRow(uint8_t absRow, uint8_t rowOffset);

  XDP_CORE_EXPORT
  std::string getRelativeRowStr(uint8_t absRow, uint8_t rowOffset);
  
  XDP_CORE_EXPORT
  module_type getModuleType(uint8_t absRow, uint8_t rowOffset);
  
  XDP_CORE_EXPORT
  uint32_t bcIdToEvent(int bcId);
  
  XDP_CORE_EXPORT
  std::string getModuleName(module_type mod);

  XDP_CORE_EXPORT
  uint8_t convertStringToUint8(const std::string& input);

  XDP_CORE_EXPORT
  std::string uint8ToStr(const uint8_t& value);

  XDP_CORE_EXPORT
  bool isDigitString(const std::string& str);

  XDP_CORE_EXPORT
  boost::property_tree::ptree
  getAIEPartitionInfoClient(void* hwCtxImpl);

  XDP_CORE_EXPORT
  boost::property_tree::ptree
  getAIEPartitionInfo(void* handle);

  XDP_CORE_EXPORT
  void displayColShiftInfo(uint8_t colShift);

  XDP_CORE_EXPORT
  uint32_t getStreamWidth(uint8_t hw_gen);

} // namespace xdp::aie

#endif
