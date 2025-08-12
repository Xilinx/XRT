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

#define XDP_PLUGIN_SOURCE

#include "aie_debug_metadata.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  namespace pt = boost::property_tree;

  /****************************************************************************
   * Constructor
   ***************************************************************************/
  AieDebugMetadata::AieDebugMetadata(uint64_t deviceID, void* handle) :
      deviceID(deviceID), handle(handle)
  {
    VPDatabase* db = VPDatabase::Instance();

    metadataReader = (db->getStaticInfo()).getAIEmetadataReader();
    if (!metadataReader)
      return;

    // Record all tiles for debugging
    for (int module = 0; module < NUM_MODULES; ++module) {
      auto type = moduleTypes[module];

      std::vector<tile_type> tiles = (type == module_type::shim) 
        ? metadataReader->getInterfaceTiles("all", "all", "input_output")
        : metadataReader->getTiles("all", type, "all");

      std::map<tile_type, std::string> moduleMap;
      for (auto& t : tiles)
        moduleMap[t] = "aie_debug";
      configMetrics.push_back(std::move(moduleMap));
    }

    // Get generation-specific register locations
    auto hwGen = getHardwareGen();
    if (hwGen == 1) {
      usedRegisters = std::make_unique<AIE1UsedRegisters>();
    }
    else if (hwGen == 5) {
      usedRegisters = std::make_unique<AIE2psUsedRegisters>();
    }
    else if ((hwGen > 1) && (hwGen < 10)) {
      usedRegisters = std::make_unique<AIE2UsedRegisters>();
    }
  }

  /****************************************************************************
   * Lookup register names and values given current AIE HW generation
   ***************************************************************************/
  std::string AieDebugMetadata::lookupRegisterName(uint64_t regVal) 
  {
    return usedRegisters->getRegisterName(regVal);
  }

  uint64_t AieDebugMetadata::lookupRegisterAddr(std::string regName) 
  {
    return usedRegisters->getRegisterAddr(regName);
  }

  /****************************************************************************
   * Convert xrt.ini setting to vector
   ***************************************************************************/
  std::vector<std::string>
  AieDebugMetadata::getSettingsVector(std::string settingsString)
  {
    if (settingsString.empty())
      return {};

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;
    boost::replace_all(settingsString, " ", "");
    boost::split(settingsVector, settingsString, boost::is_any_of(","));
    return settingsVector;
  }

  /****************************************************************************
   * Convert each xrt.ini entry to actual list of registers
  ***************************************************************************/
  std::vector<uint64_t>
  AieDebugMetadata::stringToRegList(std::string stringEntry, module_type mod)
  {
    std::vector<uint64_t> listofRegisters;

    /****************************************************************************
     AIE debug settings metrics can be entered in the following 3 ways:
     [AIE_debug_settings]
     # Very flexible but need to know specific reg values
     core_registers = 0x12345, 0x34567
     # Simplified but not flexible
     core_registers = trace_config, profile_config, all
     # Specific registers but hides gen-specific values
     core_registers = cm_core_status, mm_trace_status
    /************************************************************************* */
    if (stringEntry.rfind("0x", 0) == 0) {
      // Specific register addresses start with "0x"
      uint64_t val = stoul(stringEntry, nullptr, 16);
      listofRegisters.push_back(val);
      return listofRegisters;
    }
    else if (stringEntry == "trace_config") {
      usedRegisters->populateTraceRegisters();
    }
    else if (stringEntry == "profile_config") {
      usedRegisters->populateProfileRegisters();
    }
    else if (stringEntry == "all") {
      usedRegisters->populateAllRegisters();
    }
    else {
      // Find specific register names
      uint64_t tmpRedAddr = lookupRegisterAddr(stringEntry);
      if (tmpRedAddr != -1) {
        listofRegisters.push_back(tmpRedAddr);
      }
      else {
        xrt_core::message::send(severity_level::warning, "XRT", "Unable to parse AIE debug metric settings. " 
          "Please enter register addresses, names, or trace_config|profile_config|all.");
      }
      return listofRegisters;
    }

    if (mod == module_type::core) {
      auto coreAddressList = usedRegisters->getCoreAddresses();
      listofRegisters.insert(listofRegisters.end(), coreAddressList.begin(),
                             coreAddressList.end() );
    }
    else if (mod == module_type::dma) {
      auto memoryAddressList = usedRegisters->getMemoryAddresses();
      listofRegisters.insert(listofRegisters.end(), memoryAddressList.begin(),
                             memoryAddressList.end() );
    }
    else if (mod == module_type::shim) {
      auto interfaceAddressList = usedRegisters->getInterfaceAddresses();
      listofRegisters.insert(listofRegisters.end(), interfaceAddressList.begin(),
                             interfaceAddressList.end() );
    }
    else if (mod == module_type::mem_tile) {
      auto memoryTileAddressList = usedRegisters->getMemoryTileAddresses();
      listofRegisters.insert(listofRegisters.end(), memoryTileAddressList.begin(),
                             memoryTileAddressList.end() );
    }
    else if (mod == module_type::uc) {
      xrt_core::message::send(severity_level::debug, "XRT", "Debugging microcontroller registers not supported yet");
    }

    return listofRegisters;
  }

  /****************************************************************************
   * Parse AIE metrics
   ***************************************************************************/
  void AieDebugMetadata::parseMetrics()
  {
    parsedRegValues = {
      {module_type::core,     {}},
      {module_type::dma,      {}},
      {module_type::shim,     {}},
      {module_type::mem_tile, {}}
    };
    
    unsigned int module = 0;
    std::vector<std::string> metricsConfig;
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_core_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_memory_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_interface_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_memory_tile_registers());

    // Parse metric settings from xrt.ini file
    for (auto const type : moduleTypes) {
      std::vector<std::string> metricsSettings = getSettingsVector(metricsConfig[module++]);

      for (auto& setting : metricsSettings) {
        try {
          std::vector<uint64_t> regValList = stringToRegList(setting, type);
          for (auto val : regValList)
            parsedRegValues[type].push_back(val);
        } catch (...) {
          xrt_core::message::send(severity_level::warning, "XRT", "Unable to parse: " 
            + setting + ". Debug setting will be ignored.");
        }
      }
    }
  }

}  // namespace xdp
