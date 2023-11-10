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

#ifndef AIE_PROFILE_DEFS_H
#define AIE_PROFILE_DEFS_H

constexpr uint16_t BASE_MEMORY_COUNTER   = 128;
constexpr uint16_t BASE_SHIM_COUNTER     = 256;
constexpr uint16_t BASE_MEM_TILE_COUNTER = 384;

constexpr uint32_t GROUP_DMA_MASK                   = 0x0000f000;
constexpr uint32_t GROUP_LOCK_MASK                  = 0x55555555;
constexpr uint32_t GROUP_CONFLICT_MASK              = 0x000000ff;
constexpr uint32_t GROUP_ERROR_MASK                 = 0x00003fff;
constexpr uint32_t GROUP_STREAM_SWITCH_IDLE_MASK    = 0x11111111;
constexpr uint32_t GROUP_STREAM_SWITCH_RUNNING_MASK = 0x22222222;
constexpr uint32_t GROUP_STREAM_SWITCH_STALLED_MASK = 0x44444444;
constexpr uint32_t GROUP_STREAM_SWITCH_TLAST_MASK   = 0x88888888;
constexpr uint32_t GROUP_CORE_PROGRAM_FLOW_MASK     = 0x00001FE0;
constexpr uint32_t GROUP_CORE_STALL_MASK            = 0x0000000F;

// Note: these masks are applicable to AIE2* devices
constexpr uint32_t GROUP_SHIM_S2MM0_STALL_MASK      = 0x00041000; 
constexpr uint32_t GROUP_SHIM_S2MM1_STALL_MASK      = 0x00082000;
constexpr uint32_t GROUP_SHIM_MM2S0_STALL_MASK      = 0x00500000;
constexpr uint32_t GROUP_SHIM_MM2S1_STALL_MASK      = 0x00A00000;

#endif