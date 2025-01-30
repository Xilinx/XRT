/**
 * Copyright (C) 2023-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "aie_debug_cb.h"
#include "aie_debug_plugin.h"

namespace xdp {

  static AieDebugPlugin aieDebugPluginInstance;

  static void updateAIEDebugDevice(void* handle)
  {
    if (AieDebugPlugin::alive())
      aieDebugPluginInstance.updateAIEDevice(handle);
  }

  static void endAIEDebugRead(void* handle)
  {
    if (AieDebugPlugin::alive())
      aieDebugPluginInstance.endAIEDebugRead(handle);
  }

} // end namespace xdp

extern "C"
void updateAIEDebugDevice(void* handle)
{
  xdp::updateAIEDebugDevice(handle);
}

extern "C"
void endAIEDebugRead(void* handle)
{
  xdp::endAIEDebugRead(handle);
}
