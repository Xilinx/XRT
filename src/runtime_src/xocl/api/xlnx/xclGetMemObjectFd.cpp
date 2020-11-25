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

// Copyright 2017-2020 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/memory.h"
#include "xocl/core/device.h"
#include "xocl/core/context.h"
#include "xocl/api/detail/memory.h"
#include "xocl/api/detail/device.h"

#include "CL/cl_ext_xilinx.h"

namespace xocl {

void
validOrError(cl_mem mem,
             int* fd)
{
  if (!config::api_checks())
    return;

  detail::memory::validOrError(mem);

  if (!fd)
    throw error(CL_INVALID_VALUE,"fd can not be nullptr. It must be address of variable that will get fd value");
}


static cl_int
clGetMemObjectFd(cl_mem mem,
                 int* fd) /* returned fd */
{
  validOrError(mem, fd);

  auto xmem = xocl(mem);
  auto context = xmem->get_context();
  for (auto device : context->get_device_range()) {
    if (auto boh = xmem->get_buffer_object_or_null(device)) {
      *fd = device->get_xdevice()->getMemObjectFd(boh);
      return CL_SUCCESS;
    }
  }
  throw error(CL_INVALID_MEM_OBJECT,"mem object is not associated with any device");
}

} // Namespace xocl END

namespace xlnx {

cl_int
clGetMemObjectFd(cl_mem mem,
                 int* fd)
{
  try {
    return xocl::clGetMemObjectFd(mem, fd);
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

} // xlnx


cl_int
xclGetMemObjectFd(cl_mem mem,
                  int* fd)
{
  return xlnx::clGetMemObjectFd(mem, fd);
}
