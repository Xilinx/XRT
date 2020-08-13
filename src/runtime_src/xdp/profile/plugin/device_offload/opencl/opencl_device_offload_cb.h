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

#ifndef DEVICE_OFFLOAD_CB_DOT_H
#define DEVICE_OFFLOAD_CB_DOT_H

// These are the functions that are visible when the plugin is dynamically
//  loaded.  They should be linked to callbacks in XRT via dlsym and then
//  called directly.

extern "C"
void updateDeviceOpenCL(void* handle) ;

extern "C"
void flushDeviceOpenCL(void* handle) ;

#endif
