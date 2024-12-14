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
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

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
    
    xdp::aie::driver_config getAIEConfigMetadata() {return metadataReader->getDriverConfig();}

    bool aieMetadataEmpty() const {return (metadataReader == nullptr);}
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

    const AIEProfileFinalConfig& getAIEProfileConfig() const;

  private:
    std::vector<uint64_t> stringToRegList(std::string stringEntry, module_type t);
    std::vector<std::string> getSettingsVector(std::string settingsString);

    //The following two functions, lookupRegistername and lookupRegisterAddr returns
    //the name of the register for every register address. They takes into account
    //the fact that register names can have different addresses for different
    //AIE generations.
    std::string lookupRegisterName(uint64_t regVal) {
      if (usedRegisters->regValueToName.find(regVal) !=
          usedRegisters->regValueToName.end())
        return usedRegisters->regValueToName[regVal];
      return "";
    }

    uint64_t lookupRegisterAddr(std::string regName) {
      if (usedRegisters->regNametovalues.find(regName) !=
          usedRegisters->regNametovalues.end())
        return usedRegisters->regNametovalues[regName];
      return -1;
    }

  private:
    // Currently supporting Core, Memory, Interface Tiles, and Memory Tiles
    static constexpr int NUM_MODULES = 4;
    const std::vector<std::string> moduleNames =
      {"aie", "aie_memory", "interface_tile", "memory_tile"};
    const module_type moduleTypes[NUM_MODULES] =
      {module_type::core, module_type::dma, module_type::shim, module_type::mem_tile};
    const std::map<module_type, const char*> moduleNames = {
      {module_type::core, "AIE"},
      {module_type::dma, "DMA"},
      {module_type::shim, "Interface"},
      {module_type::mem_tile, "Memory Tile"}
    };

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
    virtual void readValues(XAie_DevInst* aieDevInst)=0;

    void insertOffsets(uint64_t rel, uint64_t ab) {
      relativeOffsets.push_back(rel);
      absoluteOffsets.push_back(ab);
    }

    void printValues(uint32_t deviceID, VPDatabase* db){
      int i=0;
      for (auto& absoluteOffset : absoluteOffsets) {
        std::stringstream msg;
        msg << "!!! Debug tile (" << col << ", " << row << ") "
            << "hex address/values: 0x" << std::hex << absoluteOffset << " : "
            << values[i] << std::dec;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

        db->getDynamicInfo().addAIEDebugSample(deviceID, col,row,relativeOffsets[i],absoluteOffset,values[i]);
        i++;
      }
    }

  public:
    std::vector <uint32_t> values;
    int col;
    int row;
    std::vector<uint64_t> relativeOffsets;
    std::vector<uint64_t> absoluteOffsets;
};

/*************************************************************************************
The class UsedRegisters is what gives us AIE hw generation specific data. The base class
has virtual functions which populate the correct registers and their addresses according
to the AIE hw generation in the derived classes. Thus we can dynamically populate the
correct registers and their addresses at runtime.
**************************************************************************************/
class UsedRegisters {
  public:
    UsedRegisters() {
      populateRegNameToValueMap();
      populateRegValueToNameMap();
    }

    std::map<std::string, uint64_t> regNametoValues;
    std::map<uint64_t, std::string> regValueToName;

    std::vector<uint64_t> getCoreAddresses() {
      return core_addresses;
    }
    std::vector<uint64_t> getMemoryAddresses() {
      return memory_addresses;
    }
    std::vector<uint64_t> getInterfaceAddresses() {
      return interface_addresses;
    }
    std::vector<uint64_t> getMemoryTileAddresses() {
      return memory_tile_addresses;
    }

    virtual void populateProfileRegisters()=0;
    virtual void populateTraceRegisters()=0;
    virtual void populateRegNameToValueMap()=0;
    virtual void populateRegValueToNameMap()=0;
    
    void populateAllRegisters() {
      populateProfileRegisters();
      populateTraceRegisters();
    }

  protected:
    std::set<uint64_t> core_addresses;
    std::set<uint64_t> memory_addresses;
    std::set<uint64_t> interface_addresses;
    std::set<uint64_t> memory_tile_addresses;
};

class AIE1UsedRegisters : public UsedRegisters {
public:
  void populateProfileRegisters() {
    // Core modules
    core_addresses.emplace(aie1::cm_performance_control0);
    core_addresses.emplace(aie1::cm_performance_control1);
    core_addresses.emplace(aie1::cm_performance_control2);
    core_addresses.emplace(aie1::cm_performance_counter0);
    core_addresses.emplace(aie1::cm_performance_counter1);
    core_addresses.emplace(aie1::cm_performance_counter2);
    core_addresses.emplace(aie1::cm_performance_counter3);
    core_addresses.emplace(aie1::cm_performance_counter0_event_value);
    core_addresses.emplace(aie1::cm_performance_counter1_event_value);
    core_addresses.emplace(aie1::cm_performance_counter2_event_value);
    core_addresses.emplace(aie1::cm_performance_counter3_event_value);

    // Memory modules
    memory_addresses.emplace(aie1::mm_performance_control0);
    memory_addresses.emplace(aie1::mm_performance_control1);
    memory_addresses.emplace(aie1::mm_performance_counter0);
    memory_addresses.emplace(aie1::mm_performance_counter1);
    memory_addresses.emplace(aie1::mm_performance_counter0_event_value);
    memory_addresses.emplace(aie1::mm_performance_counter1_event_value);

    // Interface tiles
    interface_addresses.emplace(aie1::shim_performance_control0);
    interface_addresses.emplace(aie1::shim_performance_control1);
    interface_addresses.emplace(aie1::shim_performance_counter0);
    interface_addresses.emplace(aie1::shim_performance_counter1);
    interface_addresses.emplace(aie1::shim_performance_counter0_event_value);
    interface_addresses.emplace(aie1::shim_performance_counter1_event_value);

    // Memory tiles
    // NOTE: not available on AIE1
  }
  
  void populateTraceRegisters() {
    // Core modules
    core_addresses.emplace(aie1::cm_performance_control0);
    core_addresses.emplace(aie1::cm_core_status);
    core_addresses.emplace(aie1::cm_trace_control0);
    core_addresses.emplace(aie1::cm_trace_control1);
    core_addresses.emplace(aie1::cm_trace_status);
    core_addresses.emplace(aie1::cm_trace_event0);
    core_addresses.emplace(aie1::cm_event_status0);
    core_addresses.emplace(aie1::cm_event_broadcast0);
    core_addresses.emplace(aie1::cm_timer_trig_event_low_value);
    core_addresses.emplace(aie1::cm_timer_trig_event_high_value);
    core_addresses.emplace(aie1::cm_timer_low);
    core_addresses.emplace(aie1::cm_timer_high);
    core_addresses.emplace(aie1::cm_edge_detection_event_control);
    core_addresses.emplace(aie1::cm_num_ss_event_ports);
    core_addresses.emplace(aie1::cm_stream_switch_event_port_selection_0);
    core_addresses.emplace(aie1::cm_num_event_status_regs);
    
    // Memory modules
    memory_addresses.emplace(aie1::mm_trace_control0);
    memory_addresses.emplace(aie1::mm_trace_control1);
    memory_addresses.emplace(aie1::mm_trace_status);
    memory_addresses.emplace(aie1::mm_event_status0);
    memory_addresses.emplace(aie1::mm_trace_event0);
    memory_addresses.emplace(aie1::mm_event_broadcast0);
    memory_addresses.emplace(aie1::mm_num_event_status_regs);

    // Interface tiles
    interface_addresses.emplace(aie1::shim_trace_control0);
    interface_addresses.emplace(aie1::shim_trace_control1);
    interface_addresses.emplace(aie1::shim_trace_status);
    interface_addresses.emplace(aie1::shim_trace_event0);
    interface_addresses.emplace(aie1::shim_event_broadcast_a_0);
    interface_addresses.emplace(aie1::shim_event_status0);
    interface_addresses.emplace(aie1::shim_num_ss_event_ports);
    interface_addresses.emplace(aie1::shim_stream_switch_event_port_selection_0);
    interface_addresses.emplace(aie1::shim_num_event_status_regs);

    // Memory tiles
    // NOTE: not available on AIE1
  }

  void populateRegNameToValueMap() {
    //some implementation
    regNametoValues=  {
#include "xdp/profile/plugin/aie_debug/generations/pythonlogfile1.txt"
                      };
  }

  void populateRegValueToNameMap() {
    //some implementation
    regValueToName=  {
#include "xdp/profile/plugin/aie_debug/generations/py_rev_map_gen1.txt"
                      };
  }
};

class AIE2UsedRegisters : public UsedRegisters {
public:
  void populateProfileRegisters() {
    // Core modules
    core_addresses.emplace(aie2::cm_performance_control0);
    core_addresses.emplace(aie2::cm_performance_control1);
    core_addresses.emplace(aie2::cm_performance_control2);
    core_addresses.emplace(aie2::cm_performance_counter0);
    core_addresses.emplace(aie2::cm_performance_counter1);
    core_addresses.emplace(aie2::cm_performance_counter2);
    core_addresses.emplace(aie2::cm_performance_counter3);
    core_addresses.emplace(aie2::cm_performance_counter0_event_value);
    core_addresses.emplace(aie2::cm_performance_counter1_event_value);
    core_addresses.emplace(aie2::cm_performance_counter2_event_value);
    core_addresses.emplace(aie2::cm_performance_counter3_event_value);

    // Memory modules
    memory_addresses.emplace(aie2::mm_performance_control0);
    memory_addresses.emplace(aie2::mm_performance_control1);
    memory_addresses.emplace(aie2::mm_performance_counter0);
    memory_addresses.emplace(aie2::mm_performance_counter1);
    memory_addresses.emplace(aie2::mm_performance_counter0_event_value);
    memory_addresses.emplace(aie2::mm_performance_counter1_event_value);

    // Interface tiles
    interface_addresses.emplace(aie2::shim_performance_control0);
    interface_addresses.emplace(aie2::shim_performance_control1);
    interface_addresses.emplace(aie2::shim_performance_counter0);
    interface_addresses.emplace(aie2::shim_performance_counter1);
    interface_addresses.emplace(aie2::shim_performance_counter0_event_value);
    interface_addresses.emplace(aie2::shim_performance_counter1_event_value);

    // Memory tiles
    memory_tile_addresses.emplace(aie2::mem_performance_control0);
    memory_tile_addresses.emplace(aie2::mem_performance_control1);
    memory_tile_addresses.emplace(aie2::mem_performance_control2);
    memory_tile_addresses.emplace(aie2::mem_performance_counter0);
    memory_tile_addresses.emplace(aie2::mem_performance_counter1);
    memory_tile_addresses.emplace(aie2::mem_performance_counter2);
    memory_tile_addresses.emplace(aie2::mem_performance_counter3);
    memory_tile_addresses.emplace(aie2::mem_performance_counter0_event_value);
    memory_tile_addresses.emplace(aie2::mem_performance_counter1_event_value);
    memory_tile_addresses.emplace(aie2::mem_performance_counter2_event_value);
    memory_tile_addresses.emplace(aie2::mem_performance_counter3_event_value);
  }

  void populateTraceRegisters() {
    // Core modules
    core_addresses.emplace(aie2::cm_performance_control0);
    core_addresses.emplace(aie2::cm_core_status);
    core_addresses.emplace(aie2::cm_trace_control0);
    core_addresses.emplace(aie2::cm_trace_control1);
    core_addresses.emplace(aie2::cm_trace_status);
    core_addresses.emplace(aie2::cm_trace_event0);
    core_addresses.emplace(aie2::cm_event_status0);
    core_addresses.emplace(aie2::cm_event_broadcast0);
    core_addresses.emplace(aie2::cm_timer_trig_event_low_value);
    core_addresses.emplace(aie2::cm_timer_trig_event_high_value);
    core_addresses.emplace(aie2::cm_timer_low);
    core_addresses.emplace(aie2::cm_timer_high);
    core_addresses.emplace(aie2::cm_edge_detection_event_control);
    core_addresses.emplace(aie2::cm_num_ss_event_ports);
    core_addresses.emplace(aie2::cm_stream_switch_event_port_selection_0);
    core_addresses.emplace(aie2::cm_num_event_status_regs);
    
    // Memory modules
    memory_addresses.emplace(aie2::mm_trace_control0);
    memory_addresses.emplace(aie2::mm_trace_control1);
    memory_addresses.emplace(aie2::mm_trace_status);
    memory_addresses.emplace(aie2::mm_event_status0);
    memory_addresses.emplace(aie2::mm_trace_event0);
    memory_addresses.emplace(aie2::mm_event_broadcast0);
    memory_addresses.emplace(aie2::mm_num_event_status_regs);

    // Interface tiles
    interface_addresses.emplace(aie2::shim_trace_control0);
    interface_addresses.emplace(aie2::shim_trace_control1);
    interface_addresses.emplace(aie2::shim_trace_status);
    interface_addresses.emplace(aie2::shim_trace_event0);
    interface_addresses.emplace(aie2::shim_event_broadcast_a_0);
    interface_addresses.emplace(aie2::shim_event_status0);
    interface_addresses.emplace(aie2::shim_num_ss_event_ports);
    interface_addresses.emplace(aie2::shim_stream_switch_event_port_selection_0);
    interface_addresses.emplace(aie2::shim_num_event_status_regs);

    // Memory tiles
    memory_tile_addresses.emplace(aie2::mem_trace_control0);
    memory_tile_addresses.emplace(aie2::mem_trace_control1);
    memory_tile_addresses.emplace(aie2::mem_trace_status);
    memory_tile_addresses.emplace(aie2::mem_dma_event_channel_selection);
    memory_tile_addresses.emplace(aie2::mem_edge_detection_event_control);
    memory_tile_addresses.emplace(aie2::mem_stream_switch_event_port_selection_0);
    memory_tile_addresses.emplace(aie2::mem_stream_switch_event_port_selection_1);
    memory_tile_addresses.emplace(aie2::mem_trace_event0);
    memory_tile_addresses.emplace(aie2::mem_event_broadcast0);
    memory_tile_addresses.emplace(aie2::mem_event_status0);
    memory_tile_addresses.emplace(aie2::mem_num_event_status_regs);
  }

  void populateRegNameToValueMap() {
    regNametovalues=  {
#include "xdp/profile/plugin/aie_debug/generations/pythonlogfile2.txt"
                      };
  }

  void populateRegValueToNameMap() {
    //some implementation
    regValueToName=  {
#include "xdp/profile/plugin/aie_debug/generations/py_rev_map_gen2.txt"
                      };
  }
};


class AIE2psUsedRegisters : public UsedRegisters {
public:
  void populateProfileRegisters() {
    // Core modules
    core_addresses.emplace(aie2ps::cm_performance_control0);
    core_addresses.emplace(aie2ps::cm_performance_control1);
    core_addresses.emplace(aie2ps::cm_performance_control2);
    core_addresses.emplace(aie2ps::cm_performance_counter0);
    core_addresses.emplace(aie2ps::cm_performance_counter1);
    core_addresses.emplace(aie2ps::cm_performance_counter2);
    core_addresses.emplace(aie2ps::cm_performance_counter3);
    core_addresses.emplace(aie2ps::cm_performance_counter0_event_value);
    core_addresses.emplace(aie2ps::cm_performance_counter1_event_value);
    core_addresses.emplace(aie2ps::cm_performance_counter2_event_value);
    core_addresses.emplace(aie2ps::cm_performance_counter3_event_value);

    // Memory modules
    memory_addresses.emplace(aie2ps::mm_performance_control0);
    memory_addresses.emplace(aie2ps::mm_performance_control1);
    memory_addresses.emplace(aie2ps::mm_performance_control2);
    memory_addresses.emplace(aie2ps::mm_performance_control3);
    memory_addresses.emplace(aie2ps::mm_performance_counter0);
    memory_addresses.emplace(aie2ps::mm_performance_counter1);
    memory_addresses.emplace(aie2ps::mm_performance_counter2);
    memory_addresses.emplace(aie2ps::mm_performance_counter3);
    memory_addresses.emplace(aie2ps::mm_performance_counter0_event_value);
    memory_addresses.emplace(aie2ps::mm_performance_counter1_event_value);

    // Interface tiles
    interface_addresses.emplace(aie2ps::shim_performance_control0);
    interface_addresses.emplace(aie2ps::shim_performance_control1);
    interface_addresses.emplace(aie2ps::shim_performance_control2);
    interface_addresses.emplace(aie2ps::shim_performance_control3);
    interface_addresses.emplace(aie2ps::shim_performance_control4);
    interface_addresses.emplace(aie2ps::shim_performance_control5);
    interface_addresses.emplace(aie2ps::shim_performance_counter0);
    interface_addresses.emplace(aie2ps::shim_performance_counter1);
    interface_addresses.emplace(aie2ps::shim_performance_counter2);
    interface_addresses.emplace(aie2ps::shim_performance_counter3);
    interface_addresses.emplace(aie2ps::shim_performance_counter4);
    interface_addresses.emplace(aie2ps::shim_performance_counter5);
    interface_addresses.emplace(aie2ps::shim_performance_counter0_event_value);
    interface_addresses.emplace(aie2ps::shim_performance_counter1_event_value);

    // Memory tiles
    memory_tile_addresses.emplace(aie2ps::mem_performance_control0);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control1);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control2);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control3);
    memory_tile_addresses.emplace(aie2ps::mem_performance_control4);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter0);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter1);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter2);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter3);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter4);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter5);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter0_event_value);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter1_event_value);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter2_event_value);
    memory_tile_addresses.emplace(aie2ps::mem_performance_counter3_event_value);
  }

  void populateTraceRegisters() {
    // Core modules
    core_addresses.emplace(aie2ps::cm_performance_control0);
    core_addresses.emplace(aie2ps::cm_core_status);
    core_addresses.emplace(aie2ps::cm_trace_control0);
    core_addresses.emplace(aie2ps::cm_trace_control1);
    core_addresses.emplace(aie2ps::cm_trace_status);
    core_addresses.emplace(aie2ps::cm_trace_event0);
    core_addresses.emplace(aie2ps::cm_event_status0);
    core_addresses.emplace(aie2ps::cm_event_broadcast0);
    core_addresses.emplace(aie2ps::cm_timer_trig_event_low_value);
    core_addresses.emplace(aie2ps::cm_timer_trig_event_high_value);
    core_addresses.emplace(aie2ps::cm_timer_low);
    core_addresses.emplace(aie2ps::cm_timer_high);
    core_addresses.emplace(aie2ps::cm_edge_detection_event_control);
    core_addresses.emplace(aie2ps::cm_num_ss_event_ports);
    core_addresses.emplace(aie2ps::cm_stream_switch_event_port_selection_0);
    core_addresses.emplace(aie2ps::cm_num_event_status_regs);
    
    // Memory modules
    memory_addresses.emplace(aie2ps::mm_trace_control0);
    memory_addresses.emplace(aie2ps::mm_trace_control1);
    memory_addresses.emplace(aie2ps::mm_trace_status);
    memory_addresses.emplace(aie2ps::mm_event_status0);
    memory_addresses.emplace(aie2ps::mm_trace_event0);
    memory_addresses.emplace(aie2ps::mm_event_broadcast0);
    memory_addresses.emplace(aie2ps::mm_num_event_status_regs);

    // Interface tiles
    interface_addresses.emplace(aie2ps::shim_trace_control0);
    interface_addresses.emplace(aie2ps::shim_trace_control1);
    interface_addresses.emplace(aie2ps::shim_trace_status);
    interface_addresses.emplace(aie2ps::shim_trace_event0);
    interface_addresses.emplace(aie2ps::shim_event_broadcast_a_0);
    interface_addresses.emplace(aie2ps::shim_event_status0);
    interface_addresses.emplace(aie2ps::shim_num_ss_event_ports);
    interface_addresses.emplace(aie2ps::shim_stream_switch_event_port_selection_0);
    interface_addresses.emplace(aie2ps::shim_num_event_status_regs);

    // Memory tiles
    memory_tile_addresses.emplace(aie2ps::mem_trace_control0);
    memory_tile_addresses.emplace(aie2ps::mem_trace_control1);
    memory_tile_addresses.emplace(aie2ps::mem_trace_status);
    memory_tile_addresses.emplace(aie2ps::mem_dma_event_channel_selection);
    memory_tile_addresses.emplace(aie2ps::mem_edge_detection_event_control);
    memory_tile_addresses.emplace(aie2ps::mem_stream_switch_event_port_selection_0);
    memory_tile_addresses.emplace(aie2ps::mem_stream_switch_event_port_selection_1);
    memory_tile_addresses.emplace(aie2ps::mem_trace_event0);
    memory_tile_addresses.emplace(aie2ps::mem_event_broadcast0);
    memory_tile_addresses.emplace(aie2ps::mem_event_status0);
    memory_tile_addresses.emplace(aie2ps::mem_num_event_status_regs);
  }

  void populateRegNameToValueMap() {
    regNametovalues=  {
#include "xdp/profile/plugin/aie_debug/generations/pythonlogfile2ps.txt"
                      };
  }

  void populateRegValueToNameMap() {
    //some implementation
    regValueToName=  {
#include "xdp/profile/plugin/aie_debug/generations/py_rev_map_gen2ps.txt"
                      };
  }
};

} // end XDP namespace

#endif
