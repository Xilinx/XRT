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
#include "xdp/profile/plugin/aie_trace/aie_trace_impl.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  typedef struct {
    uint64_t perf_address;
    uint32_t perf_value;
  } trace_data_t;

  typedef struct {
    uint32_t count;
    trace_data_t trace_data[1];
  } aie_trace_op_t;

  class AieTrace_WinImpl : public AieTraceImpl {
    public:
      AieTrace_WinImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata);
      ~AieTrace_WinImpl() = default;
      virtual void updateDevice();
      virtual void flushTraceModules();
      virtual void freeResources();
      virtual void pollTimers(uint64_t index, void* handle);
      virtual uint64_t checkTraceBufSize(uint64_t size);

      bool setMetricsSettings(uint64_t deviceId, void* handle);
      module_type getTileType(uint16_t row);
      uint16_t getRelativeRow(uint16_t absRow);
      
      bool isInputSet(const module_type type, const std::string metricSet);
      bool isStreamSwitchPortEvent(const XAie_Events event);
      bool isPortRunningEvent(const XAie_Events event);
      uint8_t getPortNumberFromEvent(XAie_Events event);
      void configStreamSwitchPorts(const tile_type& tile,
                                   /*xaiefal::XAieTile& xaieTile,*/ const XAie_LocType loc,
                                   const module_type type, const std::string metricSet, 
                                   const uint8_t channel0, const uint8_t channel1,
                                  std::vector<XAie_Events>& events);
      void configEventSelections(const XAie_LocType loc, const module_type type, 
                                 const std::string metricSet, const uint8_t channel0,
                                 const uint8_t channel);
      void configEdgeEvents(const tile_type& tile, const module_type type,
                            const std::string metricSet, const XAie_Events event);

      uint32_t bcIdToEvent(int bcId);
      // int run_shim_loopback();
      // uint8_t* shim_loopback_test(uint64_t src, uint64_t dest, uint32_t size, uint8_t col);

    private:
      typedef XAie_Events EventType;
      typedef std::vector<EventType> EventVector;

      xrt::device device;
      xrt::kernel aie_trace_kernel;

      xrt::kernel mKernel;
      xrt::bo input_bo;
      aie_trace_op_t* op;
      std::size_t op_size;
      XAie_DevInst aieDevInst = {0};

      std::map<std::string, EventVector> mCoreEventSets;
      std::map<std::string, EventVector> mMemoryEventSets;
      std::map<std::string, EventVector> mMemoryTileEventSets;
      std::map<std::string, EventVector> mInterfaceTileEventSets;

      // Trace metrics (same for all sets)
      EventType mCoreTraceStartEvent;
      EventType mCoreTraceEndEvent;
      EventType mMemoryModTraceStartEvent;
      EventType mMemoryTileTraceStartEvent;
      EventType mMemoryTileTraceEndEvent;
      EventType mInterfaceTileTraceStartEvent;
      EventType mInterfaceTileTraceEndEvent;

      // Tile locations to apply trace end and flush
      std::vector<XAie_LocType> mTraceFlushLocs;
      std::vector<XAie_LocType> mMemoryTileTraceFlushLocs;
      std::vector<XAie_LocType> mInterfaceTileTraceFlushLocs;

      // Keep track of number of events reserved per module and/or tile
      int mNumTileTraceEvents[static_cast<int>(module_type::num_types)][NUM_TRACE_EVENTS + 1];
    
  };

}   

#endif
