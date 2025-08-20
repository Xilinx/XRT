// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_PROFILE_H
#define AIE_PROFILE_H

#include <cstdint>
#include <memory>

#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_impl.h"
#include "xdp/profile/device/common/client_transaction.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  
  class AieProfile_WinImpl : public AieProfileImpl {
   public:
    AieProfile_WinImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);
    ~AieProfile_WinImpl() = default;

    void updateDevice();

    void startPoll(const uint64_t /*id*/) override {}
    void continuePoll(const uint64_t /*id*/) override {}
    void poll(const uint64_t id) override;
    void endPoll() override {}

    void freeResources();
    bool setMetricsSettings(const uint64_t deviceId);
    void configStreamSwitchPorts(
      const tile_type& tile, const XAie_LocType& loc,
      const module_type& type, const std::string& metricSet,
      const uint8_t channel, const XAie_Events startEvent
    );
   private:
      const std::vector<XAie_ModuleType> falModuleTypes = {
        XAIE_CORE_MOD,
        XAIE_MEM_MOD,
        XAIE_PL_MOD,
        XAIE_MEM_MOD
      };

      const std::map<module_type, std::vector<uint64_t>> regValues {
        {module_type::core, {0x31520,0x31524,0x31528,0x3152C}}, 
        {module_type::dma, {0x11020,0x11024}}, 
        {module_type::shim, {0x31020, 0x31024}}, 
        {module_type::mem_tile, {0x91020,0x91024,0x91028,0x9102C}}, 
      };

      std::map<std::string, std::vector<XAie_Events>> coreStartEvents;
      std::map<std::string, std::vector<XAie_Events>> coreEndEvents;
      std::map<std::string, std::vector<XAie_Events>> memoryStartEvents;
      std::map<std::string, std::vector<XAie_Events>> memoryEndEvents;
      std::map<std::string, std::vector<XAie_Events>> shimStartEvents;
      std::map<std::string, std::vector<XAie_Events>> shimEndEvents;
      std::map<std::string, std::vector<XAie_Events>> memTileStartEvents;
      std::map<std::string, std::vector<XAie_Events>> memTileEndEvents;

      std::unique_ptr<aie::ClientTransaction> transactionHandler;
      read_register_op_t* op;
      std::size_t op_size;
      XAie_DevInst aieDevInst = {0};
      std::vector<std::vector<uint64_t>> outputValues;
      bool finishedPoll = false;

  };

}  // namespace xdp

#endif
