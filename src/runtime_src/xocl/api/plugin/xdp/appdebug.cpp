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

/**
 * This file contains the implementation of the application debug
 * It defines lambda generator functions that are attached as
 * debug action with the cl_event
 */
#include "xocl/core/event.h"
#include "plugin/xdp/appdebug.h"

namespace xocl {
namespace appdebug {

/*
 * callback functions called from within action_ lambdas
*/

cb_action_readwrite_type cb_action_readwrite;
cb_action_copybuf_type cb_action_copybuf;
cb_action_fill_buffer_type cb_action_fill_buffer;
cb_action_map_type cb_action_map ;
cb_action_migrate_type cb_action_migrate ;
cb_action_ndrange_migrate_type cb_action_ndrange_migrate;
cb_action_ndrange_type cb_action_ndrange;
cb_action_unmap_type cb_action_unmap;
cb_action_barrier_marker_type cb_action_barrier_marker;
cb_action_readwrite_image_type cb_action_readwrite_image;

/*
 * callback registration functions called from appdebug
*/
void
register_cb_action_readwrite (cb_action_readwrite_type&& cb) {
  cb_action_readwrite = std::move(cb);
}

void
register_cb_action_copybuf (cb_action_copybuf_type&& cb) {
  cb_action_copybuf = std::move(cb);
}

void
register_cb_action_fill_buffer (cb_action_fill_buffer_type&& cb) {
  cb_action_fill_buffer = std::move(cb);
}

void
register_cb_action_map (cb_action_map_type&& cb) {
  cb_action_map = std::move(cb);
}

void
register_cb_action_migrate (cb_action_migrate_type&& cb) {
  cb_action_migrate = std::move(cb);
}

void
register_cb_action_ndrange_migrate (cb_action_ndrange_migrate_type&& cb) {
  cb_action_ndrange_migrate = std::move(cb);
}

void
register_cb_action_ndrange (cb_action_ndrange_type&& cb) {
  cb_action_ndrange = std::move(cb);
}

void
register_cb_action_unmap (cb_action_unmap_type&& cb) {
  cb_action_unmap = std::move(cb);
}

void
register_cb_action_barrier_marker (cb_action_barrier_marker_type&& cb) {
  cb_action_barrier_marker = std::move(cb);
}

void
register_cb_action_readwrite_image (cb_action_readwrite_image_type && cb) {
  cb_action_readwrite_image = std::move(cb);
}

/*
 * Lambda generator called by open CL API.
 * No references to appdebug from these
*/
action_debug_type
action_readwrite(cl_mem buffer,size_t offset, size_t size, const void* ptr)
{
  return [=](xocl::event* event) {
    if (cb_action_readwrite)
      cb_action_readwrite(event, buffer, offset, size, ptr);
  };
}

action_debug_type
action_copybuf(cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size)
{
  return [=](xocl::event* event) {
    if (cb_action_copybuf)
      cb_action_copybuf(event, src_buffer, dst_buffer, src_offset, dst_offset, size);
  };
}

action_debug_type
action_fill_buffer(cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size)
{
  return [=](xocl::event* event) {
    if (cb_action_fill_buffer)
      cb_action_fill_buffer(event, buffer, pattern, pattern_size, offset, size);
  };
}

action_debug_type
action_map(cl_mem buffer,cl_map_flags map_flags)
{
  return [=](xocl::event* event) {
    if (cb_action_map)
      cb_action_map(event, buffer, map_flags);
  };
}

action_debug_type
action_migrate(cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags)
{
  return [=](xocl::event* event) {
    if (cb_action_migrate)
      cb_action_migrate(event, num_mem_objects, mem_objects, flags);
  };
}

action_debug_type
action_ndrange_migrate(cl_event, cl_kernel kernel)
{
  return [kernel](xocl::event* event) {
    if (cb_action_ndrange_migrate)
      cb_action_ndrange_migrate(event, kernel);
  };
}

action_debug_type
action_ndrange(cl_event, cl_kernel kernel)
{
  return [kernel](xocl::event* event) {
    if (cb_action_ndrange_migrate)
      cb_action_ndrange(event, kernel);
  };
}

action_debug_type
action_unmap(cl_mem buffer)
{
  return [=](xocl::event* event) {
    if (cb_action_unmap)
      cb_action_unmap(event, buffer);
  };
}

action_debug_type
action_barrier_marker(int num_events_in_wait_list, const cl_event* event_wait_list)
{
  return [](xocl::event* event) {
    if (cb_action_barrier_marker)
      cb_action_barrier_marker(event);
  };
}

action_debug_type
action_readwrite_image(cl_mem image,const size_t* origin,const size_t* region, size_t row_pitch,size_t slice_pitch,const void* ptr)
{
  return [=](xocl::event* event) {
    if (cb_action_readwrite_image)
      cb_action_readwrite_image(event, image, origin, region, row_pitch, slice_pitch, ptr);
  };
}

}//appdebug
}//xocl
