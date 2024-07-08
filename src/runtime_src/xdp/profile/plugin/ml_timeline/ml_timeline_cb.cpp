/**
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/plugin/ml_timeline/ml_timeline_cb.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_plugin.h"

namespace xdp {

  static MLTimelinePlugin mlTimelinePluginInstance;

  static void updateDeviceMLTmln(void* hwCtxImpl)
  {
    if (MLTimelinePlugin::alive()) {
      mlTimelinePluginInstance.updateDevice(hwCtxImpl);
    } 
  } 

  static void finishflushDeviceMLTmln(void* hwCtxImpl)
  {
    if (MLTimelinePlugin::alive()) {
      mlTimelinePluginInstance.finishflushDevice(hwCtxImpl);
    } 
  } 

} // end namespace xdp

extern "C"
void updateDeviceMLTmln(void* hwCtxImpl)
{
  xdp::updateDeviceMLTmln(hwCtxImpl);
}

extern "C"
void finishflushDeviceMLTmln(void* hwCtxImpl)
{
  xdp::finishflushDeviceMLTmln(hwCtxImpl);
}

