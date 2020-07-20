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

#ifndef OPENCL_TRACE_CALLBACKS_DOT_H
#define OPENCL_TRACE_CALLBACKS_DOT_H

#include <cstdlib>

// These are the functions that are visible when the plugin is dynamically
//  linked in.  XRT should call them directly.
extern "C" 
void function_start(const char* functionName, 
		    unsigned long long int queueAddress, 
		    unsigned long long int functionID);

extern "C"
void function_end(const char* functionName, 
		  unsigned long long int queueAddress,
		  unsigned long long int functionID);

extern "C"
void action_read(unsigned int id,
		 bool isStart,
		 unsigned long long int deviceAddress,
		 const char* memoryResource,
		 size_t bufferSize,
		 bool isP2P,
		 unsigned long int* dependencies,
		 unsigned int numDependencies) ;

extern "C"
void action_write(unsigned int id,
		  bool isStart,
		  unsigned long long int deviceAddress,
		  const char* memoryResource,
		  size_t bufferSize,
		  bool isP2P,
		  unsigned long int* dependencies,
		  unsigned int numDependencies) ;

extern "C"
void action_copy(unsigned int id,
		 bool isStart,
		 unsigned long long int srcDeviceAddress,
		 const char* srcMemoryResource,
		 unsigned long long int dstDeviceAddress,
		 const char* dstMemoryResource,
		 size_t bufferSize,
		 bool isP2P,
		 unsigned long int* dependencies,
		 unsigned int numDependencies) ;
/*
extern "C"
void action_ndrange(unsigned int id, bool isStart) ;
*/
#endif
