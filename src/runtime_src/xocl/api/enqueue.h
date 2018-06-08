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

#ifndef xocl_api_enqueue_h
#define xocl_api_enqueue_h

/**
 * This file contains the API for adapting the xocl
 * data structures to the enqueing infrastructure.
 *
 * The implementation of this API still requires old "xcl" data
 * hence enqueue.cpp currently lives under runtime_src/api/enqueue.cpp
 */

#include "xocl/core/object.h"
#include "xocl/core/event.h"
#include <utility>

namespace xocl { namespace enqueue {

xocl::event::action_enqueue_type
action_fill_buffer(cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size);

xocl::event::action_enqueue_type
action_copy_buffer(cl_mem src_buffer,cl_mem dst_buffer,size_t src_offset,size_t dst_offset,size_t size);

xocl::event::action_enqueue_type
action_copy_p2p_buffer(cl_mem src_buffer,cl_mem dst_buffer,size_t src_offset,size_t dst_offset,size_t size);


xocl::event::action_enqueue_type
action_ndrange_migrate(cl_event event,cl_kernel kernel);

xocl::event::action_enqueue_type
action_read_buffer(cl_mem buffer,size_t offset, size_t size, const void* ptr);

xocl::event::action_enqueue_type
action_map_buffer(cl_event event,cl_mem buffer,cl_map_flags map_flags,size_t offset,size_t size,void** hostbase);

xocl::event::action_enqueue_type
action_map_svm_buffer(cl_event event,cl_map_flags map_flags,void* svm_ptr,size_t size);

xocl::event::action_enqueue_type
action_write_buffer(cl_mem buffer,size_t offset, size_t size, const void* ptr);

xocl::event::action_enqueue_type
action_unmap_buffer(cl_mem memobj,void* mapped_ptr);

xocl::event::action_enqueue_type
action_unmap_svm_buffer(void* svm_ptr);

xocl::event::action_enqueue_type
action_read_image(cl_mem image,const size_t* origin,const size_t* region, size_t row_pitch,size_t slice_pitch,const void* ptr);

xocl::event::action_enqueue_type
action_write_image(cl_mem image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch,const void* ptr);

xocl::event::action_enqueue_type
action_migrate_memobjects(size_t num, const cl_mem* memobjs, cl_mem_migration_flags flags);

xocl::event::action_enqueue_type
action_ndrange_execute();

template <typename F, typename ...Args>
inline void
set_event_action(xocl::event* event, F&& f, Args&&... args)
{
  event->set_enqueue_action(f(std::forward<Args>(args)...));
}

}}

#endif


