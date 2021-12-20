/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "xdp/profile/plugin/aie_debug/aie_debug_plugin.h"

namespace xdp {

  // The AIE debug plugin doesn't have any callbacks. Instead, it
  //  only has a single static instance of the plugin object

  static AIEDebugPlugin aiePluginInstance;

  static void updateAIEDebugDevice(void* handle)
  {
    aiePluginInstance.updateAIEDevice(handle);
  }

  static void endAIEDebugPoll(void* handle)
  {
    aiePluginInstance.endPollforDevice(handle);
  }

} // end namespace xdp

extern "C"
void updateAIEDebugDevice(void* handle)
{
  xdp::updateAIEDebugDevice(handle);
}

extern "C"
void endAIEDebugPoll(void* handle)
{
  xdp::endAIEDebugPoll(handle);
}
