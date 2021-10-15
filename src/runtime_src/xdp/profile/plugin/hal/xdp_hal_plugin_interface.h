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

#ifndef XDP_HAL_PLUGIN_INTERFACE_H_
#define XDP_HAL_PLUGIN_INTERFACE_H_

#include "xdp/config.h"

extern "C" {

// Generic start/stop callbacks
XDP_EXPORT
void hal_generic_cb(bool isStart, const char* name, unsigned long long int id) ;

// Specialization start/stop callbacks
XDP_EXPORT
void buffer_transfer_cb(bool isWrite, 
			bool isStart,
			const char* name,
			unsigned long long int id,
			unsigned long long int bufferId,
			unsigned long long int size) ;
}

#endif
