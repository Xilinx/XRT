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
#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/context.h"
#include "xocl/core/memory.h"
#include "detail/memory.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_mem           memobj,
             cl_mem_info      param_name,
             size_t           param_value_size,
             void *           param_value,
             size_t *         param_value_size_ret)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_VALUE if param_name is not valid, or if size in bytes
  // specified by param_value_size is < the size of return type as
  // described in the table above and param_value is not NULL.

  // CL_INVALID_MEM_OBJECT if memobj is a not a valid memory object.
  detail::memory::validOrError(memobj);

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.

  // CL_INVALID_D3D10_RESOURCE_KHR If the cl_khr_d3d10_sharing
  // extension is enabled and if param_name is
  // CL_MEM_D3D10_RESOURCE_KHR and memobj was not created by the
  // function clCreateFromD3D10BufferKHR,
  // clCreateFromD3D10Texture2DKHR, or clCreateFromD3D10Texture3DKHR.

  // CL_INVALID_DX9_MEDIA_SURFACE_KHR if param_name is
  // CL_MEM_DX9_MEDIA_SURFACE_INFO_KHR and memobj was not created by
  // the function clCreateFromDX9MediaSurfaceKHR from a Direct3D9
  // surface. (If the cl_khr_dx9_media_sharing extension is supported)

  // CL_INVALID_D3D11_RESOURCE_KHR If the cl_khr_d3d11_sharing
  // extension is supported, if param_name is
  // CL_MEM_D3D11_RESOURCE_KHR and memobj was not created by the
  // function clCreateFromD3D11BufferKHR,
  // clCreateFromD3D11Texture2DKHR, or clCreateFromD3D11Texture3DKHR."
}

static cl_int
clGetMemObjectInfo(cl_mem           memobj,
                   cl_mem_info      param_name,
                   size_t           param_value_size,
                   void *           param_value,
                   size_t *         param_value_size_ret )
{
  validOrError(memobj,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };

  switch (param_name) {
    case CL_MEM_TYPE:
      buffer.as<cl_mem_object_type>() = xocl(memobj)->get_type();
      break;
    case CL_MEM_FLAGS:
      buffer.as<cl_mem_flags>() = xocl(memobj)->get_flags();
      break;
    case CL_MEM_SIZE:
      buffer.as<size_t>() = xocl(memobj)->get_size();
      break;
    case CL_MEM_HOST_PTR:
      buffer.as<void*>() = xocl(memobj)->get_host_ptr();
      break;
    case CL_MEM_MAP_COUNT:
      buffer.as<cl_uint>() = 0; // not useful
      break;
    case CL_MEM_REFERENCE_COUNT:
      buffer.as<cl_uint>() = xocl(memobj)->count();
      break;
    case CL_MEM_CONTEXT:
      buffer.as<cl_context>() = xocl(memobj)->get_context();
      break;
    case CL_MEM_ASSOCIATED_MEMOBJECT:
      buffer.as<cl_mem>() = xocl(memobj)->get_sub_buffer_parent();
      break;
    case CL_MEM_OFFSET:
      buffer.as<size_t>() = xocl(memobj)->get_sub_buffer_offset();
      break;
    case CL_MEM_BANK:
      buffer.as<int>() = xocl(memobj)->get_memidx();
      break;
    default:
      throw error(CL_INVALID_VALUE,"clGetMemObjectInfo invalud param name");
      break;
  }

  return CL_SUCCESS;
}

} // xocl

cl_int
clGetMemObjectInfo(cl_mem           memobj,
                   cl_mem_info      param_name,
                   size_t           param_value_size,
                   void *           param_value,
                   size_t *         param_value_size_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clGetMemObjectInfo
      (memobj,param_name,param_value_size,param_value,param_value_size_ret);
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
