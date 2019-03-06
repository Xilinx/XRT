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
#include "xocl/core/device.h"
#include "xocl/core/context.h"
#include "xocl/api/detail/memory.h"
#include "xocl/api/detail/device.h"
#include "xocl/api/detail/context.h"

namespace xocl {

void
validOrError(cl_context context,
             cl_device_id device,
             cl_mem_flags flags,
             int fd,
             cl_mem* mem) /* returned cl_mem */
{
  if (!config::api_checks())
    return;

  detail::context::validOrError(context);
  detail::device::validOrError(device);
  detail::device::validOrError(context,1,&device);

  // CL_INVALID_VALUE if values specified in flags are not valid as
  // defined in the table above
  detail::memory::validOrError(flags);

  if (!fd)
    throw error(CL_INVALID_VALUE,"fd can not be zero.");

  if (!mem)
    throw error(CL_INVALID_VALUE,"mem can not be nullptr. It must be address of variable that will get cl_mem pointer");

  if (flags & (CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_ALLOC_HOST_PTR))
    throw error(CL_INVALID_VALUE, "clGetMemObjectFromFd: unsupported host_ptr flags");

}

static cl_int
clGetMemObjectFromFd(cl_context context,
                     cl_device_id device,
                     cl_mem_flags flags,
                     int fd,
                     cl_mem* mem) /* returned cl_mem */
{
  if (!flags)
    flags = CL_MEM_READ_WRITE;

  validOrError(context, device, flags, fd, mem);

  auto xcontext = xocl(context);
  auto xdevice  = xocl(device);

  size_t size = 0;
  unsigned iflags = flags;
  if (auto boh = xdevice->get_xrt_device()->getBufferFromFd(fd, size, iflags)) {
    auto buffer = std::make_unique<xocl::buffer>(xcontext, flags, size, nullptr);
    // set fields in cl_buffer
    buffer->set_ext_flags(get_xlnx_ext_flags(flags,nullptr));

    buffer->update_buffer_object_map(xdevice,boh);
    *mem = buffer.release();
    return CL_SUCCESS;

    //Sarab: How to handle importing buffer which is on multiple devices?
    //That will need to change update_buffer_object_mao functions as well..


    // allocate device buffer object if context has only one device
    // and if this is not a progvar (clCreateProgramWithBinary)
    /*
    if (!(flags & CL_MEM_PROGVAR)) {
      if (auto device = singleContextDevice(context)) {
        buffer->get_buffer_object(device);
      }
    }
    */

  }

  throw error(CL_INVALID_MEM_OBJECT, "CreateBufferFromFd: Unable to get MemObject Handle from FD");
}

} // Namespace xocl END

namespace xlnx {

cl_int
clGetMemObjectFromFd(cl_context context,
                     cl_device_id device,
                     cl_mem_flags flags,
                     int fd,
                     cl_mem* mem)
{
  try {
    return xocl::clGetMemObjectFromFd(context, device, flags, fd, mem);
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

} // xlnx


cl_int
xclGetMemObjectFromFd(cl_context context,
                      cl_device_id device,
                      cl_mem_flags flags,
                      int fd,
                      cl_mem* mem)
{
  return xlnx::clGetMemObjectFromFd(context, device, flags, fd, mem);
}
