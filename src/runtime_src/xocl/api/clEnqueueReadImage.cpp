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
#include "xocl/core/memory.h"
#include "xocl/core/event.h"

#include "detail/memory.h"
#include "detail/event.h"

#include "enqueue.h"
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_command_queue      command_queue ,
             cl_mem                image ,
             cl_bool               blocking_read ,
             const size_t *        origin,
             const size_t *        region,
             size_t                row_pitch ,
             size_t                slice_pitch ,
             void *                ptr ,
             cl_uint               num_events_in_wait_list ,
             const cl_event *      event_wait_list ,
             cl_event *            event)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.
  //
  // CL_INVALID_CONTEXT if the context associated with command_queue
  // and image are not the same or if the context associated with
  // command_queue and events in event_wait_list are not the same.
  //
  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list > 0, or event_wait_list is not NULL and
  // num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.
  //
  // CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the read and
  // write operations are blocking and the execution status of any of
  // the events in event_wait_list is a negative integer value.
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list,true /* check status*/);

  // CL_INVALID_MEM_OBJECT if image is not a valid image object.
  detail::memory::validOrError(image);

  // CL_INVALID_VALUE if the region being read specified by origin and
  // region is out of bounds or if ptr is a NULL value.
  if (!ptr)
    throw error(CL_INVALID_VALUE,"ptr is nullptr");
  if (!region || !origin)
    throw error(CL_INVALID_VALUE,"region or originis nullptr");
  if (std::any_of(region,region+3,[](size_t sz){return sz==0;}))
    throw error(CL_INVALID_VALUE,"one ore more region elements are zero");
  if (   origin[0] + region[0] > xocl::xocl(image)->get_image_width()
      || origin[1] + region[1] > xocl::xocl(image)->get_image_height()
      || origin[2] + region[2] > xocl::xocl(image)->get_image_depth())
    throw xocl::error(CL_INVALID_VALUE,"origin / region out of bounds");

  // CL_INVALID_VALUE if values in origin and region do not follow
  // rules described in the argument description for origin and
  // region.

  // CL_INVALID_IMAGE_SIZE if image dimensions (image width, height,
  // specified or compute row and/or slice pitch) for image are not
  // supported by device associated with queue.

  // CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel
  // order and data type) for image are not supported by device
  // associated with queue.

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for data store associated with image.

  // CL_INVALID_OPERATION if the device associated with command_queue
  // does not support images (i.e. CL_DEVICE_IMAGE_SUPPORT specified
  // in the table of allowed values for param_name for clGetDeviceInfo
  // is CL_FALSE).

  // CL_INVALID_OPERATION if clEnqueueReadImage is called on image
  // which has been created with CL_MEM_HOST_WRITE_ONLY or
  // CL_MEM_HOST_NO_ACCESS
  if (xocl(image)->get_flags() & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS))
    throw xocl::error(CL_INVALID_OPERATION,"image buffer flags do not allow reading");

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clEnqueueReadImage(cl_command_queue      command_queue ,
                   cl_mem                image ,
                   cl_bool               blocking_read ,
                   const size_t *        origin,
                   const size_t *        region,
                   size_t                row_pitch ,
                   size_t                slice_pitch ,
                   void *                ptr ,
                   cl_uint               num_events_in_wait_list ,
                   const cl_event *      event_wait_list ,
                   cl_event *            event)
{
  validOrError(command_queue,image,blocking_read,origin,region,row_pitch,slice_pitch,ptr,
               num_events_in_wait_list,event_wait_list,event);

  if (!row_pitch)
    row_pitch = xocl(image)->get_image_bytes_per_pixel()*region[0];
  if (!slice_pitch && xocl(image)->get_image_slice_pitch())
    slice_pitch = row_pitch*region[1];

  auto uevent = xocl::create_hard_event
    (command_queue,CL_COMMAND_READ_IMAGE,num_events_in_wait_list,event_wait_list);
  xocl::enqueue::set_event_action
    (uevent.get(),xocl::enqueue::action_read_image,image,origin,region,row_pitch,slice_pitch,ptr);
  xocl::profile::set_event_action
    (uevent.get(),xocl::profile::action_read,image,0,0,true);
#ifndef _WIN32
  xocl::lop::set_event_action(uevent.get(), xocl::lop::action_read);
#endif
  xocl::appdebug::set_event_action
    (uevent.get(),xocl::appdebug::action_readwrite_image,image,origin,region,row_pitch,slice_pitch,ptr);

  uevent->queue();
  if (blocking_read)
    uevent->wait();

  xocl::assign(event,uevent.get());
  return CL_SUCCESS;
}

} // xocl


cl_int
clEnqueueReadImage(cl_command_queue      command_queue ,
                   cl_mem                image ,
                   cl_bool               blocking_read ,
                   const size_t *        origin,
                   const size_t *        region,
                   size_t                row_pitch ,
                   size_t                slice_pitch ,
                   void *                ptr ,
                   cl_uint               num_events_in_wait_list ,
                   const cl_event *      event_wait_list ,
                   cl_event *            event)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    LOP_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueReadImage
      (command_queue,image,blocking_read,origin,region,row_pitch,slice_pitch,ptr,
       num_events_in_wait_list,event_wait_list,event);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
