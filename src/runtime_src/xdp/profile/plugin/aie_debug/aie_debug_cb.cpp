// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

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
