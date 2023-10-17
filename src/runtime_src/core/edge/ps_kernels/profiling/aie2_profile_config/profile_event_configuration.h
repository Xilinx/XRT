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

// This file contains helper structures used by the AIE event trace config
// PS kernel.

#ifndef EVENT_CONFIGURATION_DOT_H
#define EVENT_CONFIGURATION_DOT_H

#include <memory>
#include <vector>

#include "xaiefal/xaiefal.hpp"
#include "xaiengine.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/plugin/aie_profile/x86/aie_profile_kernel_config.h"

// This struct encapsulates all of the internal configuration information
// for a single AIE tile
struct EventConfiguration {
  std::map<xdp::built_in::CoreMetrics, std::vector<XAie_Events>> mCoreStartEvents;
  std::map<xdp::built_in::CoreMetrics, std::vector<XAie_Events>> mCoreEndEvents;
  std::map<xdp::built_in::MemoryMetrics, std::vector<XAie_Events>> mMemoryStartEvents;
  std::map<xdp::built_in::MemoryMetrics, std::vector<XAie_Events>> mMemoryEndEvents;
  std::map<xdp::built_in::InterfaceMetrics, std::vector<XAie_Events>> mShimStartEvents;
  std::map<xdp::built_in::InterfaceMetrics, std::vector<XAie_Events>> mShimEndEvents;
  std::map<xdp::built_in::MemTileMetrics, std::vector<XAie_Events>> mMemTileStartEvents;
  std::map<xdp::built_in::MemTileMetrics, std::vector<XAie_Events>> mMemTileEndEvents;
  std::map<xdp::module_type, uint32_t> mCounterBases;

  void initialize()
  {
    mCounterBases = {{xdp::module_type::core, 0},
      {xdp::module_type::dma, BASE_MEMORY_COUNTER},
      {xdp::module_type::shim, BASE_SHIM_COUNTER},
      {xdp::module_type::mem_tile, BASE_MEM_TILE_COUNTER}
    };

    mCoreStartEvents = {
      {
        xdp::built_in::CoreMetrics::HEAT_MAP,
        {
          XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_GROUP_CORE_STALL_CORE, XAIE_EVENT_INSTR_VECTOR_CORE,
          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::STALLS,
        {
          XAIE_EVENT_MEMORY_STALL_CORE, XAIE_EVENT_STREAM_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE,
          XAIE_EVENT_CASCADE_STALL_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::EXECUTION,
        {
          XAIE_EVENT_INSTR_VECTOR_CORE, XAIE_EVENT_INSTR_LOAD_CORE, XAIE_EVENT_INSTR_STORE_CORE,
          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::FLOATING_POINT,
        {XAIE_EVENT_FP_HUGE_CORE, XAIE_EVENT_INT_FP_0_CORE, XAIE_EVENT_FP_INVALID_CORE, XAIE_EVENT_FP_INF_CORE}
      },
      {
        xdp::built_in::CoreMetrics::STREAM_PUT_GET,
        {
          XAIE_EVENT_INSTR_CASCADE_GET_CORE, XAIE_EVENT_INSTR_CASCADE_PUT_CORE, XAIE_EVENT_INSTR_STREAM_GET_CORE,
          XAIE_EVENT_INSTR_STREAM_PUT_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::WRITE_BANDWIDTHS,
        {
          XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_INSTR_STREAM_PUT_CORE, XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
          XAIE_EVENT_GROUP_CORE_STALL_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::READ_BANDWIDTHS,
        {
          XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_INSTR_STREAM_GET_CORE, XAIE_EVENT_INSTR_CASCADE_GET_CORE,
          XAIE_EVENT_GROUP_CORE_STALL_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::AIE_TRACE,
        {
          XAIE_EVENT_PORT_RUNNING_1_CORE, XAIE_EVENT_PORT_STALLED_1_CORE, XAIE_EVENT_PORT_RUNNING_0_CORE,
          XAIE_EVENT_PORT_STALLED_0_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::EVENTS,
        {
          XAIE_EVENT_INSTR_EVENT_0_CORE, XAIE_EVENT_INSTR_EVENT_1_CORE, XAIE_EVENT_USER_EVENT_0_CORE,
          XAIE_EVENT_USER_EVENT_1_CORE
        }
      }
    };
    mCoreEndEvents = {{
        xdp::built_in::CoreMetrics::HEAT_MAP,
        {
          XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_GROUP_CORE_STALL_CORE, XAIE_EVENT_INSTR_VECTOR_CORE,
          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::STALLS,
        {
          XAIE_EVENT_MEMORY_STALL_CORE, XAIE_EVENT_STREAM_STALL_CORE, XAIE_EVENT_LOCK_STALL_CORE,
          XAIE_EVENT_CASCADE_STALL_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::EXECUTION,
        {
          XAIE_EVENT_INSTR_VECTOR_CORE, XAIE_EVENT_INSTR_LOAD_CORE, XAIE_EVENT_INSTR_STORE_CORE,
          XAIE_EVENT_GROUP_CORE_PROGRAM_FLOW_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::FLOATING_POINT,
        {
          XAIE_EVENT_FP_OVERFLOW_CORE, XAIE_EVENT_FP_UNDERFLOW_CORE, XAIE_EVENT_FP_INVALID_CORE,
          XAIE_EVENT_FP_DIV_BY_ZERO_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::STREAM_PUT_GET,
        {
          XAIE_EVENT_INSTR_CASCADE_GET_CORE, XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
          XAIE_EVENT_INSTR_STREAM_GET_CORE, XAIE_EVENT_INSTR_STREAM_PUT_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::WRITE_BANDWIDTHS,
        {
          XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_INSTR_STREAM_PUT_CORE, XAIE_EVENT_INSTR_CASCADE_PUT_CORE,
          XAIE_EVENT_GROUP_CORE_STALL_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::READ_BANDWIDTHS,
        {
          XAIE_EVENT_ACTIVE_CORE, XAIE_EVENT_INSTR_STREAM_GET_CORE, XAIE_EVENT_INSTR_CASCADE_GET_CORE,
          XAIE_EVENT_GROUP_CORE_STALL_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::AIE_TRACE,
        {
          XAIE_EVENT_PORT_RUNNING_1_CORE, XAIE_EVENT_PORT_STALLED_1_CORE, XAIE_EVENT_PORT_RUNNING_0_CORE,
          XAIE_EVENT_PORT_STALLED_0_CORE
        }
      },
      {
        xdp::built_in::CoreMetrics::EVENTS,
        {
          XAIE_EVENT_INSTR_EVENT_0_CORE, XAIE_EVENT_INSTR_EVENT_1_CORE, XAIE_EVENT_USER_EVENT_0_CORE,
          XAIE_EVENT_USER_EVENT_1_CORE
        }
      }
    };

    // **** Memory Module Counters ****
    mMemoryStartEvents = {
      {xdp::built_in::MemoryMetrics::CONFLICTS, {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {xdp::built_in::MemoryMetrics::DMA_LOCKS, {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM, XAIE_EVENT_GROUP_LOCK_MEM}},
      {
        xdp::built_in::MemoryMetrics::DMA_STALLS_S2MM,
        {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_MEM, XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_MEM}
      },
      {
        xdp::built_in::MemoryMetrics::DMA_STALLS_MM2S,
        {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_MEM, XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_MEM}
      },
      {
        xdp::built_in::MemoryMetrics::WRITE_BANDWIDTHS,
        {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM, XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}
      },
      {
        xdp::built_in::MemoryMetrics::READ_BANDWIDTHS,
        {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM, XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}
      }
    };
    mMemoryEndEvents = {
      {xdp::built_in::MemoryMetrics::CONFLICTS, {XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM, XAIE_EVENT_GROUP_ERRORS_MEM}},
      {xdp::built_in::MemoryMetrics::DMA_LOCKS, {XAIE_EVENT_GROUP_DMA_ACTIVITY_MEM, XAIE_EVENT_GROUP_LOCK_MEM}},
      {
        xdp::built_in::MemoryMetrics::DMA_STALLS_S2MM,
        {XAIE_EVENT_DMA_S2MM_0_STALLED_LOCK_ACQUIRE_MEM, XAIE_EVENT_DMA_S2MM_1_STALLED_LOCK_ACQUIRE_MEM}
      },
      {
        xdp::built_in::MemoryMetrics::DMA_STALLS_MM2S,
        {XAIE_EVENT_DMA_MM2S_0_STALLED_LOCK_ACQUIRE_MEM, XAIE_EVENT_DMA_MM2S_1_STALLED_LOCK_ACQUIRE_MEM}
      },
      {
        xdp::built_in::MemoryMetrics::WRITE_BANDWIDTHS,
        {XAIE_EVENT_DMA_S2MM_0_FINISHED_BD_MEM, XAIE_EVENT_DMA_S2MM_1_FINISHED_BD_MEM}
      },
      {
        xdp::built_in::MemoryMetrics::READ_BANDWIDTHS,
        {XAIE_EVENT_DMA_MM2S_0_FINISHED_BD_MEM, XAIE_EVENT_DMA_MM2S_1_FINISHED_BD_MEM}
      }
    };

    // **** PL/Shim Counters ****
    mShimStartEvents = {
      {
        xdp::built_in::InterfaceMetrics::INPUT_BANDWIDTHS,
        {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}
      },
      {
        xdp::built_in::InterfaceMetrics::OUTPUT_BANDWIDTHS,
        {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}
      },
      {xdp::built_in::InterfaceMetrics::PACKETS, {XAIE_EVENT_PORT_TLAST_0_PL, XAIE_EVENT_PORT_TLAST_1_PL}}
    };
    mShimEndEvents = {
      {
        xdp::built_in::InterfaceMetrics::INPUT_BANDWIDTHS,
        {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}
      },
      {
        xdp::built_in::InterfaceMetrics::OUTPUT_BANDWIDTHS,
        {XAIE_EVENT_PORT_RUNNING_0_PL, XAIE_EVENT_PORT_STALLED_0_PL}
      },
      {xdp::built_in::InterfaceMetrics::PACKETS, {XAIE_EVENT_PORT_TLAST_0_PL, XAIE_EVENT_PORT_TLAST_1_PL}}
    };

    // **** MEM Tile Counters ****
    mMemTileStartEvents = {
      {
        xdp::built_in::MemTileMetrics::INPUT_CHANNELS,
        {
          XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, XAIE_EVENT_PORT_STALLED_0_MEM_TILE, XAIE_EVENT_PORT_TLAST_0_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE
        }
      },
      {
        xdp::built_in::MemTileMetrics::INPUT_CHANNELS_DETAILS,
        {
          XAIE_EVENT_DMA_S2MM_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL0_STREAM_STARVATION_MEM_TILE,
          XAIE_EVENT_DMA_S2MM_SEL0_MEMORY_BACKPRESSURE_MEM_TILE, XAIE_EVENT_DMA_S2MM_SEL0_FINISHED_BD_MEM_TILE
        }
      },
      {
        xdp::built_in::MemTileMetrics::OUTPUT_CHANNELS,
        {
          XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, XAIE_EVENT_PORT_STALLED_0_MEM_TILE, XAIE_EVENT_PORT_TLAST_0_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE
        }
      },
      {
        xdp::built_in::MemTileMetrics::OUTPUT_CHANNELS_DETAILS,
        {
          XAIE_EVENT_DMA_MM2S_SEL0_STALLED_LOCK_ACQUIRE_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL0_STREAM_BACKPRESSURE_MEM_TILE,
          XAIE_EVENT_DMA_MM2S_SEL0_MEMORY_STARVATION_MEM_TILE, XAIE_EVENT_DMA_MM2S_SEL0_FINISHED_BD_MEM_TILE
        }
      },
      {
        xdp::built_in::MemTileMetrics::MEMORY_STATS,
        {
          XAIE_EVENT_GROUP_MEMORY_CONFLICT_MEM_TILE, XAIE_EVENT_GROUP_ERRORS_MEM_TILE, XAIE_EVENT_GROUP_LOCK_MEM_TILE,
          XAIE_EVENT_GROUP_WATCHPOINT_MEM_TILE
        }
      },
      {
        xdp::built_in::MemTileMetrics::MEM_TRACE,
        {
          XAIE_EVENT_PORT_RUNNING_0_MEM_TILE, XAIE_EVENT_PORT_STALLED_0_MEM_TILE, XAIE_EVENT_PORT_IDLE_0_MEM_TILE,
          XAIE_EVENT_PORT_TLAST_0_MEM_TILE
        }
      }
    };
    mMemTileEndEvents = mMemTileStartEvents;
  }
};

#endif
