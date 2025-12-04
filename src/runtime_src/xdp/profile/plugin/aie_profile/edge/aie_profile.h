// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_PROFILE_H
#define AIE_PROFILE_H

#include <cstdint>

#include "core/edge/common/aie_parser.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_impl.h"
#include "xdp/profile/plugin/aie_profile/util/aie_profile_util.h"
#include "xaiefal/xaiefal.hpp"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  using tile_type = xdp::tile_type;
  
  class AieProfile_EdgeImpl : public AieProfileImpl {
    public:
      AieProfile_EdgeImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);
      ~AieProfile_EdgeImpl() = default;

      void updateDevice();

      void startPoll(const uint64_t id) override;
      void continuePoll(const uint64_t id) override;
      void poll(const uint64_t id) override;
      void endPoll() override;

      void freeResources();
      bool checkAieDevice(const uint64_t deviceId, void* handle);

      bool setMetricsSettings(const uint64_t deviceId, void* handle);
      void printTileModStats(xaiefal::XAieDev* aieDevice, 
                             const tile_type& tile, 
                             const XAie_ModuleType mod);
      
      uint64_t getCounterPayload(XAie_DevInst* aieDevInst,
                                 const tile_type& tile,
                                 const module_type type,
                                 uint8_t column, 
                                 uint8_t row, 
                                 uint16_t startEvent,
                                 const std::string metricSet,
                                 const uint8_t channel);

      uint64_t getAdfProfileAPIPayload(const tile_type& tile, const std::string metricSet);

    private:
      void displayAdfAPIResults();

    private:
      XAie_DevInst*     aieDevInst = nullptr;
      xaiefal::XAieDev* aieDevice  = nullptr;    

      std::map<std::string, std::vector<XAie_Events>> coreStartEvents;
      std::map<std::string, std::vector<XAie_Events>> coreEndEvents;
      std::map<std::string, std::vector<XAie_Events>> memoryStartEvents;
      std::map<std::string, std::vector<XAie_Events>> memoryEndEvents;
      std::map<std::string, std::vector<XAie_Events>> shimStartEvents;
      std::map<std::string, std::vector<XAie_Events>> shimEndEvents;
      std::map<std::string, std::vector<XAie_Events>> memTileStartEvents;
      std::map<std::string, std::vector<XAie_Events>> memTileEndEvents; 
      std::vector<std::shared_ptr<xaiefal::XAiePerfCounter>> perfCounters;
      std::vector<std::shared_ptr<xaiefal::XAieStreamPortSelect>> streamPorts;

      bool graphItrBroadcastConfigDone = false;
      // Graph Iterator broadcast channel event
      // This event is used to reset/configure the counters in interface tiles
      XAie_Events graphIteratorBrodcastChannelEvent = XAIE_EVENT_NONE_CORE;

      // This event is asserted in another interface tile
      XAie_Events latencyUserBrodcastChannelEvent = XAIE_EVENT_NONE_CORE;

      std::map<aie::profile::adfAPI, std::map<std::string, aie::profile::adfAPIResourceInfo>> adfAPIResourceInfoMap;
      // This stores the map of location of tile and configured broadcast channel event
      std::map<std::string, std::pair<int, XAie_Events>> adfAPIBroadcastEventsMap;

      std::vector<std::shared_ptr<xaiefal::XAieBroadcast>> bcResourcesBytesTx;
      std::vector<std::shared_ptr<xaiefal::XAieBroadcast>> bcResourcesLatency;

      uint8_t m_startColShift = 0;

      // Helper: convert relative column to XAIE column
      inline uint8_t getXAIECol(uint8_t relCol) const {
        auto absCol = relCol + m_startColShift;
        // For loadxclbin flow currently XRT creates partition of whole device from 0th column.
        // Hence absolute and relative columns are same.
        // TODO: For loadxclbin flow XRT will start creating partition of the specified columns,
        //       hence we should stop adding partition shift to col for passing to XAIE Apis
        return (db->getStaticInfo().getAppStyle() == xdp::AppStyle::LOAD_XCLBIN_STYLE) ? absCol : relCol;
      }
  };
}   

#endif
