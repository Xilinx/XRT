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
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "detail/memory.h"
#include "detail/context.h"

#include <bitset>
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

namespace {

// Hack to determine if a context is associated with exactly one
// device.  Additionally, in emulation mode, the device must be
// active, e.g. loaded through a call to loadBinary.
//
// This works around a problem where clCreateBuffer is called in
// emulation mode before clCreateProgramWithBinary->loadBinary has
// been called.  The call to loadBinary can end up switching the
// device from swEm to hwEm.
//
// In non emulation mode it is sufficient to check that the context
// has only one device.
static xocl::device*
singleContextDevice(cl_context context)
{
  auto device = xocl::xocl(context)->get_device_if_one();
  if (!device)
    return nullptr;

  return (device->is_active())
    ? device
    : nullptr;
}

}

namespace xocl {

static void
validOrError(cl_context       context,
             cl_svm_mem_flags flags,
             size_t           size,
             unsigned int     alignment)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context
  detail::context::validOrError(context);

  // CL_INVALID_VALUE if values specified in flags are not valid as
  // defined in the table above
  // TODO: Check SVM flags when we going to support fine grain SVM buffer
  detail::memory::validOrError(flags);

  // CL_INVALID_BUFFER_SIZE if size is 0.
  if (!size)
    throw error(CL_INVALID_BUFFER_SIZE,"size==0");

  // CL_INVALID_VALUE if values specified in alignment are not 4096
  if (alignment != 4096)
    throw error(CL_INVALID_VALUE,"alignment need to be 4096");
}

static void*
clSVMAlloc(cl_context       context,
           cl_svm_mem_flags flags,
           size_t           size,
           unsigned int     alignment)
{
  if (!flags)
    flags = CL_MEM_READ_WRITE;

  validOrError(context,flags,size,alignment);

  if (auto device = singleContextDevice(context))
      return device->get_xrt_device()->alloc_svm(size);
  
  return nullptr;
}

} // xocl

void*
clSVMAlloc(cl_context       context,
           cl_svm_mem_flags flags,
           size_t           size,
           unsigned int     alignment)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clSVMAlloc
      (context,flags,size,alignment);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
  }
  return nullptr;
}
