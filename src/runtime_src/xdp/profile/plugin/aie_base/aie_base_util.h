// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_BASE_UTIL_DOT_H
#define AIE_BASE_UTIL_DOT_H

#include <cstdint>
#include <string>
#include <set>
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_base/generations/aie_generations.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie {

  /**
   * @brief   Check if HW generation is AIE1
   * @param   hwGen integer representing the hardware generation
   * @return  true if HW generation is AIE1
   */
  inline bool isAIE1(const int hwGen)
  {
    return (hwGen == XAIE_DEV_GEN_AIE);
  }

  /**
   * @brief   Check if HW generation is AIE2
   * @param   hwGen integer representing the hardware generation
   * @return  true if HW generation is AIE2
   */
  inline bool isAIE2(const int hwGen)
  {
    //return ((hwGen > XAIE_DEV_GEN_AIE) && (hwGen <= XAIE_DEV_GEN_AIE2P_STRIX_B0)
    //        && (hwGen != XAIE_DEV_GEN_AIE2PS));
    return ((hwGen > 1) && (hwGen <= 9) && (hwGen != 5));
  }

  /**
   * @brief   Check if HW generation is AIE2ps
   * @param   hwGen integer representing the hardware generation
   * @return  true if HW generation is AIE2ps
   */
  inline bool isAIE2ps(const int hwGen)
  {
    //return (hwGen == XAIE_DEV_GEN_AIE2PS);
    return (hwGen == 5);
  }

  /**
   * @brief   Check if HW generation is NPU3
   * @param   hwGen integer representing the hardware generation
   * @return  true if HW generation is NPU3
   */
  inline bool isNPU3(const int hwGen)
  {
    return (hwGen >= 40);
  }
  
  /**
   * @brief   Check if microcontrollers are supported
   * @param   hwGen integer representing the hardware generation
   * @return  true if microcontrollers are available on specified generation
   */
  inline bool isMicroSupported(const int hwGen)
  {
    return (isAIE2ps(hwGen) || isNPU3(hwGen));
  }

  /**
   * @brief   Get HW generation-specific number of performance counters
   * @note    This function currently supports AIE1 and AIE2*
   * @param   hwGen integer representing the hardware generation
   * @param   mod module type
   * @return  number of counters available in the module
   */
  inline unsigned int getNumCounters(const int hwGen, xdp::module_type mod)
  {
    if (mod == xdp::module_type::core) {
      return (xdp::aie::isAIE2ps(hwGen)    ? aie2ps::cm_num_counters
                : aie2::cm_num_counters);
    }
    if (mod == xdp::module_type::dma) {
      return (xdp::aie::isAIE2ps(hwGen)    ? aie2ps::mm_num_counters
                : aie2::mm_num_counters);
    }
    if (mod == xdp::module_type::shim) {
      return (xdp::aie::isAIE2ps(hwGen)    ? aie2ps::shim_num_counters
                : aie2::shim_num_counters);
    }
    if (mod == xdp::module_type::mem_tile) {
      return (xdp::aie::isAIE2ps(hwGen)    ? aie2ps::mem_num_counters
                : aie2::mem_num_counters);
    }
    return 0;
  }

  /**
   * @brief   Get HW generation-specific stream bit width
   * @note    This function currently supports AIE1 and AIE2*
   * @param   hwGen integer representing the hardware generation
   * @return  bit width of streams in the array
   */
  inline unsigned int getStreamBitWidth(const int hwGen)
  {
    return (xdp::aie::isAIE2ps(hwGen)    ? aie2ps::stream_bit_width
              : aie2::stream_bit_width);
  }

  /**
   * @brief   Get HW generation-specific cascade bit width
   * @note    This function currently supports AIE1 and AIE2*
   * @param   hwGen integer representing the hardware generation
   * @return  bit width of cascades in the array
   */
  inline unsigned int getCascadeBitWidth(const int hwGen)
  {
    return (xdp::aie::isAIE2ps(hwGen)    ? aie2ps::cascade_bit_width
              : aie2::cascade_bit_width);
  }

  /**
   * @brief  Check if event is core module event
   * @param  event Event ID to check
   * @return True if given event is from a core module
   */
  inline bool isCoreModuleEvent(const XAie_Events event)
  {
    return ((event >= XAIE_EVENT_NONE_CORE) 
            && (event <= XAIE_EVENT_INSTR_ERROR_CORE));
  }
 
  /**
   * @brief  Check if event is a port running event
   * @param  event Event ID to check
   * @return True if given event is a port running event
   */
  inline bool isPortRunningEvent(const XAie_Events event)
  {
    static std::set<XAie_Events> runningEvents = {
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

  /**
   * @brief  Check if event is a port stalled event
   * @param  event Event ID to check
   * @return True if given event is a port stalled event
   */
  inline bool isPortStalledEvent(const XAie_Events event)
  {
    static std::set<XAie_Events> stalledEvents = {
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
 
  /**
   * @brief  Check if event is a port idle event
   * @param  event Event ID to check
   * @return True if given event is a port idle event
   */
  inline bool isPortIdleEvent(const XAie_Events event)
  {
    static std::set<XAie_Events> idleEvents = {
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
 
  /**
   * @brief  Check if event is a port tlast event
   * @param  event Event ID to check
   * @return True if given event is a port tlast event
   */
  inline bool isPortTlastEvent(const XAie_Events event)
  {
    static std::set<XAie_Events> tlastEvents = {
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
 
  /**
   * @brief  Check if event is generated by a stream switch monitor port
   * @param  event Event ID to check
   * @return True if given event is from a stream switch port
   */
  inline bool isStreamSwitchPortEvent(const XAie_Events event)
  {
    if (isPortRunningEvent(event) || isPortStalledEvent(event) ||
        isPortIdleEvent(event) || isPortTlastEvent(event))
      return true;
 
    return false;
  }

  /**
   * @brief  Get port number from event
   * @note   This function covers AIE Tiles and Interface Tiles
   * @param  event Event ID to check
   * @return Port number associated with given event (default: 0)
   */
  inline uint8_t getPortNumberFromEvent(const XAie_Events event)
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
   
  /**
   * @brief  Get channel number from event
   * @note   This function covers AIE Tiles and Interface Tiles
   * @param  event Event ID to check
   * @return Channel number associated with given event (default: -1)
   */
  inline int8_t getChannelNumberFromEvent(XAie_Events event)
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
 #ifndef XDP_CLIENT_BUILD
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
#ifndef XDP_CLIENT_BUILD
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

}  // namespace xdp::aie

#endif
