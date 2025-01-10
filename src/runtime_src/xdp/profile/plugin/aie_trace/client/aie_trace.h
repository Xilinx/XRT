/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_TRACE_DOT_H
#define AIE_TRACE_DOT_H

#include <cstdint>

#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_impl.h"
#include "xdp/profile/device/common/client_transaction.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  class AieTrace_WinImpl : public AieTraceImpl {
    public:
      AieTrace_WinImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata);
      ~AieTrace_WinImpl() = default;
      virtual void updateDevice();
      virtual void flushTraceModules();
      virtual void freeResources();
      virtual void pollTimers(uint64_t index, void* handle);
      virtual uint64_t checkTraceBufSize(uint64_t size);

      void modifyEvents(module_type type, io_type subtype, 
                        const std::string metricSet, uint8_t channel, 
                        std::vector<XAie_Events>& events);
      bool setMetricsSettings(uint64_t deviceId, void* handle);
      bool configureWindowedEventTrace(void* handle);
      void build2ChannelBroadcastNetwork(void *handle, uint8_t broadcastId1, uint8_t broadcastId2, XAie_Events event);
      void reset2ChannelBroadcastNetwork(void *handle, uint8_t broadcastId1, uint8_t broadcastId2);
      module_type getTileType(uint8_t row);
      uint16_t getRelativeRow(uint16_t absRow);
      uint32_t bcIdToEvent(int bcId);
      
      bool isInputSet(const module_type type, const std::string metricSet);
      bool isStreamSwitchPortEvent(const XAie_Events event);
      bool isPortRunningEvent(const XAie_Events event);
      bool isCoreModuleEvent(const XAie_Events event);
      bool isDmaSet(const std::string metricSet);

      uint8_t getPortNumberFromEvent(XAie_Events event);
      int8_t getChannelNumberFromEvent(XAie_Events event);
      void configStreamSwitchPorts(const tile_type& tile, const XAie_LocType loc,
                                   const module_type type, const std::string metricSet, 
                                   const uint8_t channel0, const uint8_t channel1,
                                  std::vector<XAie_Events>& events, aie_cfg_base& config);
      std::vector<XAie_Events> configComboEvents(const XAie_LocType loc, const XAie_ModuleType mod, 
                                                 const module_type type, const std::string metricSet, 
                                                 aie_cfg_base& config);
      void configGroupEvents(const XAie_LocType loc, const XAie_ModuleType mod, 
                             const module_type type, const std::string metricSet);
      void configEventSelections(const XAie_LocType loc, const module_type type, 
                                 const std::string metricSet, const uint8_t channel0,
                                 const uint8_t channel1, aie_cfg_base& config);
      void configEdgeEvents(const tile_type& tile, const module_type type,
                            const std::string metricSet, const XAie_Events event,
                            const uint8_t channel = 0);
    
    private:
      typedef XAie_Events EventType;
      typedef std::vector<EventType> EventVector;
      std::unique_ptr<aie::ClientTransaction> transactionHandler;
  

      std::size_t op_size;
      XAie_DevInst aieDevInst = {0};

      std::map<std::string, EventVector> coreEventSets;
      std::map<std::string, EventVector> memoryEventSets;
      std::map<std::string, EventVector> memoryTileEventSets;
      std::map<std::string, EventVector> interfaceTileEventSets;

      // Trace metrics (same for all sets)
      EventType coreTraceStartEvent;
      EventType coreTraceEndEvent;
      EventType memoryModTraceStartEvent;
      EventType memoryTileTraceStartEvent;
      EventType memoryTileTraceEndEvent;
      EventType interfaceTileTraceStartEvent;
      EventType interfaceTileTraceEndEvent;

      bool m_trace_start_broadcast;

      // Tile locations to apply trace end and flush
      std::vector<XAie_LocType> traceFlushLocs;
      std::vector<XAie_LocType> memoryTileTraceFlushLocs;
      std::vector<XAie_LocType> interfaceTileTraceFlushLocs;

      // Keep track of number of events reserved per module and/or tile
      int mNumTileTraceEvents[static_cast<int>(module_type::num_types)][NUM_TRACE_EVENTS + 1];
    
  };

}   

#endif
