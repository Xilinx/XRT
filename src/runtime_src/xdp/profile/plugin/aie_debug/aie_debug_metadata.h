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

#ifndef AIE_DEBUG_METADATA_H
#define AIE_DEBUG_METADATA_H

#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <vector>
#include <set>

#include "core/common/device.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"

namespace xdp {

class AieDebugMetadata {
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
    const aie::BaseFiletypeImpl* metadataReader = nullptr;

  public:
    AieDebugMetadata(uint64_t deviceID, void* handle);

    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}
    
    std::map<tile_type, std::string> getConfigMetrics(const int module) {return configMetrics[module];}
    xdp::aie::driver_config getAIEConfigMetadata();

    uint8_t getAIETileRowOffset() const {return (metadataReader == nullptr) ? 0 : metadataReader->getAIETileRowOffset(); }
    int getHardwareGen() const {return (metadataReader == nullptr) ? 0 : metadataReader->getHardwareGeneration(); }

    int getNumModules() {return NUM_MODULES;}
    xrt::hw_context getHwContext(){return hwContext;}
    void setHwContext(xrt::hw_context c) {
      hwContext = std::move(c);
    }

    const AIEProfileFinalConfig& getAIEProfileConfig() const ;
};

} // end XDP namespace

#endif
