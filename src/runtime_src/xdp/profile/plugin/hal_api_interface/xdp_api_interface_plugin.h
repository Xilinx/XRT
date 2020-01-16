/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef XDP_API_INTERFACE_PLUGIN_DOT_H
#define XDP_API_INTERFACE_PLUGIN_DOT_H

#include "xclperf.h"

#include "xdp/config.h"

// Currently, the HAL API Interface does not require a proper
//  plugin object, as it does not interface with the event database.
//  Instead, it just directly communicates with the counters in hardware.

extern "C" {
  XDP_EXPORT void hal_api_interface_cb_func(HalInterfaceCallbackType cb_type, 
					    void* payload) ;
}

#endif
