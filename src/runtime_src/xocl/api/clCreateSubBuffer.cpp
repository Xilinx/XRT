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
#include "detail/memory.h"
#include "plugin/xdp/profile.h"

#include "xrt/util/memory.h"

namespace xocl {

static void
validOrError(cl_mem                   buffer,
             cl_mem_flags             flags,
             cl_buffer_create_type    buffer_create_type,
             const void *             buffer_create_info,
             cl_int *                 errcode_ret)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object or is a sub-buffer object
  detail::memory::validOrError(buffer);
  if (xocl(buffer)->get_sub_buffer_parent())
    throw error(CL_INVALID_MEM_OBJECT,"buffer is already a sub buffer");

  detail::memory::validOrError(flags);

  auto bflags = xocl(buffer)->get_flags();
  // CL_INVALID_VALUE if buffer was created with CL_MEM_WRITE_ONLY and
  // flags specifies CL_MEM_READ_WRITE or CL_MEM_READ_ONLY, or if
  // buffer was created with CL_MEM_READ_ONLY and flags specifies
  // CL_MEM_READ_WRITE or CL_MEM_WRITE_ONLY, or if flags specifies
  // CL_MEM_USE_HOST_PTR or CL_MEM_ALLOC_HOST_PTR or
  // CL_MEM_COPY_HOST_PTR.
  if ( ( (bflags & CL_MEM_WRITE_ONLY) && ( (flags & CL_MEM_READ_ONLY)  || (flags & CL_MEM_READ_WRITE) ) ) ||
       ( (bflags & CL_MEM_READ_ONLY)  && ( (flags & CL_MEM_WRITE_ONLY) || (flags & CL_MEM_READ_WRITE) ) ) ||
       ( (flags & CL_MEM_USE_HOST_PTR) || (flags & CL_MEM_ALLOC_HOST_PTR) || (flags & CL_MEM_COPY_HOST_PTR) )
     )
    throw error(CL_INVALID_VALUE,"bad flags 1");

  // CL_INVALID_VALUE if buffer was created with
  // CL_MEM_HOST_WRITE_ONLY and flags specifies CL_MEM_HOST_READ_ONLY
  // or if buffer was created with CL_MEM_HOST_READ_ONLY and flags
  // specifies CL_MEM_HOST_WRITE_ONLY, or if buffer was created with
  // CL_MEM_HOST_NO_ACCESS and flags specifies CL_MEM_HOST_READ_ONLY
  // or CL_MEM_HOST_WRITE_ONLY.
  if ( ( (bflags & CL_MEM_HOST_WRITE_ONLY) && (flags & CL_MEM_HOST_READ_ONLY)  ) ||
       ( (bflags & CL_MEM_HOST_READ_ONLY)  && (flags & CL_MEM_HOST_WRITE_ONLY) ) ||
       ( (bflags & CL_MEM_HOST_NO_ACCESS)  && ( (flags & CL_MEM_HOST_READ_ONLY) || (flags & CL_MEM_HOST_WRITE_ONLY) ) )
     )
    throw error(CL_INVALID_VALUE,"bad flags 2");

  // CL_INVALID_VALUE if value specified in buffer_create_type is not valid.
  if(buffer_create_type!=CL_BUFFER_CREATE_TYPE_REGION)
    throw error(CL_INVALID_VALUE,"buffer_create_type is not valid");

  // CL_INVALID_VALUE if value(s) specified in buffer_create_info (for
  // a given buffer_create_type) is not valid or if buffer_create_info
  // is NULL.
  if(!buffer_create_info)
    throw error(CL_INVALID_VALUE,"buffer_create_info is not null");

  // cast based on buffer_create_type
  auto region = reinterpret_cast<const cl_buffer_region*>(buffer_create_info);
  if ( (region->origin+region->size) > xocl(buffer)->get_size())
    throw error(CL_INVALID_VALUE,"buffer_create_info buffer overflow");

  // CL_INVALID_BUFFER_SIZE if size is 0.
  if (region->size==0)
    throw error(CL_INVALID_VALUE,"buffer_create_info invalid size==0");
}

static cl_mem
clCreateSubBuffer(cl_mem                   parentbuffer,
                  cl_mem_flags             flags,
                  cl_buffer_create_type    buffer_create_type,
                  const void *             buffer_create_info,
                  cl_int *                 errcode_ret)
{
  validOrError(parentbuffer,flags,buffer_create_type,buffer_create_info,errcode_ret);

  auto pflags = xocl(parentbuffer)->get_flags();

  // Inherit device access flags from parent buffer if not specified
  auto device_access_flags = (CL_MEM_READ_WRITE | CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY);
  flags = (flags | (flags & device_access_flags ? 0 : pflags & device_access_flags));

  // Inherit host ptr flags from parent buffer
  auto host_ptr_flags = (CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR);
  flags = (flags | (pflags & host_ptr_flags));
  
  // Inherit host access flags from parent buffer if not specified
  auto host_access_flags = (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS);
  flags = (flags | (flags & host_access_flags ? 0 : pflags & host_access_flags));

  size_t sz = 0;
  size_t offset = 0;
  if (buffer_create_type & CL_BUFFER_CREATE_TYPE_REGION) {
    auto region = reinterpret_cast<const cl_buffer_region*>(buffer_create_info);
    sz = region->size;
    offset = region->origin;
  } 

  auto usb = xrt::make_unique<sub_buffer>(xocl(parentbuffer),flags,offset,sz);

  assign(errcode_ret,CL_SUCCESS);
  return usb.release();
}

} // xocl


cl_mem
clCreateSubBuffer(cl_mem                   parentbuffer,
                  cl_mem_flags             flags,
                  cl_buffer_create_type    buffer_create_type,
                  const void *             buffer_create_info,
                  cl_int *                 errcode_ret)
{
  try {
    return xocl::clCreateSubBuffer
      (parentbuffer,flags,buffer_create_type,buffer_create_info,errcode_ret);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}



