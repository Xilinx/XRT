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

#include "xdp/profile/plugin/aie_trace/aie_trace_cb.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_plugin.h"

namespace xdp {

  // AIE Trace Plugin has only a static instance of the plugin object and a callback

  static AieTracePlugin aieTracePluginInstance;

  static void updateAIEDevice(void* handle)
  {
    aieTracePluginInstance.updateAIEDevice(handle);
  }

  static void flushAIEDevice(void* handle)
  {
    aieTracePluginInstance.flushAIEDevice(handle);
  }

} // end namespace xdp

extern "C" 
void updateAIEDevice(void* handle)
{
  xdp::updateAIEDevice(handle);
}

extern "C" 
void flushAIEDevice(void* handle)
{
  xdp::flushAIEDevice(handle);
}

