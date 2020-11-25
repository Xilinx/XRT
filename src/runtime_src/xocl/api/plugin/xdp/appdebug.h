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

#ifndef xocl_api_appdebug_h
#define xocl_api_appdebug_h

#include "xocl/core/event.h"

namespace xocl {
namespace appdebug {

  void load_xdp_app_debug() ;
  void register_appdebug_functions(void* handle) ;

using action_debug_type = xocl::event::action_debug_type;

/*
 * callback function types called from within action_ lambdas
*/
using cb_action_readwrite_type = std::function<void (xocl::event* event, cl_mem buffer,size_t offset, size_t size, const void* ptr)>;
using cb_action_copybuf_type = std::function< void (xocl::event* event, cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size)> ;
using cb_action_fill_buffer_type = std::function<void (xocl::event* event, cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size)> ;
using cb_action_map_type = std::function<void (xocl::event* event, cl_mem buffer,cl_map_flags map_flag)>  ;
using cb_action_migrate_type = std::function<void (xocl::event* event, cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags)>  ;
using cb_action_ndrange_migrate_type = std::function< void (xocl::event* event, cl_kernel kernel)> ;
using cb_action_ndrange_type = std::function< void (xocl::event* event, cl_kernel kernel)> ;
using cb_action_unmap_type = std::function<void (xocl::event* event, cl_mem buffer)> ;
using cb_action_barrier_marker_type = std::function< void (xocl::event* event) > ;
using cb_action_readwrite_image_type = std::function <void (xocl::event* event, cl_mem image,const size_t* origin,const size_t* region, size_t row_pitch,size_t slice_pitch,const void* ptr)> ;

/*
 * callback registration functions called from appdebug
*/
XRT_XOCL_EXPORT void register_cb_action_readwrite (cb_action_readwrite_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_copybuf (cb_action_copybuf_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_fill_buffer (cb_action_fill_buffer_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_map (cb_action_map_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_migrate (cb_action_migrate_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_ndrange_migrate (cb_action_ndrange_migrate_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_ndrange (cb_action_ndrange_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_unmap (cb_action_unmap_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_barrier_marker (cb_action_barrier_marker_type&& cb);
XRT_XOCL_EXPORT void register_cb_action_readwrite_image (cb_action_readwrite_image_type && cb);
/*
 * Lambda generator called by open CL API.
 * No references to appdebug from these
*/
XRT_XOCL_EXPORT
action_debug_type
action_readwrite(cl_mem buffer,size_t offset, size_t size, const void* ptr);

XRT_XOCL_EXPORT
action_debug_type
action_copybuf(cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size);

XRT_XOCL_EXPORT
action_debug_type
action_fill_buffer(cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size);

XRT_XOCL_EXPORT
action_debug_type
action_map(cl_mem buffer,cl_map_flags map_flags);

XRT_XOCL_EXPORT
action_debug_type
action_migrate(cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags);

XRT_XOCL_EXPORT
action_debug_type
action_ndrange_migrate(cl_event event, cl_kernel kernel);

XRT_XOCL_EXPORT
action_debug_type
action_ndrange(cl_event event, cl_kernel kernel);

XRT_XOCL_EXPORT
action_debug_type
action_unmap(cl_mem buffer);

XRT_XOCL_EXPORT
action_debug_type
action_barrier_marker(int num_events_in_wait_list, const cl_event* event_wait_list);

XRT_XOCL_EXPORT
action_debug_type
action_readwrite_image(cl_mem image,const size_t* origin,const size_t* region,
                                  size_t row_pitch,size_t slice_pitch,const void* ptr);


template <typename F, typename ...Args>
void
set_event_action(xocl::event* event, F&& f, Args&&... args)
{
  //Save on effort creating lambda if debug not enabled
  if (xrt_xocl::config::get_app_debug()) {
    event->set_debug_action(f(std::forward<Args>(args)...));
  }
}

}//appdebug
}//xocl

#endif


