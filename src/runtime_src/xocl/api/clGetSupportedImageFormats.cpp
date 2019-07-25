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
#include "xocl/api/image.h"
#include "detail/context.h"

#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(cl_context context,
             cl_mem_flags         flags,
             cl_mem_object_type   image_type,
             cl_uint              num_entries,
             cl_image_format *    image_formats,
             cl_uint *            num_image_formats)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context.
  detail::context::validOrError(context);

  // CL_INVALID_VALUE if flags or image_type are not valid, or if num_entries is 0 and image_formats is not NULL.
  if (!num_entries && image_formats)
    throw error(CL_INVALID_VALUE, "clGetSupportedImageFormats num_entries==0");

  if (image_type != CL_MEM_OBJECT_IMAGE1D &&
      image_type != CL_MEM_OBJECT_IMAGE1D_ARRAY &&
      image_type != CL_MEM_OBJECT_IMAGE1D_BUFFER &&
      image_type != CL_MEM_OBJECT_IMAGE2D_ARRAY &&
      image_type != CL_MEM_OBJECT_IMAGE2D &&
      image_type != CL_MEM_OBJECT_IMAGE3D)
    throw xocl::error(CL_INVALID_VALUE, "Bad image_type");

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
}

static cl_int
clGetSupportedImageFormats(cl_context context,
	cl_mem_flags         flags,
	cl_mem_object_type   image_type,
	cl_uint              num_entries,
	cl_image_format *    image_formats,
	cl_uint *            num_image_formats)
{
  validOrError(context,flags,image_type,num_entries,image_formats,num_image_formats);

  size_t n = 0;
  for (size_t i = 0; i < sizeof(xocl::images::cl_image_order)/sizeof(uint32_t); ++i)
  {
      for (size_t j = 0; j < sizeof(xocl::images::cl_image_type)/sizeof(uint32_t); ++j) {
        const cl_image_format fmt = {
          .image_channel_order = xocl::images::cl_image_order[i],
          .image_channel_data_type = xocl::images::cl_image_type[j]
        };
        xocl::images::xlnx_image_type supported_image_type = xocl::images::get_image_supported_format(&fmt, flags);
        if (supported_image_type==xocl::images::xlnx_image_type::XLNX_UNSUPPORTED_FORMAT)
          continue;
        if (n < num_entries && image_formats)
          image_formats[n] = fmt;
        ++n;
      }
  }
  if (num_image_formats)
      *num_image_formats = n;
  return CL_SUCCESS;

}

} // xocl

cl_int
clGetSupportedImageFormats(cl_context           context,
                           cl_mem_flags         flags,
                           cl_mem_object_type   image_type,
                           cl_uint              num_entries,
                           cl_image_format *    image_formats,
                           cl_uint *            num_image_formats)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetSupportedImageFormats
      (context,flags,image_type,num_entries,image_formats,num_image_formats);
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
