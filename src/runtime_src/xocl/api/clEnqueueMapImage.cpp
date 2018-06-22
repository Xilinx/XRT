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

#include <CL/opencl.h>
#include "xocl/config.h"
#include "xocl/core/error.h"

#include "profile.h"

namespace xocl {

static void
validOrError(cl_command_queue   command_queue ,
             cl_mem             image , 
             cl_bool            blocking_map , 
             cl_map_flags       map_flags , 
             const size_t *     origin ,
             const size_t *     region ,
             size_t *           image_row_pitch ,
             size_t *           image_slice_pitch ,
             cl_uint            num_events_in_wait_list ,
             const cl_event *   event_wait_list ,
             cl_event *         event ,
             cl_int *           errcode_ret )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.

  // CL_INVALID_CONTEXT if the context associated with command_queue and image are not the same or if the context associated with command_queue and events in event_wait_list are not the same.

  // CL_INVALID_MEM_OBJECT if image is not a valid image object.

  // CL_INVALID_VALUE if region being mapped given by (origin,
  // origin+region) is out of bounds or if values specified in
  // map_flags are not valid.

  // CL_INVALID_VALUE if values in origin and region do not follow
  // rules described in the argument description for origin and
  // region.

  // CL_INVALID_VALUE if image_row_pitch is NULL.

  // CL_INVALID_VALUE if image is a 3D image, 1D or 2D image array
  // object and image_slice_pitch is NULL.

  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list > 0, or event_wait_list is not NULL and
  // num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.

  // CL_INVALID_IMAGE_SIZE if image dimensions (image width, height,
  // specified or compute row and/or slice pitch) for image are not
  // supported by device associated with queue.

  // CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel
  // order and data type) for image are not supported by device
  // associated with queue.

  // CL_MAP_FAILURE if there is a failure to map the requested region
  // into the host address space. This error cannot occur for image
  // objects created with CL_MEM_USE_HOST_PTR or
  // CL_MEM_ALLOC_HOST_PTR.

  // CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the map operation
  // is blocking and the execution status of any of the events in
  // event_wait_list is a negative integer value.

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for data store associated with image.

  // CL_INVALID_OPERATION if the device associated with command_queue
  // does not support images (i.e. CL_DEVICE_IMAGE_SUPPORT specified
  // in the table of OpenCL Device Queries for clGetDeviceInfo is
  // CL_FALSE.

  // CL_INVALID_OPERATION if image has been created with
  // CL_MEM_HOST_WRITE_ONLY or CL_MEM_HOST_NO_ACCESS and CL_MAP_READ
  // is set in map_flags or if image has been created with
  // CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS and CL_MAP_WRITE
  // or CL_MAP_WRITE_INVALIDATE_REGION is set in map_flags.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.

  // CL_INVALID_OPERATION if mapping would lead to overlapping regions
  // being mapped for writing.
}

static void*
clEnqueueMapImage(cl_command_queue   command_queue ,
                  cl_mem             image , 
                  cl_bool            blocking_map , 
                  cl_map_flags       map_flags , 
                  const size_t *     origin ,
                  const size_t *     region ,
                  size_t *           image_row_pitch ,
                  size_t *           image_slice_pitch ,
                  cl_uint            num_events_in_wait_list ,
                  const cl_event *   event_wait_list ,
                  cl_event *         event ,
                  cl_int *           errcode_ret )
{
  validOrError(command_queue,image,blocking_map,map_flags,origin,region,image_row_pitch,image_slice_pitch,
               num_events_in_wait_list,event_wait_list,event,errcode_ret);
  throw error(CL_XILINX_UNIMPLEMENTED,"Not implemented");
}

} // xocl

void*
clEnqueueMapImage(cl_command_queue   command_queue ,
                  cl_mem             image , 
                  cl_bool            blocking_map , 
                  cl_map_flags       map_flags , 
                  const size_t *     origin ,
                  const size_t *     region ,
                  size_t *           image_row_pitch ,
                  size_t *           image_slice_pitch ,
                  cl_uint            num_events_in_wait_list ,
                  const cl_event *   event_wait_list ,
                  cl_event *         event ,
                  cl_int *           errcode_ret )
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueMapImage
      (command_queue,image,blocking_map,map_flags,origin,region,image_row_pitch,image_slice_pitch
       ,num_events_in_wait_list,event_wait_list,event,errcode_ret);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}



