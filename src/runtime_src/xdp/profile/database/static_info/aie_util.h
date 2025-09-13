// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

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
  getNumRows(const boost::property_tree::ptree& aie_meta,
            const std::string& location);

  XDP_CORE_EXPORT
  uint8_t
  getAIETileRowOffset(const boost::property_tree::ptree& aie_meta,
                      const std::string& location);
  
  XDP_CORE_EXPORT
  bool isInfoVerbosity();

  XDP_CORE_EXPORT
  bool isDebugVerbosity();

  XDP_CORE_EXPORT
  bool isInputSet(const module_type type, const std::string metricSet);
  
  XDP_CORE_EXPORT
  bool isDmaSet(const std::string metricSet);

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
  getAIEPartitionInfo(void* handle, bool isHwCtxImpl = true);

  XDP_CORE_EXPORT
  void displayColShiftInfo(uint8_t colShift);

  XDP_CORE_EXPORT
  std::string getGraphUniqueId(io_config& ioc);

} // namespace xdp::aie

#endif
