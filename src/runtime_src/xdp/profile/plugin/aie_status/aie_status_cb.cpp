/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "xdp/profile/plugin/aie_status/aie_status_plugin.h"

namespace xdp {

  // The AIE status plugin doesn't have any callbacks. Instead, it
  //  only has a single static instance of the plugin object

  static AIEStatusPlugin aiePluginInstance;

  static void updateAIEStatusDevice(void* handle, bool hw_context_flow)
  {
    if (AIEStatusPlugin::alive()) {
      aiePluginInstance.updateAIEDevice(handle, hw_context_flow);
    }
  }

  static void endAIEStatusPoll(void* handle)
  {
    if (AIEStatusPlugin::alive()) {
      aiePluginInstance.endPollforDevice(handle);
    }
  }

} // end namespace xdp

extern "C"
void updateAIEStatusDevice(void* handle, bool hw_context_flow)
{
  xdp::updateAIEStatusDevice(handle, hw_context_flow);
}

extern "C"
void endAIEStatusPoll(void* handle)
{
  xdp::endAIEStatusPoll(handle);
}
