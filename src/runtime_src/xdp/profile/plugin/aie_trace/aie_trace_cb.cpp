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

#include "aie_trace_cb.h"
#include "aie_trace_plugin.h"


namespace xdp {

  // AIE Trace Plugin has only a static instance of the plugin object and a callback

  static AieTracePluginUnified aieTracePluginInstance;

  static void updateAIEDevice(void* handle, bool hw_context_flow)
  {
    if (AieTracePluginUnified::alive())
      aieTracePluginInstance.updateAIEDevice(handle, hw_context_flow);
  }

  static void flushAIEDevice(void* handle)
  {
    if (AieTracePluginUnified::alive())
      aieTracePluginInstance.flushAIEDevice(handle);
  }

  static void finishFlushAIEDevice(void* handle)
  {
    if (AieTracePluginUnified::alive())
      aieTracePluginInstance.finishFlushAIEDevice(handle);
  }
  
} // end namespace xdp

extern "C" 
void updateAIEDevice(void* handle, bool hw_context_flow)
{
  xdp::updateAIEDevice(handle, hw_context_flow);
}

extern "C" 
void flushAIEDevice(void* handle)
{
  xdp::flushAIEDevice(handle);
}

extern "C"
void finishFlushAIEDevice(void* handle)
{
  xdp::finishFlushAIEDevice(handle);
}
