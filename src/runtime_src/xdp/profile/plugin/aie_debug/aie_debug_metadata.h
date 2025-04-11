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

#ifndef AIE_DEBUG_METADATA_H
#define AIE_DEBUG_METADATA_H

#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <vector>
#include <set>
#include <optional>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/plugin/aie_debug/used_registers.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

#define NUMBEROFMODULES 4

namespace xdp {

// Forward declarations
class BaseReadableTile;

class AieDebugMetadata {
  public:
    AieDebugMetadata(uint64_t deviceID, void* handle);

    void parseMetrics();

    module_type getModuleType(int mod) {return moduleTypes[mod];}
    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}

    std::map<tile_type, std::string> getConfigMetrics(const int module) {
      return configMetrics[module];
    }
    std::vector<std::pair<tile_type, std::string>> getConfigMetricsVec(const int module) {
      return {configMetrics[module].begin(), configMetrics[module].end()};
    }

    std::map<module_type, std::vector<uint64_t>>& getRegisterValues() {
      return parsedRegValues;
    }

    bool aieMetadataEmpty() const {return (metadataReader == nullptr);}
    xdp::aie::driver_config getAIEConfigMetadata() {return metadataReader->getDriverConfig();}

    uint8_t getAIETileRowOffset() const {
      return (metadataReader == nullptr) ? 0 : metadataReader->getAIETileRowOffset();
    }
    int getHardwareGen() const {
      return (metadataReader == nullptr) ? 0 : metadataReader->getHardwareGeneration();
    }

    int getNumModules() {return NUM_MODULES;}
    xrt::hw_context getHwContext() {return hwContext;}
    void setHwContext(xrt::hw_context c) {
      hwContext = std::move(c);
    }

    std::string lookupRegisterName(uint64_t regVal, module_type mod);
    std::optional<uint64_t> lookupRegisterAddr(const std::string& regName, module_type mod);
    //std::map<uint64_t, uint32_t> lookupRegisterSizes();
    uint32_t lookupRegisterSizes(uint64_t regVal,module_type mod);

  private:
    std::vector<uint64_t> stringToRegList(std::string stringEntry, module_type t);
    std::vector<std::string> getSettingsVector(std::string settingsString);

  private:
    // Currently supporting Core, Memory, Interface Tiles, and Memory Tiles
    static constexpr int NUM_MODULES = 4;
    const std::vector<std::string> moduleNames =
      {"aie", "aie_memory", "interface_tile", "memory_tile"};
    const module_type moduleTypes[NUM_MODULES] =
      {module_type::core, module_type::dma, module_type::shim, module_type::mem_tile};

    void* handle;
    uint64_t deviceID;
    xrt::hw_context hwContext;
    std::vector<std::map<tile_type, std::string>> configMetrics;
    std::map<module_type, std::vector<uint64_t>> parsedRegValues;
    const aie::BaseFiletypeImpl* metadataReader = nullptr;

    // List of AIE HW generation-specific registers
    std::unique_ptr<UsedRegisters> usedRegisters;
};

/*****************************************************************
The BaseReadableTile is created to simplify the retrieving of value at
each tile. This class encapsulates all the data (row, col, list of registers
to read) pertaining to a particuar tile, for easy extraction of tile by tile data.
****************************************************************** */
class BaseReadableTile {
  public:
    //virtual void readValues(XAie_DevInst* aieDevInst, std::map<uint64_t, uint32_t>* lookupRegAddrToSizeMap)=0;
    virtual void readValues(XAie_DevInst* aieDevInst, std::shared_ptr<AieDebugMetadata> metadata)=0;

    void setTileOffset(uint64_t offset) {tileOffset = offset;}
    void addOffsetName(uint64_t rel, std::string name,module_type mod) {
      switch (mod){
        case module_type::core : {
          coreRelativeOffsets.push_back(rel);
          coreRegisterNames.push_back(name);
        }
        break;
        case module_type::dma : {
          memoryRelativeOffsets.push_back(rel);
          memoryRegisterNames.push_back(name);
        }
        break;
        case module_type::shim : {
          shimRelativeOffsets.push_back(rel);
          shimRegisterNames.push_back(name);
        }
        break;
        case module_type::mem_tile : {
          memTileRelativeOffsets.push_back(rel);
          memTileRegisterNames.push_back(name);
        }
        break;
        default: break;
      }
    }

    void printValues(uint32_t deviceID, VPDatabase* db) {
      int i = 0;
      std::vector<uint64_t>* addrVectors[] = {&coreRelativeOffsets, &memoryRelativeOffsets, &shimRelativeOffsets, &memTileRelativeOffsets};
      std::vector<xdp::aie::AieDebugValue>* valueVectors[] = {&coreValues, &memoryValues, &shimValues, &memTileValues};
      std::vector<std::string>* nameVectors[]={&coreRegisterNames,&memoryRegisterNames,&shimRegisterNames,&memTileRegisterNames};
      /*
      for (auto& offset : relativeOffsets) {
        db->getDynamicInfo().addAIEDebugSample(deviceID, col, row, values[i],
                                               offset, registerNames[i]);
        i++;
      }
      */
     for (int i = 0; i < NUMBEROFMODULES; ++i) {
        for (int j = 0; j < addrVectors[i]->size(); ++j) {
          db->getDynamicInfo().addAIEDebugSample(deviceID, col, row, (*valueVectors[i])[j],
                                               (*addrVectors[i])[j], (*nameVectors[i])[j]);
        }
     }
    }

  public:
    uint8_t col;
    uint8_t row;
    uint64_t tileOffset;
    std::vector<xdp::aie::AieDebugValue> coreValues;
    std::vector<xdp::aie::AieDebugValue> memoryValues;
    std::vector<xdp::aie::AieDebugValue> shimValues;
    std::vector<xdp::aie::AieDebugValue> memTileValues;
    //std::vector<uint64_t> relativeOffsets;
    std::vector<uint64_t> coreRelativeOffsets;
    std::vector<uint64_t> memoryRelativeOffsets;
    std::vector<uint64_t> shimRelativeOffsets;
    std::vector<uint64_t> memTileRelativeOffsets;
    //std::vector<std::string> registerNames;
    std::vector<std::string> coreRegisterNames;
    std::vector<std::string> memoryRegisterNames;
    std::vector<std::string> shimRegisterNames;
    std::vector<std::string> memTileRegisterNames;
};

} // end XDP namespace

#endif
