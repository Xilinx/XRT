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

#include "xocl/config.h"
#include "xocl/core/event.h"
#include "xocl/core/memory.h"
#include "xocl/core/device.h"
#include "xocl/core/context.h"
#include "xocl/core/command_queue.h"
#include "detail/command_queue.h"
#include "detail/memory.h"
#include "detail/context.h"
#include "detail/event.h"

#include "api.h"
#include "enqueue.h"
#include "khronos/khronos.h"
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile.h"

#include <CL/opencl.h>

namespace xocl {

static cl_uint
getDeviceMemBaseAddrAlign(cl_device_id device)
{
  cl_uint size = 0;
  api::clGetDeviceInfo(device,CL_DEVICE_MEM_BASE_ADDR_ALIGN,sizeof(cl_uint),&size,nullptr);
  return size;
}

inline size_t
origin_in_bytes(const size_t* origin,
                size_t        row_pitch,
                size_t        slice_pitch)
{
  return origin[2] * slice_pitch
       + origin[1] * row_pitch
       + origin[0];
}

inline size_t
extent_in_bytes(const size_t* region,
                const size_t* origin,
                size_t        row_pitch,
                size_t        slice_pitch)
{
  return origin_in_bytes(origin,row_pitch,slice_pitch)
       + (region[2] - 1) * slice_pitch
       + (region[1] - 1) * row_pitch
       + region[0];
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
             cl_mem               src_buffer ,
             cl_mem               dst_buffer , 
             const size_t *       src_origin ,
             const size_t *       dst_origin ,
             const size_t *       region , 
             size_t               src_row_pitch ,
             size_t               src_slice_pitch ,
             size_t               dst_row_pitch ,
             size_t               dst_slice_pitch ,
             cl_uint              num_events_in_wait_list ,
             const cl_event *     event_wait_list ,
             cl_event *           event_parameter)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host
  // command-queue.
  detail::command_queue::validOrError(command_queue);

  // CL_INVALID_MEM_OBJECT if src_buffer and dst_buffer are not valid
  // buffer objects.
  detail::memory::validOrError({src_buffer,dst_buffer});

  // CL_INVALID_CONTEXT if the context associated with command_queue,
  // src_buffer, and dst_buffer are not the same or if the context
  // associated with command_queue and events in event_wait_list are
  // not the same.
  //
  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list is > 0, or event_wait_list is not NULL
  // and num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.
  auto context = xocl(command_queue)->get_context();
  detail::context::validOrError(context,{src_buffer,dst_buffer});
  detail::event::validOrError(context,num_events_in_wait_list,event_wait_list);

  // CL_INVALID_VALUE if any region array element is 0.
  // Further, the values in region cannot be zero
  if (!region || !src_origin || !dst_origin)
    throw error(CL_INVALID_VALUE,"region, src_origin, or dst_origin is nullptr");
  if (std::any_of(region,region+3,[](size_t sz){return sz==0;}))
    throw error(CL_INVALID_VALUE,"one or more region elements are zero");

  // CL_INVALID_VALUE if (src_origin, region, src_row_pitch,
  // src_slice_pitch) or (dst_origin, region, dst_row_pitch,
  // dst_slice_pitch) require accessing elements outside the
  // src_buffer and dst_buffer objects respectively.
  if (extent_in_bytes(region,src_origin,src_row_pitch,src_slice_pitch) > xocl(src_buffer)->get_size())
    throw error(CL_INVALID_VALUE,"src_origin,region,src_row_pitch,src_slice_pitch out of range");
  if (extent_in_bytes(region,dst_origin,dst_row_pitch,dst_slice_pitch) > xocl(dst_buffer)->get_size())
    throw error(CL_INVALID_VALUE,"dst_origin,region,dst_row_pitch,dst_slice_pitch out of range");
    
  // CL_INVALID_VALUE if src_row_pitch is not 0 and is less than
  // region[0].
  if (src_row_pitch && src_row_pitch < region[0])
    throw error(CL_INVALID_VALUE,"invalid src_row_pitch");

  // CL_INVALID_VALUE if dst_row_pitch is not 0 and is less than
  // region[0].
  if (dst_row_pitch && dst_row_pitch < region[0])
    throw error(CL_INVALID_VALUE,"invalid dst_row_pitch");

  // CL_INVALID_VALUE if src_slice_pitch is not 0 and is less than
  // region[1] * src_row_pitch or if src_slice_pitch is not 0 and is
  // not a multiple of src_row_pitch.
  if (src_slice_pitch && (src_slice_pitch < region[1] * src_row_pitch))
    throw error(CL_INVALID_VALUE,"invalid src_slice_pitch");
  if (src_slice_pitch && (!src_row_pitch || (src_slice_pitch % src_row_pitch)))
    throw error(CL_INVALID_VALUE,"invalid src_slice_pitch");

  // CL_INVALID_VALUE if dst_slice_pitch is not 0 and is less than
  // region[1] * dst_row_pitch or if dst_slice_pitch is not 0 and is
  // not a multiple of dst_row_pitch.
  if (dst_slice_pitch && (dst_slice_pitch < region[1] * dst_row_pitch))
    throw error(CL_INVALID_VALUE,"invalid dst_slice_pitch");
  if (dst_slice_pitch && (!dst_row_pitch || (dst_slice_pitch % dst_row_pitch)))
    throw error(CL_INVALID_VALUE,"invalid dst_slice_pitch");

  // CL_INVALID_VALUE if src_buffer and dst_buffer are the same buffer
  // object and src_slice_pitch is not equal to dst_slice_pitch and
  // src_row_pitch is not equal to dst_row_pitch.
  if (src_buffer==dst_buffer && src_slice_pitch!=dst_slice_pitch && src_row_pitch!=dst_row_pitch)
    throw error(CL_INVALID_VALUE,"src_buffer==dst_buffer + pitch errors");

  // CL_MEM_COPY_OVERLAP if src_buffer and dst_buffer are the same
  // buffer or sub-buffer object and the source and destination
  // regions overlap or if src_buffer and dst_buffer are different
  // sub-buffers of the same associated buffer object and they
  // overlap. Refer to Appendix D in the OpenCL specification for
  // details on how to determine if source and destination regions
  // overlap.
  if(src_buffer==dst_buffer && 
     khronos::check_copy_overlap(src_origin,dst_origin,region,src_row_pitch,src_slice_pitch))
    throw error(CL_MEM_COPY_OVERLAP,"src_buffer==dst_buffer overlap error");

  // CL_MISALIGNED_SUB_BUFFER_OFFSET if src_buffer is a sub-buffer
  // object and offset specified when the sub-buffer object is created
  // is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
  // associated with queue.
  //
  // CL_MISALIGNED_SUB_BUFFER_OFFSET if dst_buffer is a sub-buffer
  // object and offset specified when the sub-buffer object is created
  // is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
  // associated with queue.
  auto align = getDeviceMemBaseAddrAlign(xocl(command_queue)->get_device());
  if (xocl(src_buffer)->is_sub_buffer() && (xocl(src_buffer)->get_sub_buffer_offset() % align))
    throw error(CL_MISALIGNED_SUB_BUFFER_OFFSET,"clEnqueueCopyBuffer bad src sub buffer offset");
  if (xocl(dst_buffer)->is_sub_buffer() && (xocl(dst_buffer)->get_sub_buffer_offset() % align))
    throw error(CL_MISALIGNED_SUB_BUFFER_OFFSET,"clEnqueueCopyBuffer bad dst sub buffer offset");

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for data store associated with src_buffer or
  // dst_buffer.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clEnqueueCopyBufferRect(cl_command_queue     command_queue , 
                        cl_mem               src_buffer ,
                        cl_mem               dst_buffer , 
                        const size_t *       src_origin ,
                        const size_t *       dst_origin ,
                        const size_t *       region , 
                        size_t               src_row_pitch ,
                        size_t               src_slice_pitch ,
                        size_t               dst_row_pitch ,
                        size_t               dst_slice_pitch ,
                        cl_uint              num_events_in_wait_list ,
                        const cl_event *     event_wait_list ,
                        cl_event *           event_parameter)
{
  setIfZero(src_row_pitch,src_slice_pitch,dst_row_pitch,dst_slice_pitch,region);

  validOrError
    (command_queue,src_buffer,dst_buffer,src_origin,dst_origin,region,
     src_row_pitch,src_slice_pitch,dst_row_pitch,dst_slice_pitch,
     num_events_in_wait_list,event_wait_list,event_parameter);

  // Soft event
  auto context = xocl(command_queue)->get_context();
  auto uevent = xocl::create_soft_event(context,CL_COMMAND_COPY_BUFFER_RECT,num_events_in_wait_list,event_wait_list);
  // queue the event, block until successfully submitted
  uevent->queue(true/*wait*/);
  uevent->set_status(CL_RUNNING);

  //memcpy
  {
    auto device = xocl(command_queue)->get_device();
    auto xdevice = device->get_xrt_device();
    auto src_boh = xocl(src_buffer)->get_buffer_object(device);
    auto dst_boh = xocl(dst_buffer)->get_buffer_object(device);
    void* host_ptr_src = xdevice->map(src_boh);
    void* host_ptr_dst = xdevice->map(dst_boh);

    for(size_t zit=0;zit<region[2];++zit) {
      for(size_t yit=0;yit<region[1];++yit) {
        size_t src_row_origin_in_bytes =
          origin_in_bytes(src_origin,src_row_pitch,src_slice_pitch)
          + zit*src_slice_pitch
          + yit*src_row_pitch;

        size_t dst_row_origin_in_bytes =
          origin_in_bytes(dst_origin,dst_row_pitch,dst_slice_pitch)
          + zit*dst_slice_pitch
          + yit*dst_row_pitch;

        std::memcpy( &((uint8_t *)(host_ptr_dst))[dst_row_origin_in_bytes],
                     &((uint8_t *)(host_ptr_src))[src_row_origin_in_bytes],
                     region[0]);
      }
    }
    xdevice->unmap(src_boh);
    xdevice->unmap(dst_boh);
  }

  //set event CL_COMPLETE
  uevent->set_status(CL_COMPLETE);
  xocl::assign(event_parameter,uevent.get());
  return CL_SUCCESS;
}

} // xocl

cl_int
clEnqueueCopyBufferRect(cl_command_queue     command_queue , 
                        cl_mem               src_buffer ,
                        cl_mem               dst_buffer , 
                        const size_t *       src_origin ,
                        const size_t *       dst_origin ,
                        const size_t *       region , 
                        size_t               src_row_pitch ,
                        size_t               src_slice_pitch ,
                        size_t               dst_row_pitch ,
                        size_t               dst_slice_pitch ,
                        cl_uint              num_events_in_wait_list ,
                        const cl_event *     event_wait_list ,
                        cl_event *           event_parameter)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueCopyBufferRect
      (command_queue,src_buffer,dst_buffer,src_origin,dst_origin,region,
       src_row_pitch,src_slice_pitch,dst_row_pitch,dst_slice_pitch,
       num_events_in_wait_list,event_wait_list,event_parameter);

  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
