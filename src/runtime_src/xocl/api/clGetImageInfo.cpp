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
#include "xocl/core/error.h"
#include "xocl/core/param.h"
#include "detail/memory.h"
#include "plugin/xdp/profile.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_mem            image,
             cl_image_info     param_name,
             size_t            param_value_size,
             void *            param_value,
             size_t *          param_value_size_ret)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_VALUE if param_name is not valid, or if size in bytes
  // specified by param_value_size is < size of return type as
  // described in the table above and param_value is not NULL.

  // CL_INVALID_MEM_OBJECT if image is a not a valid image object.
  detail::memory::validOrError(image);

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.

  // CL_INVALID_DX9_MEDIA_SURFACE_KHR if param_name is
  // CL_IMAGE_DX9_MEDIA_PLANE_KHR and image was not created by the
  // function clCreateFromDX9MediaSurfaceKHR. (If the
  // cl_khr_dx9_media_sharing extension is supported)

  // CL_INVALID_D3D10_RESOURCE_KHR if param_name is
  // CL_MEM_D3D10_SUBRESOURCE_KHR and image was not created by the
  // function clCreateFromD3D10Texture2DKHR or
  // clCreateFromD3D10Texture3DKHR. (If the cl_khr_d3d10_sharing
  // extension is supported)

  // CL_INVALID_D3D11_RESOURCE_KHR if param_name is
  // CL_MEM_D3D11_SUBRESOURCE_KHR and image was not created by the
  // function clCreateFromD3D11Texture2DKHR or
  // clCreateFromD3D11Texture3DKHR. (If the cl_khr_d3d11_sharing
  // extension is supported)
}

static cl_int
clGetImageInfo(cl_mem            image,
               cl_image_info     param_name,
               size_t            param_value_size,
               void *            param_value,
               size_t *          param_value_size_ret )
{
  validOrError(image,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };
  auto buf = buffer.as_array<cl_image_format>(1);
  cl_image_format fmt;

  switch(param_name){
    case CL_IMAGE_FORMAT:
      fmt = xocl(image)->get_image_format();
      std::memcpy(buf,&fmt, sizeof(cl_image_format));
      break;
    case CL_IMAGE_ELEMENT_SIZE:
      buffer.as<size_t>() = xocl(image)->get_image_bytes_per_pixel();
      break;
    case CL_IMAGE_ROW_PITCH:
      buffer.as<size_t>() = xocl(image)->get_image_row_pitch();
      break;
    case CL_IMAGE_SLICE_PITCH:
      buffer.as<size_t>() = xocl(image)->get_image_slice_pitch();
      break;
    case CL_IMAGE_WIDTH:
      buffer.as<size_t>() = xocl(image)->get_image_width();
      break;
    case CL_IMAGE_HEIGHT:
      buffer.as<size_t>() = xocl(image)->get_image_height();
      break;
    case CL_IMAGE_DEPTH:
      buffer.as<size_t>() = xocl(image)->get_image_depth();
      break;
    case CL_IMAGE_ARRAY_SIZE:
      throw error(CL_INVALID_OPERATION,"not implemented");
      break;
    case CL_IMAGE_NUM_MIP_LEVELS:
      throw error(CL_INVALID_OPERATION,"not implemented");
      break;
    case CL_IMAGE_NUM_SAMPLES:
      throw error(CL_INVALID_OPERATION,"not implemented");
      break;
    default:
      throw error(CL_INVALID_VALUE,"Invalid param_name '" + std::to_string(param_name) + "'");
      break;
  }

  return CL_SUCCESS;
}

} // xocl

cl_int
clGetImageInfo(cl_mem            image,
               cl_image_info     param_name,
               size_t            param_value_size,
               void *            param_value,
               size_t *          param_value_size_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetImageInfo
      (image,param_name,param_value_size,param_value,param_value_size_ret);
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
