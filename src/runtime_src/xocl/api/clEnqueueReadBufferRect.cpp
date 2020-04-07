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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/memory.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/event.h"
#include "detail/command_queue.h"
#include "detail/memory.h"
#include "detail/event.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

namespace xocl {

inline size_t
origin_in_bytes(const size_t* origin,
                size_t        row_pitch,
                size_t        slice_pitch)
{
  return origin[2] * slice_pitch
       + origin[1] * row_pitch
       + origin[0];
}

static void
setIfZero(size_t& src_row_pitch,
          size_t& src_slice_pitch,
          size_t& dst_row_pitch,
          size_t& dst_slice_pitch,
          const size_t* region)
{
  // If src_row_pitch is 0, src_row_pitch is computed as region[0].
  if (!src_row_pitch)
    src_row_pitch = region[0];

  // If src_slice_pitch is 0, src_slice_pitch is computed as region[1]
  // * src_row_pitch.
  if (!src_slice_pitch)
    src_slice_pitch = region[1]*src_row_pitch;

  // If dst_row_pitch is 0, dst_row_pitch is computed as region[0].
  if (!dst_row_pitch)
    dst_row_pitch = region[0];

  // If dst_slice_pitch is 0, dst_slice_pitch is computed as region[1]
  // * dst_row_pitch.
  if (!dst_slice_pitch)
    dst_slice_pitch = region[1]*dst_row_pitch;
}

static void
validOrError(cl_command_queue     command_queue ,
             cl_mem               buffer ,
             cl_bool              blocking ,
             const size_t *       buffer_origin ,
             const size_t *       host_origin , 
             const size_t *       region ,
             size_t               buffer_row_pitch ,
             size_t               buffer_slice_pitch ,
             size_t               host_row_pitch ,
             size_t               host_slice_pitch ,                        
             void *               ptr ,
             cl_uint              num_events_in_wait_list ,
             const cl_event *     event_wait_list ,
             cl_event *           event )
{
  if (!config::api_checks())
    return;

  detail::command_queue::validOrError(command_queue);
  detail::memory::validOrError(buffer);
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list,blocking/*check status*/);
  if (!ptr)
    throw error(CL_INVALID_VALUE,"ptr argument is nullptr");

  detail::memory::validSubBufferOffsetAlignmentOrError(buffer,xocl(command_queue)->get_device());

  // CL_INVALID_OPERATION if clEnqueueReadBufferRect is called on
  // buffer which has been created with CL_MEM_HOST_WRITE_ONLY or
  // CL_MEM_HOST_NO_ACCESS.
  if((xocl::xocl(buffer)->get_flags() & CL_MEM_HOST_WRITE_ONLY) || 
     (xocl::xocl(buffer)->get_flags() & CL_MEM_HOST_NO_ACCESS))
    throw error(CL_INVALID_OPERATION,"Buffer created with wrong flags");
}

static cl_int
clEnqueueReadBufferRect(cl_command_queue     command_queue ,
                         cl_mem               buffer ,
                         cl_bool              blocking ,
                         const size_t *       buffer_origin ,
                         const size_t *       host_origin , 
                         const size_t *       region ,
                         size_t               buffer_row_pitch ,
                         size_t               buffer_slice_pitch ,
                         size_t               host_row_pitch ,
                         size_t               host_slice_pitch ,                        
                         void *               ptr ,
                         cl_uint              num_events_in_wait_list ,
                         const cl_event *     event_wait_list ,
                         cl_event *           event )
{
  setIfZero(buffer_row_pitch,buffer_slice_pitch,host_row_pitch,host_slice_pitch,region);

  validOrError(command_queue,buffer,blocking
               ,buffer_origin,host_origin,region
               ,buffer_row_pitch,buffer_slice_pitch,host_row_pitch,host_slice_pitch
               ,ptr,num_events_in_wait_list ,event_wait_list,event);

  size_t buffer_origin_in_bytes = origin_in_bytes(buffer_origin,
                                                  buffer_row_pitch,
                                                  buffer_slice_pitch);

  size_t host_origin_in_bytes = origin_in_bytes(host_origin,host_row_pitch,
                                                host_slice_pitch);

  //allocate and aggregate event
  if(event) {

    // Soft event
    auto cmd = CL_COMMAND_WRITE_BUFFER_RECT;
    auto context = xocl(command_queue)->get_context();
    auto uevent = xocl::create_soft_event(context,cmd,num_events_in_wait_list,event_wait_list);

    // this is a user visible event, user retains a reference
    xocl::assign(event,uevent.get());

    // queue the event, block until successfully submitted
    xocl::xocl(*event)->queue(true /*wait*/);
  }

  // Now the event is running, this should be hard_event and handle asynchronously
  auto device = xocl::xocl(command_queue)->get_device();
  auto xdevice = device->get_xrt_device();
  auto boh = xocl::xocl(buffer)->get_buffer_object_or_error(device);
  void* host_ptr = xdevice->map(boh);
  
  size_t yit,zit;
  for(zit=0;zit<region[2];zit++){
    for(yit=0;yit<region[1];yit++){
      size_t buffer_row_origin_in_bytes =
        buffer_origin_in_bytes+
        zit*buffer_slice_pitch+
        yit*buffer_row_pitch;
      size_t host_row_origin_in_bytes =
        host_origin_in_bytes+
        zit*host_slice_pitch+
        yit*host_row_pitch;
      memcpy( &((uint8_t *)(ptr))[host_row_origin_in_bytes],
              &((uint8_t *)(host_ptr))[buffer_row_origin_in_bytes],
              region[0]);
    }
  }
  xdevice->unmap(boh);

  if (event)
    xocl::xocl(*event)->set_status(CL_COMPLETE);

  return CL_SUCCESS;
}

} // xocl
 
cl_int
clEnqueueReadBufferRect(cl_command_queue     command_queue ,
                         cl_mem               buffer ,
                         cl_bool              blocking ,
                         const size_t *       buffer_origin ,
                         const size_t *       host_origin , 
                         const size_t *       region ,
                         size_t               buffer_row_pitch ,
                         size_t               buffer_slice_pitch ,
                         size_t               host_row_pitch ,
                         size_t               host_slice_pitch ,                        
                         void *               ptr ,
                         cl_uint              num_events_in_wait_list ,
                         const cl_event *     event_wait_list ,
                         cl_event *           event )
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    LOP_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueReadBufferRect
      (command_queue,buffer,blocking,buffer_origin,host_origin,region
       ,buffer_row_pitch,buffer_slice_pitch
       ,host_row_pitch,host_slice_pitch
       ,ptr,num_events_in_wait_list,event_wait_list,event);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}


