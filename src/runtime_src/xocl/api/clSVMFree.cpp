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
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xrt/util/memory.h"
#include "profile.h"

#include "detail/memory.h"
#include "detail/context.h"

#include <bitset>

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
             void*            svm_pointer)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context
  detail::context::validOrError(context);

}

void clSVMFree(cl_context     context,
               void *         svm_pointer)
{
  validOrError(context,svm_pointer);

  // If a NULL pointer is passed in svm_pointer, no action occurs.
  if (svm_pointer) {
    if (auto device = singleContextDevice(context)) {
      device->get_xrt_device()->free_svm(svm_pointer);
    }
  }

}

} // xocl

// Note that clSVMFree does not wait for previously enqueued commands
// that may be using svm_pointer to finish before freeing svm_pointer.
// It is the responsibility of the application to make sure that enqueued
// commands that use svm_pointer have finished before freeing svm_pointer.
// This can be done by enqueuing a blocking operation such as clFinish,
// clWaitForEvents, clEnqueueReadBuffer or by registering a callback with
// the events associated with enqueued commands and when the last enqueued
// comamnd has finished freeing svm_pointer.
void clSVMFree(cl_context     context,
               void *         svm_pointer)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    xocl::clSVMFree
      (context,svm_pointer);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
  }
}
