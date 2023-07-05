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

#define XDP_SOURCE 

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <memory>
#include <cstring>

#include "aie_profile.h"

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/xrt_profiling.h"
#include "core/edge/user/shim.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->getAieArray() ;
    if (!aieArray)
      return nullptr ;
    return aieArray->getDevInst() ;
  }

  static void* allocateAieDevice(void* devHandle)
  {
    auto aieDevInst = static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle)) ;
    if (!aieDevInst)
      return nullptr;
    return new xaiefal::XAieDev(aieDevInst, false) ;
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    auto object = static_cast<xaiefal::XAieDev*>(aieDevice) ;
    if (object != nullptr)
      delete object ;
  }
} // end anonymous namespace

namespace xdp {
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;
  using severity_level = xrt_core::message::severity_level;

  AieProfile_EdgeImpl::AieProfile_EdgeImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata)
  {
    mCounterBases = {
      {module_type::core,     0},
      {module_type::dma,      BASE_MEMORY_COUNTER},
      {module_type::shim,     BASE_SHIM_COUNTER},
      {module_type::mem_tile, BASE_MEM_TILE_COUNTER}
    };

    mCoreStartEvents = {
      {"heat_map",                {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_GROUP_CORE_STALL_CORE,
                                   XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stalls",                  {XAIE_EVENT_MEMORY_STALL_CORE,         XAIE_EVENT_STREAM_STALL_CORE,
                                   XAIE_EVENT_LOCK_STALL_CORE,           XAIE_EVENT_CASCADE_STALL_CORE}},
      {"execution",               {XAIE_EVENT_INSTR_VECTOR_CORE,         XAIE_EVENT_INSTR_LOAD_CORE,
                                   XAIE_EVENT_INSTR_STORE_CORE,          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE}},
      {"stream_put_get",          {XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
                                   XAIE_EVENT_INSTR_STREAM_GET_CORE,     XAIE_EVENT_INSTR_STREAM_PUT_CORE}},
      {"write_throughputs",       {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_PUT_CORE,
                                   XAIE_EVENT_INSTR_CASCADE_PUT_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"read_throughputs",        {XAIE_EVENT_ACTIVE_CORE,               XAIE_EVENT_INSTR_STREAM_GET_CORE,
                                   XAIE_EVENT_INSTR_CASCADE_GET_CORE,    XAIE_EVENT_GROUP_CORE_STALL_CORE}},
      {"aie_trace",               {XAIE_EVENT_PORT_RUNNING_1_CORE,       XAIE_EVENT_PORT_STALLED_1_CORE,
                                   XAIE_EVENT_PORT_RUNNING_0_CORE,       XAIE_EVENT_PORT_STALLED_0_CORE}},
      {"events",                  {XAIE_EVENT_INSTR_EVENT_0_CORE,        XAIE_EVENT_INSTR_EVENT_1_CORE,
                                   XAIE_EVENT_USER_EVENT_0_CORE,         XAIE_EVENT_USER_EVENT_1_CORE}}
    };
    if (metadata->getHardwareGen() == 1) {
      mCoreStartEvents["floating_point"] = {XAIE_EVENT_FP_OVERFLOW_CORE, XAIE_EVENT_FP_UNDERFLOW_CORE,
                                            XAIE_EVENT_FP_INVALID_CORE,  XAIE_EVENT_FP_DIV_BY_ZERO_CORE};
    }
    else {
      mCoreStartEvents["floating_point"] = {XAIE_EVENT_FP_HUGE_CORE,     XAIE_EVENT_INT_FP_0_CORE, 
                                            XAIE_EVENT_FP_INVALID_CORE,  XAIE_EVENT_FP_INF_CORE};
    }
    mCoreEndEvents = mCoreStartEvents;

    // **** Memory Module Counters ****
    mMemoryStartEvents = {
      {"conflicts",               {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {"dma_locks",               {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM,    XAIE_EVENT_GROUP_LOCK_MEM}},
      {"write_throughputs",       {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM,
                                   XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}},
      {"read_throughputs",        {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM,
                                   XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}}
    };
    if (metadata->getHardwareGen() == 1) {
      mMemoryStartEvents["dma_stalls_s2mm"] = {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM,
                                               XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM};
      mMemoryStartEvents["dma_stalls_mm2s"] = {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM,
                                               XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM};
    }
    else {
      mMemoryStartEvents["dma_stalls_s2mm"] = {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM,
                                               XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM};
      mMemoryStartEvents["dma_stalls_mm2s"] = {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_MEM,
                                               XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_MEM};
    }
    mMemoryEndEvents = mMemoryStartEvents;

    // **** Interface Tile Counters ****
    mShimStartEvents = {
      {"input_throughputs",       {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"output_throughputs",      {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}},
      {"packets",                 {XAIE_EVENT_PORT_TLAST_0_PL,   XAIE_EVENT_PORT_TLAST_1_PL}}
    };
    mShimEndEvents = mShimStartEvents;

    // **** Memory Tile Counters ****
    mMemTileStartEvents = {
      {"input_channels",          {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE,   
                                   XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE}},
      {"input_channels_details",  {XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE, 
                                   XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE}},
      {"output_channels",         {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE,   
                                   XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE}},
      {"output_channels_details", {XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE, 
                                   XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE,
                                   XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE}},
      {"memory_stats",            {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM_TILE,
                                   XAIE_EVENT_GROUP_ERRORS_MEM_TILE,
                                   XAIE_EVENT_GROUP_LOCK_MEM_TILE,
                                   XAIE_EVENT_GROUP_WATCHPOINT_MEM_TILE}},
      {"mem_trace",               {XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, 
                                   XAIE_EVENT_PORT_STALLED_0_MEM_TILE,
                                   XAIE_EVENT_PORT_IDLE_0_MEM_TILE,
                                   XAIE_EVENT_PORT_TLAST_0_MEM_TILE}}
    };
    mMemTileEndEvents = mMemTileStartEvents;
  }

  bool AieProfile_EdgeImpl::checkAieDevice(uint64_t deviceId, void* handle)
  {
    aieDevInst = static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    aieDevice  = static_cast<xaiefal::XAieDev*>(db->getStaticInfo().getAieDevice(allocateAieDevice, deallocateAieDevice, handle)) ;
    if (!aieDevInst || !aieDevice) {
      xrt_core::message::send(severity_level::warning, "XRT", 
          "Unable to get AIE device. There will be no AIE profiling.");
      return false;
    }
    return true;
  }

  void AieProfile_EdgeImpl::updateDevice() {

      if(!checkAieDevice(metadata->getDeviceID(), metadata->getHandle()))
              return;

      bool runtimeCounters = setMetricsSettings(metadata->getDeviceID(), metadata->getHandle());
  
      if (!runtimeCounters) {
        std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(metadata->getHandle());
        auto counters = xrt_core::edge::aie::get_profile_counters(device.get());

        if (counters.empty()) {
          xrt_core::message::send(severity_level::warning, "XRT", 
            "AIE Profile Counters were not found for this design. Please specify tile_based_[aie|aie_memory|interface_tile]_metrics under \"AIE_profile_settings\" section in your xrt.ini.");
          (db->getStaticInfo()).setIsAIECounterRead(metadata->getDeviceID(),true);
          return;
        }
        else {
          XAie_DevInst* aieDevInst =
            static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, metadata->getHandle()));

          for (auto& counter : counters) {
            tile_type tile;
            auto payload = getCounterPayload(aieDevInst, tile, module_type::core, counter.column, 
                                             counter.row, counter.startEvent, "N/A", 0);

            (db->getStaticInfo()).addAIECounter(metadata->getDeviceID(), counter.id, counter.column,
                counter.row, counter.counterNumber, counter.startEvent, counter.endEvent,
                counter.resetEvent, payload, counter.clockFreqMhz, counter.module, counter.name);
          }
        }
      }
  }

  void AieProfile_EdgeImpl::configGroupEvents(XAie_DevInst* aieDevInst,
                                             const XAie_LocType loc,
                                             const XAie_ModuleType mod,
                                             const XAie_Events event,
                                             const std::string metricSet)
  {
    // Set masks for group events
    // NOTE: Group error enable register is blocked, so ignoring
    if (event == XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_DMA_MASK);
    else if (event == XAIE_EVENT_GROUP_LOCK_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_LOCK_MASK);
    else if (event == XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CONFLICT_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_PROGRAM_FLOW_MASK);
    else if (event == XAIE_EVENT_GROUP_CORE_STALL_CORE)
      XAie_EventGroupControl(aieDevInst, loc, mod, event, GROUP_CORE_STALL_MASK);
  }

  bool AieProfile_EdgeImpl::isStreamSwitchPortEvent(const XAie_Events event)
  {
    // AIE tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_CORE) 
        && (event < XAIE_EVENT_GROUP_BROADCAST_CORE))
      return true;
    // Interface tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_PL) 
        && (event < XAIE_EVENT_GROUP_BROADCAST_A_PL))
      return true;
    // Memory tiles
    if ((event > XAIE_EVENT_GROUP_STREAM_SWITCH_MEM_TILE) 
        && (event < XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM_TILE))
      return true;

    return false;
  }

  // Configure stream switch ports for monitoring purposes
  // NOTE: Used to monitor streams: trace, interfaces, and memory tiles
  void AieProfile_EdgeImpl::configStreamSwitchPorts(XAie_DevInst* aieDevInst,
                                                    const tile_type& tile,
                                                    xaiefal::XAieTile& xaieTile,
                                                    const XAie_LocType loc,
                                                    const module_type type,
                                                    const XAie_Events event,
                                                    const int countnum,
                                                    const std::string metricSet,
                                                    const uint8_t channel)
  {
    // Only configure as needed: must be applicable event and only need at most two
    if (!isStreamSwitchPortEvent(event) || (countnum > 1))
      return;

    auto switchPortRsc = xaieTile.sswitchPort();
    auto ret = switchPortRsc->reserve();
    if (ret != AieRC::XAIE_OK)
      return;

    uint32_t rscId = 0;
    XAie_LocType tmpLoc;
    XAie_ModuleType tmpMod;
    switchPortRsc->getRscId(tmpLoc, tmpMod, rscId);

    // AIE Tiles (e.g., trace streams)
    if (type == module_type::core) {
      // Define stream switch port to monitor core or memory trace
      uint8_t traceSelect = (event == XAIE_EVENT_PORT_RUNNING_0_CORE) ? 0 : 1;
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, XAIE_STRMSW_SLAVE, TRACE, traceSelect);
      return;
    }

    // Interface tiles (e.g., PLIO, GMIO)
    if (type == module_type::shim) {
      // Grab slave/master and stream ID
      // NOTE: stored in getTilesForProfiling() above
      auto slaveOrMaster = (tile.itr_mem_col == 0) ? XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      auto streamPortId  = static_cast<uint8_t>(tile.itr_mem_row);

      // Define stream switch port to monitor interface 
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, slaveOrMaster, SOUTH, streamPortId);
      return;
    }

    // Memory tiles
    if (metricSet.find("trace") != std::string::npos) {
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, XAIE_STRMSW_SLAVE, TRACE, 0);
    }
    else {
      auto slaveOrMaster = (metricSet.find("output") != std::string::npos) ?
        XAIE_STRMSW_SLAVE : XAIE_STRMSW_MASTER;
      XAie_EventSelectStrmPort(aieDevInst, loc, rscId, slaveOrMaster, DMA, channel);
    }

    mStreamPorts.push_back(switchPortRsc);
  }

  void 
  AieProfile_EdgeImpl::configEventSelections(XAie_DevInst* aieDevInst,
                                             const XAie_LocType loc,
                                             const XAie_ModuleType mod,
                                             const module_type type,
                                             const std::string metricSet,
                                             const uint8_t channel0,
                                             const uint8_t channel1) 
  {
    if (type != module_type::mem_tile)
      return;

    XAie_DmaDirection dmaDir = (metricSet.find("input") != std::string::npos) ? DMA_S2MM : DMA_MM2S;
    XAie_EventSelectDmaChannel(aieDevInst, loc, 0, dmaDir, channel0);
    XAie_EventSelectDmaChannel(aieDevInst, loc, 1, dmaDir, channel1);
  }

  // Get reportable payload specific for this tile and/or counter
  uint32_t 
  AieProfile_EdgeImpl::getCounterPayload(XAie_DevInst* aieDevInst, 
                                         const tile_type& tile, 
                                         const module_type type, 
                                         uint16_t column, 
                                         uint16_t row, 
                                         uint16_t startEvent, 
                                         const std::string metricSet,
                                         const uint8_t channel)
  {
    // 1. Stream IDs for interface tiles
    if (type == module_type::shim) {
      // NOTE: value = ((master or slave) << 8) & (stream ID)
      return ((tile.itr_mem_col << 8) | tile.itr_mem_row);
    }

    // 2. Channel IDs for memory tiles
    if (type == module_type::mem_tile) {
      // NOTE: value = ((master or slave) << 8) & (channel ID)
      uint8_t isMaster = (metricSet.find("input") != std::string::npos) ? 1 : 0;
      return ((isMaster << 8) | channel);
    }

    // 3. DMA BD sizes for AIE tiles
    if ((startEvent != XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM)
        && (startEvent != XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM))
      return 0;

    uint32_t payloadValue = 0;

    constexpr int NUM_BDS = 8;
    constexpr uint32_t BYTES_PER_WORD = 4;
    constexpr uint32_t ACTUAL_OFFSET = 1;
    uint64_t offsets[NUM_BDS] = {XAIEGBL_MEM_DMABD0CTRL,            XAIEGBL_MEM_DMABD1CTRL,
                                 XAIEGBL_MEM_DMABD2CTRL,            XAIEGBL_MEM_DMABD3CTRL,
                                 XAIEGBL_MEM_DMABD4CTRL,            XAIEGBL_MEM_DMABD5CTRL,
                                 XAIEGBL_MEM_DMABD6CTRL,            XAIEGBL_MEM_DMABD7CTRL};
    uint32_t lsbs[NUM_BDS]    = {XAIEGBL_MEM_DMABD0CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD1CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD2CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD3CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD4CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD5CTRL_LEN_LSB,
                                 XAIEGBL_MEM_DMABD6CTRL_LEN_LSB,    XAIEGBL_MEM_DMABD7CTRL_LEN_LSB};
    uint32_t masks[NUM_BDS]   = {XAIEGBL_MEM_DMABD0CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD1CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD2CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD3CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD4CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD5CTRL_LEN_MASK,
                                 XAIEGBL_MEM_DMABD6CTRL_LEN_MASK,   XAIEGBL_MEM_DMABD7CTRL_LEN_MASK};
    uint32_t valids[NUM_BDS]  = {XAIEGBL_MEM_DMABD0CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD1CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD2CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD3CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD4CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD5CTRL_VALBD_MASK,
                                 XAIEGBL_MEM_DMABD6CTRL_VALBD_MASK, XAIEGBL_MEM_DMABD7CTRL_VALBD_MASK};

    auto tileOffset = _XAie_GetTileAddr(aieDevInst, row, column);
    for (int bd = 0; bd < NUM_BDS; ++bd) {
      uint32_t regValue = 0;
      XAie_Read32(aieDevInst, tileOffset + offsets[bd], &regValue);
      
      if (regValue & valids[bd]) {
        uint32_t bdBytes = BYTES_PER_WORD * (((regValue >> lsbs[bd]) & masks[bd]) + ACTUAL_OFFSET);
        payloadValue = std::max(bdBytes, payloadValue);
      }
    }

    return payloadValue;
  }

  void AieProfile_EdgeImpl::printTileModStats(xaiefal::XAieDev* aieDevice, 
      const tile_type& tile, XAie_ModuleType mod)
  {
    auto col = tile.col;
    auto row = tile.row;
    auto loc = XAie_TileLoc(col, row);
    std::string moduleName = (mod == XAIE_CORE_MOD) ? "aie" 
                           : ((mod == XAIE_MEM_MOD) ? "aie_memory" 
                           : "interface_tile");
    const std::string groups[3] = {
      XAIEDEV_DEFAULT_GROUP_GENERIC,
      XAIEDEV_DEFAULT_GROUP_STATIC,
      XAIEDEV_DEFAULT_GROUP_AVAIL
    };

    std::stringstream msg;
    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : " << moduleName << std::endl;
    for (auto&g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);
      auto ts = stats.getNumRsc(loc, mod, xaiefal::XAIE_TRACE_EVENTS_RSC);
      auto bc = stats.getNumRsc(loc, mod, XAIE_BCAST_CHANNEL_RSC);
      msg << "Resource Group : " << std::left <<  std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " "
          << std::endl;
    }

    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  uint16_t AieProfile_EdgeImpl::getRelativeRow(uint16_t absRow)
  {
    auto rowOffset = metadata->getAIETileRowOffset();
    if (absRow == 0)
      return 0;
    if (absRow < rowOffset)
      return (absRow - 1);
    return (absRow - rowOffset);
  }

  module_type 
  AieProfile_EdgeImpl::getModuleType(uint16_t absRow, XAie_ModuleType mod)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < metadata->getAIETileRowOffset())
      return module_type::mem_tile;
    return ((mod == XAIE_CORE_MOD) ? module_type::core : module_type::dma);
  }

  bool AieProfile_EdgeImpl::isValidType(module_type type, XAie_ModuleType mod)
  {
    if ((mod == XAIE_CORE_MOD) && ((type == module_type::core) 
        || (type == module_type::dma)))
      return true;
    if ((mod == XAIE_MEM_MOD) && ((type == module_type::dma) 
        || (type == module_type::mem_tile)))
      return true;
    if ((mod == XAIE_PL_MOD) && (type == module_type::shim)) 
      return true;
    return false;
  }

  // Set metrics for all specified AIE counters on this device with configs given in AIE_profile_settings
  bool 
  AieProfile_EdgeImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    int counterId = 0;
    bool runtimeCounters = false;
    XAie_ModuleType falModuleTypes[metadata->getNumModules()] = 
        {XAIE_CORE_MOD, XAIE_MEM_MOD, XAIE_PL_MOD, XAIE_MEM_MOD};

    auto stats = aieDevice->getRscStat(XAIEDEV_DEFAULT_GROUP_AVAIL);
    auto configChannel0 = metadata->getConfigChannel0();
    auto configChannel1 = metadata->getConfigChannel1();

    for (int module = 0; module < metadata->getNumModules(); ++module) {
      int numTileCounters[metadata->getNumCountersMod(module)+1] = {0};
      XAie_ModuleType mod = falModuleTypes[module];

      // Iterate over tiles and metrics to configure all desired counters
      for (auto& tileMetric : metadata->getConfigMetrics(module)) {
        auto tile        = tileMetric.first;
        auto col         = tile.col;
        auto row         = tile.row;
        auto type        = getModuleType(row, mod);
        if (!isValidType(type, mod))
          continue;

        auto& metricSet  = tileMetric.second;
        auto loc         = XAie_TileLoc(col, row);
        auto& xaieTile   = aieDevice->tile(col, row);
        auto xaieModule  = (mod == XAIE_CORE_MOD) ? xaieTile.core()
                         : ((mod == XAIE_MEM_MOD) ? xaieTile.mem() 
                         : xaieTile.pl());

        auto startEvents = (type  == module_type::core) ? mCoreStartEvents[metricSet]
                         : ((type == module_type::dma)  ? mMemoryStartEvents[metricSet]
                         : ((type == module_type::shim) ? mShimStartEvents[metricSet]
                         : mMemTileStartEvents[metricSet]));
        auto endEvents   = (type  == module_type::core) ? mCoreEndEvents[metricSet]
                         : ((type == module_type::dma)  ? mMemoryEndEvents[metricSet]
                         : ((type == module_type::shim) ? mShimEndEvents[metricSet]
                         : mMemTileEndEvents[metricSet]));

        int numCounters  = 0;
        auto numFreeCtr  = stats.getNumRsc(loc, mod, XAIE_PERFCNT_RSC);

        // Specify Sel0/Sel1 for memory tile events 21-44
        auto iter0 = configChannel0.find(tile);
        auto iter1 = configChannel1.find(tile);
        uint8_t channel0 = (iter0 == configChannel0.end()) ? 0 : iter0->second;
        uint8_t channel1 = (iter1 == configChannel1.end()) ? 1 : iter1->second;
        configEventSelections(aieDevInst, loc, XAIE_MEM_MOD, type, metricSet, channel0, channel1);

        // Request and configure all available counters for this tile
        for (int i=0; i < numFreeCtr; ++i) {
          auto startEvent    = startEvents.at(i);
          auto endEvent      = endEvents.at(i);
          uint8_t resetEvent = 0;

          // Request counter from resource manager
          auto perfCounter = xaieModule.perfCounter();
          auto ret = perfCounter->initialize(mod, startEvent, mod, endEvent);
          if (ret != XAIE_OK) break;
          ret = perfCounter->reserve();
          if (ret != XAIE_OK) break;
        
          // Channel number is based on monitoring port 0 or 1
          auto channel = (startEvent <= XAIE_EVENT_PORT_TLAST_0_MEM_TILE) ? channel0 : channel1;

          configGroupEvents(aieDevInst, loc, mod, startEvent, metricSet);
          configStreamSwitchPorts(aieDevInst, tileMetric.first, xaieTile, loc, type,
                                  startEvent, i, metricSet, channel);
        
          // Start the counters after group events have been configured
          ret = perfCounter->start();
          if (ret != XAIE_OK) break;
          mPerfCounters.push_back(perfCounter);

          // Convert enums to physical event IDs for reporting purposes
          uint8_t tmpStart;
          uint8_t tmpEnd;
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod, startEvent, &tmpStart);
          XAie_EventLogicalToPhysicalConv(aieDevInst, loc, mod,   endEvent, &tmpEnd);
          uint16_t phyStartEvent = tmpStart + mCounterBases[type];
          uint16_t phyEndEvent   = tmpEnd   + mCounterBases[type];
          auto payload = getCounterPayload(aieDevInst, tileMetric.first, type, col, row, 
                                           startEvent, metricSet, channel);

          // Store counter info in database
          std::string counterName = "AIE Counter " + std::to_string(counterId);
          (db->getStaticInfo()).addAIECounter(deviceId, counterId, col, row, i,
                phyStartEvent, phyEndEvent, resetEvent, payload, metadata->getClockFreqMhz(), 
                metadata->getModuleName(module), counterName);
          counterId++;
          numCounters++;
        }

        std::stringstream msg;
        msg << "Reserved " << numCounters << " counters for profiling AIE tile (" << col << "," 
            << row << ") using metric set " << metricSet << ".";
        xrt_core::message::send(severity_level::debug, "XRT", msg.str());
        numTileCounters[numCounters]++;
      }
    
      // Report counters reserved per tile
      {
        std::stringstream msg;
        msg << "AIE profile counters reserved in " << metadata->getModuleName(module) << " - ";
        for (int n=0; n <= metadata->getNumCountersMod(module); ++n) {
          if (numTileCounters[n] == 0) continue;
          msg << n << ": " << numTileCounters[n] << " tiles";
          if (n != metadata->getNumCountersMod(module)) msg << ", ";

          (db->getStaticInfo()).addAIECounterResources(deviceId, n, numTileCounters[n], module);
        }
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }

      runtimeCounters = true;
    } // modules

    return runtimeCounters;
  }

  void AieProfile_EdgeImpl::poll(uint32_t index, void* handle)
  {
    // Wait until xclbin has been loaded and device has been updated in database
    if (!(db->getStaticInfo().isDeviceReady(index)))
      return;
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle)) ;
    if (!aieDevInst)
      return;

    uint32_t prevColumn = 0;
    uint32_t prevRow = 0;
    uint64_t timerValue = 0;

    // Iterate over all AIE Counters & Timers
    auto numCounters = db->getStaticInfo().getNumAIECounter(index);
    for (uint64_t c=0; c < numCounters; c++) {
      auto aie = db->getStaticInfo().getAIECounter(index, c);
      if (!aie)
        continue;

      std::vector<uint64_t> values;
      values.push_back(aie->column);
      values.push_back(getRelativeRow(aie->row));
      values.push_back(aie->startEvent);
      values.push_back(aie->endEvent);
      values.push_back(aie->resetEvent);

      // Read counter value from device
      uint32_t counterValue;
      if (mPerfCounters.empty()) {
        // Compiler-defined counters
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row);
        XAie_PerfCounterGet(aieDevInst, tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
      }
      else {
        // Runtime-defined counters
        auto perfCounter = mPerfCounters.at(c);
        perfCounter->readResult(counterValue);
      }
      values.push_back(counterValue);

      // Read tile timer (once per tile to minimize overhead)
      if ((aie->column != prevColumn) || (aie->row != prevRow)) {
        prevColumn = aie->column;
        prevRow = aie->row;
        auto moduleType = getModuleType(aie->row, XAIE_CORE_MOD);
        auto falModuleType =  (moduleType == module_type::core) ? XAIE_CORE_MOD 
                            : ((moduleType == module_type::shim) ? XAIE_PL_MOD 
                            : XAIE_MEM_MOD);
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row);
        XAie_ReadTimer(aieDevInst, tileLocation, falModuleType, &timerValue);
      }
      values.push_back(timerValue);
      values.push_back(aie->payload);

      // Get timestamp in milliseconds
      double timestamp = xrt_core::time_ns() / 1.0e6;
      db->getDynamicInfo().addAIESample(index, timestamp, values);
    }
  }

  void AieProfile_EdgeImpl::freeResources() 
  {
    for (auto& c : mPerfCounters){
      c->stop();
      c->release();
    }

    for (auto& c : mStreamPorts){
      c->stop();
      c->release();
    }
  }

}