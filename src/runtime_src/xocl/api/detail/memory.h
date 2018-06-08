/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef xocl_api_detail_memory_h_
#define xocl_api_detail_memory_h_

#include "CL/cl.h"
#include <vector>

namespace xocl { namespace detail {

namespace memory {

void
validOrError(const cl_mem);

void
validOrError(const std::vector<cl_mem>& mem_objects);

void
validOrError(const cl_mem, size_t offset, size_t size);

void
validOrError(const cl_mem, cl_map_flags, size_t offset, size_t size);

void
validOrError(const cl_mem mem
             ,const size_t* buffer_origin, const size_t* host_origin, const size_t* region
             ,size_t buffer_row_pitch,size_t buffer_slice_pitch
             ,size_t host_row_pitch, size_t host_slice_pitch);

void
validSubBufferOffsetAlignmentOrError(const cl_mem mem, const cl_device_id);

void
validOrError(cl_mem_flags flags);

void
validHostPtrOrError(cl_mem_flags flags, const void* hostptr);


} // memory

}} // detail,xocl

#endif


