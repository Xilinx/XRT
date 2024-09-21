/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/plugin/aie_pc/aie_pc_cb.h"
#include "xdp/profile/plugin/aie_pc/aie_pc_plugin.h"

namespace xdp {

  static AIEPCPlugin aiePCPluginInstance;

  static void updateDeviceAIEPC(void* hwCtxImpl)
  {
    if (AIEPCPlugin::alive()) {
      aiePCPluginInstance.updateDevice(hwCtxImpl);
    } 
  } 

  static void finishflushDeviceAIEPC(void* hwCtxImpl)
  {
    if (AIEPCPlugin::alive()) {
      aiePCPluginInstance.finishflushDevice(hwCtxImpl);
    } 
  } 

} // end namespace xdp

extern "C"
void updateDeviceAIEPC(void* hwCtxImpl)
{
  xdp::updateDeviceAIEPC(hwCtxImpl);
}

extern "C"
void finishflushDeviceAIEPC(void* hwCtxImpl)
{
  xdp::finishflushDeviceAIEPC(hwCtxImpl);
}