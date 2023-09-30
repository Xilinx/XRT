/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_SOURCE

#include "xdp/profile/plugin/flexml_timeline/flexml_timeline_cb.h"
#include "xdp/profile/plugin/flexml_timeline/flexml_timeline_plugin.h"

namespace xdp {

  static FlexMLTimelinePlugin flexmlTimelinePluginInstance;

  static void updateDeviceFlexmlTmln(void* handle)
  {
    if (FlexMLTimelinePlugin::alive()) {
      flexmlTimelinePluginInstance.updateAIEDevice(handle);
    } 
  } 

  static void flushDeviceFlexmlTmln(void* handle)
  {
    if (FlexMLTimelinePlugin::alive()) {
      flexmlTimelinePluginInstance.flushAIEDevice(handle);
    } 
  } 

  static void finishflushDeviceFlexmlTmln(void* handle)
  {
    if (FlexMLTimelinePlugin::alive()) {
      flexmlTimelinePluginInstance.finishflushAIEDevice(handle);
    } 
  } 

} // end namespace xdp

extern "C"
void updateDeviceFlexmlTmln(void* handle)
{
  xdp::updateDeviceFlexmlTmln(handle);
}

extern "C"
void flushDeviceFlexmlTmln(void* handle)
{
  xdp::flushDeviceFlexmlTmln(handle);
}

extern "C"
void finishflushDeviceFlexmlTmln(void* handle)
{
  xdp::finishflushDeviceFlexmlTmln(handle);
}

