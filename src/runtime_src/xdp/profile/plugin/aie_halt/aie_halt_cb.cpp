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

#include "xdp/profile/plugin/aie_halt/aie_halt_cb.h"
#include "xdp/profile/plugin/aie_halt/aie_halt_plugin.h"

namespace xdp {

  static AIEHaltPlugin aieHaltPluginInstance;

  static void updateDeviceAIEHalt(void* hwCtxImpl)
  {
    if (AIEHaltPlugin::alive()) {
      aieHaltPluginInstance.updateDevice(hwCtxImpl);
    } 
  } 

  static void finishflushDeviceAIEHalt(void* hwCtxImpl)
  {
    if (AIEHaltPlugin::alive()) {
      aieHaltPluginInstance.finishflushDevice(hwCtxImpl);
    } 
  } 

} // end namespace xdp

extern "C"
void updateDeviceAIEHalt(void* hwCtxImpl)
{
  xdp::updateDeviceAIEHalt(hwCtxImpl);
}

extern "C"
void finishflushDeviceAIEHalt(void* hwCtxImpl)
{
  xdp::finishflushDeviceAIEHalt(hwCtxImpl);
}