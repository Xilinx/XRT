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
#define XDP_PLUGIN_SOURCE

#include "aie_profile_cb.h"
#include "aie_profile_plugin.h"

namespace xdp {

  static AieProfilePlugin aieProfilePluginInstance;

  static void updateAIECtrDevice(void* handle, bool hw_context_flow)
  {
    if (AieProfilePlugin::alive())
      aieProfilePluginInstance.updateAIEDevice(handle, hw_context_flow);
  }

  static void endAIECtrPoll(void* handle)
  {
    if (AieProfilePlugin::alive())
      aieProfilePluginInstance.endPollforDevice(handle);
  }

} // end namespace xdp

extern "C"
void updateAIECtrDevice(void* handle, bool hw_context_flow)
{
  xdp::updateAIECtrDevice(handle, hw_context_flow);
}

extern "C"
void endAIECtrPoll(void* handle)
{
  xdp::endAIECtrPoll(handle);
}
