/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#ifndef xrt_core_edge_user_aie_event_h
#define xrt_core_edge_user_aie_event_h

#include "AIEResources.h"
#include <map>

XAie_Events XAIETILE_EVENT_SHIM_PORT_RUNNING[] = {
  XAIE_EVENT_PORT_RUNNING_0_PL,
  XAIE_EVENT_PORT_RUNNING_1_PL,
  XAIE_EVENT_PORT_RUNNING_2_PL,
  XAIE_EVENT_PORT_RUNNING_3_PL,
  XAIE_EVENT_PORT_RUNNING_4_PL,
  XAIE_EVENT_PORT_RUNNING_5_PL,
  XAIE_EVENT_PORT_RUNNING_6_PL,
  XAIE_EVENT_PORT_RUNNING_7_PL
};

XAie_Events XAIETILE_EVENT_SHIM_PORT_IDLE[] = {
  XAIE_EVENT_PORT_IDLE_0_PL,
  XAIE_EVENT_PORT_IDLE_1_PL,
  XAIE_EVENT_PORT_IDLE_2_PL,
  XAIE_EVENT_PORT_IDLE_3_PL,
  XAIE_EVENT_PORT_IDLE_4_PL,
  XAIE_EVENT_PORT_IDLE_5_PL,
  XAIE_EVENT_PORT_IDLE_6_PL,
  XAIE_EVENT_PORT_IDLE_7_PL
};

XAie_Events XAIETILE_EVENT_SHIM_BROADCAST_A[] = {
  XAIE_EVENT_BROADCAST_A_0_PL,
  XAIE_EVENT_BROADCAST_A_1_PL,
  XAIE_EVENT_BROADCAST_A_2_PL,
  XAIE_EVENT_BROADCAST_A_3_PL,
  XAIE_EVENT_BROADCAST_A_4_PL,
  XAIE_EVENT_BROADCAST_A_5_PL,
  XAIE_EVENT_BROADCAST_A_6_PL,
  XAIE_EVENT_BROADCAST_A_7_PL,
  XAIE_EVENT_BROADCAST_A_8_PL,
  XAIE_EVENT_BROADCAST_A_9_PL,
  XAIE_EVENT_BROADCAST_A_10_PL,
  XAIE_EVENT_BROADCAST_A_11_PL,
  XAIE_EVENT_BROADCAST_A_12_PL,
  XAIE_EVENT_BROADCAST_A_13_PL,
  XAIE_EVENT_BROADCAST_A_14_PL,
  XAIE_EVENT_BROADCAST_A_15_PL
};

namespace zynqaie {

static std::map<Resources::module_type, XAie_ModuleType> AIEResourceModuletoXAieModuleTypeMap = {{Resources::pl_module, XAIE_PL_MOD}, {Resources::core_module, XAIE_CORE_MOD}, {Resources::memory_module, XAIE_MEM_MOD}};

}
#endif
