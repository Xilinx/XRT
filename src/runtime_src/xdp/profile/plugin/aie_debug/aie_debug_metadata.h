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

#ifndef AIE_DEBUG_METADATA_H
#define AIE_DEBUG_METADATA_H

#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <vector>
#include <set>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_debug/generations/aie1_registers.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2_registers.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2ps_registers.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

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

    module_type getModuleType(int mod) {return moduleTypes[mod];}
    uint64_t getDeviceID() {return deviceID;}
    void* getHandle() {return handle;}

    std::map<tile_type, std::string> getConfigMetrics(const int module) {
      return configMetrics[module];
    }
    std::vector<std::pair<tile_type, std::string>> getConfigMetricsVec(const int module) {
      return {configMetrics[module].begin(), configMetrics[module].end()};
    }
    xdp::aie::driver_config getAIEConfigMetadata();

    bool aieMetadataEmpty() const {return (metadataReader == nullptr);}
    uint8_t getAIETileRowOffset() const {return (metadataReader == nullptr) ? 0 : metadataReader->getAIETileRowOffset();}
    int getHardwareGen() const {return (metadataReader == nullptr) ? 0 : metadataReader->getHardwareGeneration();}

    int getNumModules() {return NUM_MODULES;}
    xrt::hw_context getHwContext() {return hwContext;}
    void setHwContext(xrt::hw_context c) {
      hwContext = std::move(c);
    }

    const AIEProfileFinalConfig& getAIEProfileConfig() const ;
};

class BaseReadableTile {
  public:
    std::vector <uint32_t> values;
    int col;
    int row;
    std::vector<uint64_t> relativeOffsets;
    std::vector<uint64_t> absoluteOffsets;

    virtual void readValues(XAie_DevInst* aieDevInst)=0;
    //virtual void readValues(){}

    void insertOffsets(uint64_t rel, uint64_t ab) {
      relativeOffsets.push_back(rel);
      absoluteOffsets.push_back(ab);
    }


    void printValues( uint32_t deviceID){
      int i=0;
      for (auto& absoluteOffset : absoluteOffsets){
        //std::cout<< "Debug tile (" << col << ", " << row << ") "
        //<< "hex address/values: 0x" << std::hex << absoluteOffset << " "
        //<< values[i++] << std::endl;
        /*
        std::stringstream msg;
        msg << "!!! Debug tile (" << col << ", " << row << ") "
            << "hex address/values: 0x" << std::hex << absoluteOffset << " : "
            << values[i++] << std::dec;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
        */
        db->getDynamicInfo().addAIEDebugSample(deviceId, col,row,absoluteOffset,relativeOffsets[i],values[i]);
        i++;
      }
    }
  };

  class UsedRegisters {

  public:

  std::vector<uint64_t> core_addresses;
  std::vector<uint64_t> interface_addresses ;
  std::vector<uint64_t> memory_addresses;
  std::vector<uint64_t> memory_tile_addresses;
  std::map<std::string, uint64_t> regNametovalues;
  std::map<uint64_t, std::string> regValueToName;
    virtual void populateProfileRegisters()=0;
    virtual void populateTraceRegisters()=0;
    virtual void populateRegNameToValueMap()=0;
    virtual void populateRegValueToNameMap()=0;
    void populateAllRegisters() {
      populateProfileRegisters();
      populateTraceRegisters();
    }
};

class AIE1UsedRegisters : public UsedRegisters {
 public:
  void populateProfileRegisters(){
 //populate the correct usedregisters
    std::vector<uint64_t> profile_core_addresses={0x00031020,0x00031024,0x00031028,0x0003102c};
    std::vector<uint64_t> profile_interface_addresses={0x0003ff00,0x0003ff04};
    std::vector<uint64_t> profile_memory_addresses={0x00011000,0x00011008};
    std::vector<uint64_t> profile_memory_tile_addresses={0x0};
    core_addresses.insert(std::end(core_addresses), std::begin(profile_core_addresses), std::end(profile_core_addresses));
    interface_addresses.insert(std::end(interface_addresses), std::begin(profile_interface_addresses), std::end(profile_interface_addresses));
    memory_addresses.insert(std::end(memory_addresses), std::begin(profile_memory_addresses), std::end(profile_memory_addresses));
    memory_tile_addresses.insert(std::end(memory_tile_addresses), std::begin(profile_memory_tile_addresses), std::end(profile_memory_tile_addresses));
  }
  void populateTraceRegisters(){
 //populate the correct usedregisters
    std::vector<uint64_t> trace_core_addresses={0x00034500,0x00034504};
    std::vector<uint64_t> trace_interface_addresses={0x0003ff00,0x0003ff04};
    std::vector<uint64_t> trace_memory_addresses={0x00014050,0x00014060,0x00014070,0x00014080};
    std::vector<uint64_t> trace_memory_tile_addresses={0x00011000};
    core_addresses.insert(std::end(core_addresses), std::begin(trace_core_addresses), std::end(trace_core_addresses));
    interface_addresses.insert(std::end(interface_addresses), std::begin(trace_interface_addresses), std::end(trace_interface_addresses));
    memory_addresses.insert(std::end(memory_addresses), std::begin(trace_memory_addresses), std::end(trace_memory_addresses));
    memory_tile_addresses.insert(std::end(memory_tile_addresses), std::begin(trace_memory_tile_addresses), std::end(trace_memory_tile_addresses));
  }
  void populateRegNameToValueMap(){
    //some implementation
    regNametovalues=  {
#include "xdp/profile/plugin/aie_debug/generations/pythonlogfile1.txt"
                      };
  }

  void populateRegValueToNameMap(){
    //some implementation
    regValueToName=  {
#include "xdp/profile/plugin/aie_debug/generations/py_rev_map_gen1.txt"
                      };
  }

};

class AIE2UsedRegisters : public UsedRegisters {
 public:
  void populateProfileRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00032500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateTraceRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00031500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateRegNameToValueMap(){
    regNametovalues=  {
#include "xdp/profile/plugin/aie_debug/generations/pythonlogfile2.txt"
                      };
  }

  void populateRegValueToNameMap(){
    //some implementation
    regValueToName=  {
#include "xdp/profile/plugin/aie_debug/generations/py_rev_map_gen2.txt"
                      };
  }

};

class AIE2pUsedRegisters : public UsedRegisters {
 public:
  void populateProfileRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00032500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateTraceRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00031500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateRegNameToValueMap(){
    //Dont know which ones are exactly for AIE2p. Populating with AIE2 ones for now
    //TODO: populate correct registers with python script
    regNametovalues=  {
#include "xdp/profile/plugin/aie_debug/generations/pythonlogfile2.txt"
                      };
  }

  void populateRegValueToNameMap(){
    //some implementation
    regValueToName=  {
#include "xdp/profile/plugin/aie_debug/generations/py_rev_map_gen2.txt"
                      };
  }

};

class AIE2psUsedRegisters : public UsedRegisters {
 public:
  void populateProfileRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00031500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateTraceRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00031500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateRegNameToValueMap(){
    regNametovalues=  {
#include "xdp/profile/plugin/aie_debug/generations/pythonlogfile2ps.txt"
                      };
  }

  void populateRegValueToNameMap(){
    //some implementation
    regValueToName=  {
#include "xdp/profile/plugin/aie_debug/generations/py_rev_map_gen2ps.txt"
                      };
  }

};

class AIE4UsedRegisters : public UsedRegisters {
 public:
  void populateProfileRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00031500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateTraceRegisters(){
 //populate the correct usedregisters
    core_addresses={0x00031500};
    interface_addresses={0x0003FF00};
    memory_addresses={0x00011000};
    memory_tile_addresses={0x00011000};
  }
  void populateRegNameToValueMap(){
    //some implementation
    //dummy one for now
    regNametovalues=  { {"None",aie2ps::cm_core_bmll0_part1}};
  }
  void populateRegValueToNameMap(){
    //some implementation
    regValueToName=  { {0x00009320,"None"}  };
  }

};

} // end XDP namespace

#endif
