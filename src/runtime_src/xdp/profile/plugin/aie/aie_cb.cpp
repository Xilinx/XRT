/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include "xdp/profile/plugin/aie/aie_plugin.h"

namespace xdp {

  // The AIE profiling plugin doesn't have any callbacks. Instead, it
  //  only has a single static instance of the plugin object

  static AIEProfilingPlugin aiePluginInstance;

  static void updateAIECtrDevice(void* handle)
  {
    aiePluginInstance.updateAIEDevice(handle);
  }

  static void endAIECtrPoll(void* handle)
  {
    aiePluginInstance.endPollforDevice(handle);
  }

} // end namespace xdp

extern "C"
void updateAIECtrDevice(void* handle)
{
  xdp::updateAIECtrDevice(handle);
}

extern "C"
void endAIECtrPoll(void* handle)
{
  xdp::endAIECtrPoll(handle);
}
