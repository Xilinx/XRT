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

#include "xdp/profile/plugin/aie_trace/aie_trace_util.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>
#include <set>

#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp::aie::trace {
  using severity_level = xrt_core::message::severity_level;

  /****************************************************************************
   * Check if input-based metric set
   ***************************************************************************/
  bool isInputSet(const module_type type, const std::string metricSet)
  {
    // Catch memory tile sets
    if (type == module_type::mem_tile) {
      if ((metricSet.find("input") != std::string::npos)
          || (metricSet.find("s2mm") != std::string::npos))
        return true;
      else
        return false;
    }

    // Remaining covers interface tiles
    if ((metricSet.find("input") != std::string::npos)
        || (metricSet.find("mm2s") != std::string::npos))
      return true;
    else
      return false;
  }

  /****************************************************************************
   * Check if stream switch port event
   ***************************************************************************/
  bool isStreamSwitchPortEvent(const XAie_Events event)
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

  /****************************************************************************
   * Check if port running event
   ***************************************************************************/
  bool isPortRunningEvent(const XAie_Events event)
  {
    std::set<XAie_Events> runningEvents = {
      XAIE_EVENT_PORT_RUNNING_0_CORE,     XAIE_EVENT_PORT_RUNNING_1_CORE,
      XAIE_EVENT_PORT_RUNNING_2_CORE,     XAIE_EVENT_PORT_RUNNING_3_CORE,
      XAIE_EVENT_PORT_RUNNING_4_CORE,     XAIE_EVENT_PORT_RUNNING_5_CORE,
      XAIE_EVENT_PORT_RUNNING_6_CORE,     XAIE_EVENT_PORT_RUNNING_7_CORE,
      XAIE_EVENT_PORT_RUNNING_0_PL,       XAIE_EVENT_PORT_RUNNING_1_PL,
      XAIE_EVENT_PORT_RUNNING_2_PL,       XAIE_EVENT_PORT_RUNNING_3_PL,
      XAIE_EVENT_PORT_RUNNING_4_PL,       XAIE_EVENT_PORT_RUNNING_5_PL,
      XAIE_EVENT_PORT_RUNNING_6_PL,       XAIE_EVENT_PORT_RUNNING_7_PL,
      XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, XAIE_EVENT_PORT_RUNNING_1_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_2_MEM_TILE, XAIE_EVENT_PORT_RUNNING_3_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_4_MEM_TILE, XAIE_EVENT_PORT_RUNNING_5_MEM_TILE,
      XAIE_EVENT_PORT_RUNNING_6_MEM_TILE, XAIE_EVENT_PORT_RUNNING_7_MEM_TILE
    };

    return (runningEvents.find(event) != runningEvents.end());
  }

  /****************************************************************************
   * Get relative row of given tile
   ***************************************************************************/
  uint16_t getRelativeRow(uint16_t absRow, uint16_t rowOffset)
  {
    if (absRow == 0)
      return 0;
    if (absRow < rowOffset)
      return (absRow - 1);
    return (absRow - rowOffset);
  }

  /****************************************************************************
   * Get module type
   ***************************************************************************/
  module_type getModuleType(uint16_t absRow, uint16_t rowOffset)
  {
    if (absRow == 0)
      return module_type::shim;
    if (absRow < rowOffset)
      return module_type::mem_tile;
    return module_type::core;
  }

  /****************************************************************************
   * Get port number based on event
   ***************************************************************************/
  uint8_t getPortNumberFromEvent(XAie_Events event)
  {
    switch (event) {
    case XAIE_EVENT_PORT_RUNNING_3_PL:
    case XAIE_EVENT_PORT_STALLED_3_PL:
      return 3;
    case XAIE_EVENT_PORT_RUNNING_2_PL:
    case XAIE_EVENT_PORT_STALLED_2_PL:
      return 2;
    case XAIE_EVENT_PORT_RUNNING_1_PL:
    case XAIE_EVENT_PORT_STALLED_1_PL:
      return 1;
    default:
      return 0;
    }
  }

  /****************************************************************************
   * Convert broadcast ID to event ID
   ***************************************************************************/
  uint32_t bcIdToEvent(int bcId)
  {
    return bcId + CORE_BROADCAST_EVENT_BASE;
  }
  
  /****************************************************************************
   * Get module name
   ***************************************************************************/
  std::string getModuleName(module_type mod)
  {
    static std::map<module_type, std::string> modNames {
      {module_type::core,     "AIE modules"},
      {module_type::dma,      "AIE tile memory modules"},
      {module_type::shim,     "interface tiles"},
      {module_type::mem_tile, "memory tiles"}
    };

    return modNames[mod];
  }

  /****************************************************************************
   * Print out resource usage statistics for a given tile
   ***************************************************************************/
  void printTileStats(xaiefal::XAieDev* aieDevice, const tile_type& tile)
  {
    if (xrt_core::config::get_verbosity() < static_cast<uint32_t>(severity_level::info))
      return;

    auto col = tile.col;
    auto row = tile.row;
    auto loc = XAie_TileLoc(col, row);
    std::stringstream msg;

    const std::string groups[3] = {
      XAIEDEV_DEFAULT_GROUP_GENERIC,
      XAIEDEV_DEFAULT_GROUP_STATIC, 
      XAIEDEV_DEFAULT_GROUP_AVAIL
    };

    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : Core" << std::endl;
    for (auto& g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_PERFCNT_RSC);
      auto ts = stats.getNumRsc(loc, XAIE_CORE_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
      auto bc = stats.getNumRsc(loc, XAIE_CORE_MOD, XAIE_BCAST_CHANNEL_RSC);
      msg << "Resource Group : " << std::left << std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " " 
          << std::endl;
    }
    msg << "Resource usage stats for Tile : (" << col << "," << row << ") Module : Memory" << std::endl;
    for (auto& g : groups) {
      auto stats = aieDevice->getRscStat(g);
      auto pc = stats.getNumRsc(loc, XAIE_MEM_MOD, XAIE_PERFCNT_RSC);
      auto ts = stats.getNumRsc(loc, XAIE_MEM_MOD, xaiefal::XAIE_TRACE_EVENTS_RSC);
      auto bc = stats.getNumRsc(loc, XAIE_MEM_MOD, XAIE_BCAST_CHANNEL_RSC);
      msg << "Resource Group : " << std::left << std::setw(10) << g << " "
          << "Performance Counters : " << pc << " "
          << "Trace Slots : " << ts << " "
          << "Broadcast Channels : " << bc << " " 
          << std::endl;
    }
    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  /****************************************************************************
   * Print out reserved trace events
   ***************************************************************************/
  void printTraceEventStats(int m, int numTiles[])
  {
    if (xrt_core::config::get_verbosity() < static_cast<uint32_t>(severity_level::info))
      return;

    auto modName = getModuleName(static_cast<module_type>(m));

    std::stringstream msg;
    msg << "AIE trace events reserved in " << modName << " - ";
    for (int n = 0; n <= NUM_TRACE_EVENTS; ++n) {
      if (numTiles[n] == 0)
        continue;
      msg << n << ": " << numTiles[n] << " tiles, ";
    }

    xrt_core::message::send(severity_level::info, "XRT", msg.str());
  }

  /****************************************************************************
   * Modify events in metric set based on type and channel
   ***************************************************************************/
  void modifyEvents(module_type type, uint16_t subtype, const std::string metricSet,
                    uint8_t channel, std::vector<XAie_Events>& events)
  {
    // Only needed for GMIO DMA channel 1
    if ((type != module_type::shim) || (subtype == 0) || (channel == 0))
      return;

    // Check type to minimize replacements
    if (isInputSet(type, metricSet)) {
      // Input or MM2S
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_START_TASK_PL,          XAIE_EVENT_DMA_MM2S_1_START_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL,         XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL,       XAIE_EVENT_DMA_MM2S_1_FINISHED_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL,        XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL, XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL,   XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_PL);
    }
    else {
      // Output or S2MM
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_START_TASK_PL,          XAIE_EVENT_DMA_S2MM_1_START_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL,         XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL,       XAIE_EVENT_DMA_S2MM_1_FINISHED_TASK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL,        XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL,   XAIE_EVENT_DMA_S2MM_1_STREAM_STARVATION_PL);
      std::replace(events.begin(), events.end(), 
          XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL, XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL);
    }
  }
}  // namespace xdp
