// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XDP_AIE_DEBUG_PLUGIN_DOT_H
#define XDP_AIE_DEBUG_PLUGIN_DOT_H

#include <memory>

#include "xdp/profile/plugin/aie_debug/aie_debug_impl.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class AieDebugPlugin : public XDPPlugin
  {
  public:
    AieDebugPlugin();
    ~AieDebugPlugin();
    static bool alive();
    void updateAIEDevice(void* handle);
    void endAIEDebugRead(void* handle);
    void endPollforDevice(void* handle);

    //*********************************************
    //new methods pushed up from edge implementaton

    std::vector<uint64_t>
    stringToRegList(std::string stringEntry, module_type t);

    std::vector<std::string>
    getSettingsVector(std::string settingsString);

    /****************************************************************************
    This function parses the metric settings as we enter into the xrt.ini file,
    and returns the list of all registers whose values to read.
    The AIE debug settings metrics can be entered in the following 3 ways:
    [AIE_debug_settings]
    # Very flexible but need to know specific reg values
    core_registers = 0x12345, 0x34567
    # Simplified but not flexible
    core_registers = trace_config, profile_config, all
    # Specific registers but hides gen-specific values
    core_registers = cm_core_status, mm_trace_status
    /************************************************************************* */
    std::map<module_type, std::vector<uint64_t>> parseMetrics();

    //The following two functions, lookupRegistername and lookupRegisterAddr returns
    //the name of the register for every register address. They takes into account
    //the fact that register names can have different addresses for different
    //AIE generations.
    std::string lookupRegistername(uint64_t regVal){
      std::string regName = "";
      if(usedRegisters->regValueToName.find(regVal)!=
                            usedRegisters->regValueToName.end())
        regName = usedRegisters->regValueToName[regVal];
      return regName;
    }

    uint64_t lookupRegisterAddr(std::string regName) {
      if(usedRegisters->regNametovalues.find(regName)!=
                            usedRegisters->regNametovalues.end())
        return usedRegisters->regNametovalues[regName];
      return -1 ;
    }


  private:
    uint64_t getDeviceIDFromHandle(void* handle);

    static bool live;
    uint32_t mIndex = 0;
    /***************************************************
    Since the addresses of the register names vary from
    generation to generation, the class UsedRegisters is
    created. UsedRegisters populates the lists of register
    names and addresses according to the aie hw generation.
    /*************************************************** */
    std::unique_ptr<UsedRegisters> usedRegisters;
    const std::map<module_type, const char*> moduleTypes = {
      {module_type::core, "AIE"},
      {module_type::dma, "DMA"},
      {module_type::shim, "Interface"},
      {module_type::mem_tile, "Memory Tile"}
    };

    //This struct and handleToAIEData map is created to provision multiple AIEs one the same machine, each denoted by its own handle
    struct AIEData {
      uint64_t deviceID;
      bool valid;
      std::unique_ptr<AieDebugImpl> implementation;
      std::shared_ptr<AieDebugMetadata> metadata;
    };
    std::map<void*, AIEData> handleToAIEData;

  };

} // end namespace xdp

#endif
