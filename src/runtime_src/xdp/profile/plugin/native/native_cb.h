/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef NATIVE_CB_DOT_H
#define NATIVE_CB_DOT_H

#include "xdp/config.h"

// These are the functions that are visible when the plugin is dynamically
//  linked in.  XRT should call them directly
extern "C"
XDP_EXPORT
void native_function_start(const char* functionName, unsigned long long int functionID) ;

extern "C"
XDP_EXPORT
void native_function_end(const char* functionName, unsigned long long int functionID, unsigned long long int timestamp) ;

extern "C"
XDP_EXPORT
void native_sync_start(const char* functionName, unsigned long long int functionID, bool isWrite) ;

extern "C"
XDP_EXPORT
void native_sync_end(const char* functionName, unsigned long long int functionID, unsigned long long int timestamp, bool isWrite, unsigned long long int size);

#endif
