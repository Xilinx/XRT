// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_PROFILE_NPU3_H
#define AIE_PROFILE_NPU3_H

#include <cstdint>
#include <memory>

#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_impl.h"
#include "xdp/profile/device/common/npu3_transaction.h"
#include "xdp/profile/plugin/aie_base/generations/npu3_registers.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  
  class AieProfile_NPU3Impl : public AieProfileImpl {
   public:
    AieProfile_NPU3Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);
    ~AieProfile_NPU3Impl() = default;

    void updateDevice();
    void poll(const uint32_t index, void* handle);
    void freeResources();
    bool setMetricsSettings(const uint64_t deviceId);
    
    void configEventSelections(const XAie_LocType loc, const module_type type,
                               const std::string metricSet, std::vector<uint8_t>& channels);
    void configStreamSwitchPorts(const tile_type& tile, const XAie_LocType& loc,
                                 const module_type& type, const std::string& metricSet,
                                 const uint8_t channel, const XAie_Events startEvent);
   private:
      const std::vector<XAie_ModuleType> falModuleTypes = {
        XAIE_CORE_MOD,
        XAIE_MEM_MOD,
        XAIE_PL_MOD,
        XAIE_MEM_MOD
      };

      const std::map<module_type, std::vector<uint64_t>> regValues {
#ifdef XDP_NPU3_BUILD
        {module_type::core,     {npu3::cm_performance_counter0,    npu3::cm_performance_counter1,
                                 npu3::cm_performance_counter2,    npu3::cm_performance_counter3,
                                 npu3::cm_performance_counter4,    npu3::cm_performance_counter5,
                                 npu3::cm_performance_counter6,    npu3::cm_performance_counter7,
                                 npu3::cm_performance_counter8,    npu3::cm_performance_counter9,
                                 npu3::cm_performance_counter10,   npu3::cm_performance_counter11}},
        {module_type::dma,      {}}, 
        {module_type::shim,     {npu3::shim_performance_counter0,  npu3::shim_performance_counter1,
                                 npu3::shim_performance_counter2,  npu3::shim_performance_counter3,
                                 npu3::shim_performance_counter4,  npu3::shim_performance_counter5,
                                 npu3::shim_performance_counter6,  npu3::shim_performance_counter7,
                                 npu3::shim_performance_counter8,  npu3::shim_performance_counter9,
                                 npu3::shim_performance_counter10, npu3::shim_performance_counter11}},
        {module_type::mem_tile, {npu3::mem_performance_counter0,   npu3::mem_performance_counter1,
                                 npu3::mem_performance_counter2,   npu3::mem_performance_counter3,
                                 npu3::mem_performance_counter4,   npu3::mem_performance_counter5,
                                 npu3::mem_performance_counter6,   npu3::mem_performance_counter7,
                                 npu3::mem_performance_counter8,   npu3::mem_performance_counter9,
                                 npu3::mem_performance_counter10,  npu3::mem_performance_counter11}}
#endif
      };

      std::map<std::string, std::vector<XAie_Events>> coreStartEvents;
      std::map<std::string, std::vector<XAie_Events>> coreEndEvents;
      std::map<std::string, std::vector<XAie_Events>> memoryStartEvents;
      std::map<std::string, std::vector<XAie_Events>> memoryEndEvents;
      std::map<std::string, std::vector<XAie_Events>> shimStartEvents;
      std::map<std::string, std::vector<XAie_Events>> shimEndEvents;
      std::map<std::string, std::vector<XAie_Events>> memTileStartEvents;
      std::map<std::string, std::vector<XAie_Events>> memTileEndEvents;

      bool finishedPoll = false;
      read_register_op_t* op;
      std::size_t op_size;
      XAie_DevInst aieDevInst = {0};
      std::vector<std::vector<uint64_t>> outputValues;
      std::unique_ptr<aie::NPU3Transaction> tranxHandler;
  };

}  // namespace xdp

#endif
