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

#ifndef OPENCL_COUNTERS_CALLBACKS_DOT_H
#define OPENCL_COUNTERS_CALLBACKS_DOT_H

// These are the functions that are visible when the plugin is dynamically
//  linked in.  XRT should call them directly.  We use unsigned long long int
//  so that both Windows and Linux can pass 64-bit values natively via a
//  C interface.

extern "C"
void log_function_call_start(const char* functionName, unsigned long long int queueAddress, bool isOOO) ;

extern "C"
void log_function_call_end(const char* functionName) ;

extern "C"
void log_kernel_execution(const char* kernelName,
                          bool isStart,
                          unsigned long long int kernelInstanceAddress,
                          unsigned long long int contextId,
                          unsigned long long int commandQueueId,
                          const char* deviceName,
                          const char* globalWorkSize,
                          const char* localWorkSize,
                          const char** buffers,
                          unsigned long long int numBuffers) ;

extern "C"
void log_compute_unit_execution(const char* cuName,
                                const char* kernelName,
                                const char* localWorkGroup,
                                const char* globalWorkGroup,
                                bool isStart) ;

extern "C"
void counter_action_read(unsigned long long int contextId,
                         unsigned long long int numDevices,
                         const char* deviceName,
                         unsigned long long int size,
                         bool isStart,
                         bool isP2P,
                         unsigned long long int address,
                         unsigned long long int commandQueueId) ;

extern "C"
void counter_action_write(unsigned long long int contextId,
                          const char* deviceName,
                          unsigned long long int size,
                          bool isStart,
                          bool isP2P,
                          unsigned long long int address,
                          unsigned long long int commandQueueId) ;

extern "C"
void counter_mark_objects_released() ;

#endif
