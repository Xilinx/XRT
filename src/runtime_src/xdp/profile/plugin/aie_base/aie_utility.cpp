/**
 * Copyright (C) 2025 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include <set>
#include "xdp/profile/plugin/aie_base/aie_utility.h"

namespace xdp::aie {
  
  // ***************************************************************
  // Determine hardware generation
  // ***************************************************************
  bool isAIE1(const int hwGen)
  {
    return (hwGen == XAIE_DEV_GEN_AIE);
  }

  bool isAIE2(const int hwGen)
  {
    //return ((hwGen > XAIE_DEV_GEN_AIE) && (hwGen <= XAIE_DEV_GEN_AIE2P_STRIX_B0)
    //        && (hwGen != XAIE_DEV_GEN_AIE2PS));
    return ((hwGen > XAIE_DEV_GEN_AIE) && (hwGen <= 9) && (hwGen != 5));
  }

  bool isAIE2ps(const int hwGen)
  {
    //return (hwGen == XAIE_DEV_GEN_AIE2PS);
    return (hwGen == 5);
  }

  bool isNPU3(const int hwGen)
  {
    // TODO: replace with enum once available 
    return (hwGen >= 40);
  }

  bool isMicroSupported(const int hwGen)
  {
    return (isAIE2ps(hwGen) || isNPU3(hwGen));
  }

  /****************************************************************************
   * Check if metric set contains DMA events
   * TODO: Traverse events vector instead of based on name
   ***************************************************************************/
  bool isDmaSet(const std::string metricSet)
  {
    if ((metricSet.find("dma") != std::string::npos)
        || (metricSet.find("s2mm") != std::string::npos)
        || (metricSet.find("mm2s") != std::string::npos))
      return true;
    return false;
  }
 
  /****************************************************************************
   * Check if core module event
   ***************************************************************************/
  bool isCoreModuleEvent(const XAie_Events event)
  {
    return ((event >= XAIE_EVENT_NONE_CORE) 
            && (event <= XAIE_EVENT_INSTR_ERROR_CORE));
  }
 
  /****************************************************************************
   * Check if stream switch port event
   ***************************************************************************/
   bool isStreamSwitchPortEvent(const XAie_Events event)
   {
     if (isPortRunningEvent(event) || isPortStalledEvent(event) ||
         isPortIdleEvent(event) || isPortTlastEvent(event))
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
   * Check if port stalled event
   ***************************************************************************/
  bool isPortStalledEvent(const XAie_Events event)
  {
    std::set<XAie_Events> stalledEvents = {
      XAIE_EVENT_PORT_STALLED_0_CORE,     XAIE_EVENT_PORT_STALLED_1_CORE,
      XAIE_EVENT_PORT_STALLED_2_CORE,     XAIE_EVENT_PORT_STALLED_3_CORE,
      XAIE_EVENT_PORT_STALLED_4_CORE,     XAIE_EVENT_PORT_STALLED_5_CORE,
      XAIE_EVENT_PORT_STALLED_6_CORE,     XAIE_EVENT_PORT_STALLED_7_CORE,
      XAIE_EVENT_PORT_STALLED_0_PL,       XAIE_EVENT_PORT_STALLED_1_PL,
      XAIE_EVENT_PORT_STALLED_2_PL,       XAIE_EVENT_PORT_STALLED_3_PL,
      XAIE_EVENT_PORT_STALLED_4_PL,       XAIE_EVENT_PORT_STALLED_5_PL,
      XAIE_EVENT_PORT_STALLED_6_PL,       XAIE_EVENT_PORT_STALLED_7_PL,
      XAIE_EVENT_PORT_STALLED_0_MEM_TILE, XAIE_EVENT_PORT_STALLED_1_MEM_TILE,
      XAIE_EVENT_PORT_STALLED_2_MEM_TILE, XAIE_EVENT_PORT_STALLED_3_MEM_TILE,
      XAIE_EVENT_PORT_STALLED_4_MEM_TILE, XAIE_EVENT_PORT_STALLED_5_MEM_TILE,
      XAIE_EVENT_PORT_STALLED_6_MEM_TILE, XAIE_EVENT_PORT_STALLED_7_MEM_TILE
    };

    return (stalledEvents.find(event) != stalledEvents.end());
  }

  /****************************************************************************
   * Check if port idle event
   ***************************************************************************/
  bool isPortIdleEvent(const XAie_Events event)
  {
    std::set<XAie_Events> idleEvents = {
      XAIE_EVENT_PORT_IDLE_0_CORE,     XAIE_EVENT_PORT_IDLE_1_CORE,
      XAIE_EVENT_PORT_IDLE_2_CORE,     XAIE_EVENT_PORT_IDLE_3_CORE,
      XAIE_EVENT_PORT_IDLE_4_CORE,     XAIE_EVENT_PORT_IDLE_5_CORE,
      XAIE_EVENT_PORT_IDLE_6_CORE,     XAIE_EVENT_PORT_IDLE_7_CORE,
      XAIE_EVENT_PORT_IDLE_0_PL,       XAIE_EVENT_PORT_IDLE_1_PL,
      XAIE_EVENT_PORT_IDLE_2_PL,       XAIE_EVENT_PORT_IDLE_3_PL,
      XAIE_EVENT_PORT_IDLE_4_PL,       XAIE_EVENT_PORT_IDLE_5_PL,
      XAIE_EVENT_PORT_IDLE_6_PL,       XAIE_EVENT_PORT_IDLE_7_PL,
      XAIE_EVENT_PORT_IDLE_0_MEM_TILE, XAIE_EVENT_PORT_IDLE_1_MEM_TILE,
      XAIE_EVENT_PORT_IDLE_2_MEM_TILE, XAIE_EVENT_PORT_IDLE_3_MEM_TILE,
      XAIE_EVENT_PORT_IDLE_4_MEM_TILE, XAIE_EVENT_PORT_IDLE_5_MEM_TILE,
      XAIE_EVENT_PORT_IDLE_6_MEM_TILE, XAIE_EVENT_PORT_IDLE_7_MEM_TILE
    };

    return (idleEvents.find(event) != idleEvents.end());
  }

  /****************************************************************************
   * Check if port Tlast event
   ***************************************************************************/
  bool isPortTlastEvent(const XAie_Events event)
  {
    std::set<XAie_Events> tlastEvents = {
      XAIE_EVENT_PORT_TLAST_0_CORE,     XAIE_EVENT_PORT_TLAST_1_CORE,
      XAIE_EVENT_PORT_TLAST_2_CORE,     XAIE_EVENT_PORT_TLAST_3_CORE,
      XAIE_EVENT_PORT_TLAST_4_CORE,     XAIE_EVENT_PORT_TLAST_5_CORE,
      XAIE_EVENT_PORT_TLAST_6_CORE,     XAIE_EVENT_PORT_TLAST_7_CORE,
      XAIE_EVENT_PORT_TLAST_0_PL,       XAIE_EVENT_PORT_TLAST_1_PL,
      XAIE_EVENT_PORT_TLAST_2_PL,       XAIE_EVENT_PORT_TLAST_3_PL,
      XAIE_EVENT_PORT_TLAST_4_PL,       XAIE_EVENT_PORT_TLAST_5_PL,
      XAIE_EVENT_PORT_TLAST_6_PL,       XAIE_EVENT_PORT_TLAST_7_PL,
      XAIE_EVENT_PORT_TLAST_0_MEM_TILE, XAIE_EVENT_PORT_TLAST_1_MEM_TILE,
      XAIE_EVENT_PORT_TLAST_2_MEM_TILE, XAIE_EVENT_PORT_TLAST_3_MEM_TILE,
      XAIE_EVENT_PORT_TLAST_4_MEM_TILE, XAIE_EVENT_PORT_TLAST_5_MEM_TILE,
      XAIE_EVENT_PORT_TLAST_6_MEM_TILE, XAIE_EVENT_PORT_TLAST_7_MEM_TILE
    };

    return (tlastEvents.find(event) != tlastEvents.end());
  }

  /****************************************************************************
   * Get port number based on event
   ***************************************************************************/
  uint8_t getPortNumberFromEvent(const XAie_Events event)
  {
    switch (event) {
    case XAIE_EVENT_PORT_RUNNING_7_CORE:
    case XAIE_EVENT_PORT_STALLED_7_CORE:
    case XAIE_EVENT_PORT_IDLE_7_CORE:
    case XAIE_EVENT_PORT_RUNNING_7_PL:
    case XAIE_EVENT_PORT_STALLED_7_PL:
    case XAIE_EVENT_PORT_IDLE_7_PL:
      return 7;
    case XAIE_EVENT_PORT_RUNNING_6_CORE:
    case XAIE_EVENT_PORT_STALLED_6_CORE:
    case XAIE_EVENT_PORT_IDLE_6_CORE:
    case XAIE_EVENT_PORT_RUNNING_6_PL:
    case XAIE_EVENT_PORT_STALLED_6_PL:
    case XAIE_EVENT_PORT_IDLE_6_PL:
      return 6;
    case XAIE_EVENT_PORT_RUNNING_5_CORE:
    case XAIE_EVENT_PORT_STALLED_5_CORE:
    case XAIE_EVENT_PORT_IDLE_5_CORE:
    case XAIE_EVENT_PORT_RUNNING_5_PL:
    case XAIE_EVENT_PORT_STALLED_5_PL:
    case XAIE_EVENT_PORT_IDLE_5_PL:
      return 5;
    case XAIE_EVENT_PORT_RUNNING_4_CORE:
    case XAIE_EVENT_PORT_STALLED_4_CORE:
    case XAIE_EVENT_PORT_IDLE_4_CORE:
    case XAIE_EVENT_PORT_RUNNING_4_PL:
    case XAIE_EVENT_PORT_STALLED_4_PL:
    case XAIE_EVENT_PORT_IDLE_4_PL:
      return 4;
    case XAIE_EVENT_PORT_RUNNING_3_CORE:
    case XAIE_EVENT_PORT_STALLED_3_CORE:
    case XAIE_EVENT_PORT_IDLE_3_CORE:
    case XAIE_EVENT_PORT_RUNNING_3_PL:
    case XAIE_EVENT_PORT_STALLED_3_PL:
    case XAIE_EVENT_PORT_IDLE_3_PL:
      return 3;
    case XAIE_EVENT_PORT_RUNNING_2_CORE:
    case XAIE_EVENT_PORT_STALLED_2_CORE:
    case XAIE_EVENT_PORT_IDLE_2_CORE:
    case XAIE_EVENT_PORT_RUNNING_2_PL:
    case XAIE_EVENT_PORT_STALLED_2_PL:
    case XAIE_EVENT_PORT_IDLE_2_PL:
      return 2;
    case XAIE_EVENT_PORT_RUNNING_1_CORE:
    case XAIE_EVENT_PORT_STALLED_1_CORE:
    case XAIE_EVENT_PORT_IDLE_1_CORE:
    case XAIE_EVENT_PORT_RUNNING_1_PL:
    case XAIE_EVENT_PORT_STALLED_1_PL:
    case XAIE_EVENT_PORT_IDLE_1_PL:
      return 1;
    default:
      return 0;
    }
  }

  /****************************************************************************
   * Get channel number based on event
   * NOTE: This only covers AIE Tiles and Interface Tiles
   ***************************************************************************/
   int8_t getChannelNumberFromEvent(XAie_Events event)
   {
     switch (event) {
     case XAIE_EVENT_DMA_S2MM_0_START_TASK_MEM:
     case XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM:
     case XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_MEM:
     case XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM:
     case XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_MEM:
     case XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_MEM:
     case XAIE_EVENT_DMA_MM2S_0_START_TASK_MEM:
     case XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM:
     case XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_MEM:
     case XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_MEM:
     case XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_MEM:
     case XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_MEM:
     case XAIE_EVENT_DMA_S2MM_0_START_BD_PL:
     case XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_PL:
     case XAIE_EVENT_DMA_S2MM_0_START_TASK_PL:
     case XAIE_EVENT_DMA_S2MM_0_FINISHED_TASK_PL:
     case XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_PL:
     case XAIE_EVENT_DMA_S2MM_0_STREAM_STARVATION_PL:
     case XAIE_EVENT_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL:
     case XAIE_EVENT_DMA_MM2S_0_START_BD_PL:
     case XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_PL:
     case XAIE_EVENT_DMA_MM2S_0_START_TASK_PL:
     case XAIE_EVENT_DMA_MM2S_0_FINISHED_TASK_PL:
     case XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_PL:
     case XAIE_EVENT_DMA_MM2S_0_STREAM_BACKPRESSURE_PL:
     case XAIE_EVENT_DMA_MM2S_0_MEMORY_STARVATION_PL:
 #ifdef XDP_VE2_BUILD
     case XAIE_EVENT_NOC0_DMA_S2MM_0_START_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_0_FINISHED_BD_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_0_FINISHED_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_0_STALLED_LOCK_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_0_STREAM_STARVATION_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_0_MEMORY_BACKPRESSURE_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_0_START_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_0_FINISHED_BD_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_0_FINISHED_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_0_STALLED_LOCK_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_0_STREAM_BACKPRESSURE_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_0_MEMORY_STARVATION_PL:
 #endif
       return 0;
     case XAIE_EVENT_DMA_S2MM_1_START_TASK_MEM:
     case XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM:
     case XAIE_EVENT_DMA_S2MM_1_FINISHED_TASK_MEM:
     case XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM:
     case XAIE_EVENT_DMA_S2MM_1_STREAM_STARVATION_MEM:
     case XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_MEM:
     case XAIE_EVENT_DMA_MM2S_1_START_TASK_MEM:
     case XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM:
     case XAIE_EVENT_DMA_MM2S_1_FINISHED_TASK_MEM:
     case XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_MEM:
     case XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_MEM:
     case XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_MEM:
     case XAIE_EVENT_DMA_S2MM_1_START_BD_PL:
     case XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_PL:
     case XAIE_EVENT_DMA_S2MM_1_START_TASK_PL:
     case XAIE_EVENT_DMA_S2MM_1_FINISHED_TASK_PL:
     case XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_PL:
     case XAIE_EVENT_DMA_S2MM_1_STREAM_STARVATION_PL:
     case XAIE_EVENT_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL:
     case XAIE_EVENT_DMA_MM2S_1_START_BD_PL:
     case XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_PL:
     case XAIE_EVENT_DMA_MM2S_1_START_TASK_PL:
     case XAIE_EVENT_DMA_MM2S_1_FINISHED_TASK_PL:
     case XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_PL:
     case XAIE_EVENT_DMA_MM2S_1_STREAM_BACKPRESSURE_PL:
     case XAIE_EVENT_DMA_MM2S_1_MEMORY_STARVATION_PL:
#ifdef XDP_VE2_BUILD
     case XAIE_EVENT_NOC0_DMA_S2MM_1_START_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_1_FINISHED_BD_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_1_FINISHED_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_1_STALLED_LOCK_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_1_STREAM_STARVATION_PL:
     case XAIE_EVENT_NOC0_DMA_S2MM_1_MEMORY_BACKPRESSURE_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_1_START_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_1_FINISHED_BD_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_1_FINISHED_TASK_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_1_STALLED_LOCK_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_1_STREAM_BACKPRESSURE_PL:
     case XAIE_EVENT_NOC0_DMA_MM2S_1_MEMORY_STARVATION_PL:
#endif
       return 1;
     default:
       return -1;
     }
   }
 
} // namespace xdp::aie
