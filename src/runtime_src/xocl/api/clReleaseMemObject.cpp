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
#include "xocl/core/command_queue.h"
#include "detail/memory.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_mem memobj)
{
  if (!config::api_checks())
    return;

  detail::memory::validOrError(memobj);
}

static cl_int
clReleaseMemObject(cl_mem memobj)
{
  validOrError(memobj);

  if (!xocl(memobj)->release())
    return CL_SUCCESS;

  // Host Accessible Prorgam Scope Globals
  // Progvars are deleted via kernel argument destruction through
  // regular reference counting.  Here we just make sure progvars
  // are not deleted via clReleaseMemObject.
  auto flags = xocl(memobj)->get_flags();
  if (flags & CL_MEM_PROGVAR) {
    XOCL_DEBUG(std::cout,"clReleaseMemObject on user buffer backed by external progvar, mem obj not deleted\n");
    return CL_SUCCESS;
  }

#if 0
  // After the memobj reference count becomes zero *and* commands
  // queued for execution on a command-queue(s) that use memobj have
  // finished, the memory object is deleted.  Easiest is to just
  // wait for all command queues.
  auto context = xocl(memobj)->get_context();
  for (auto command_queue : context->get_queue_range())
    command_queue->wait();
#endif

  delete xocl(memobj);
  return CL_SUCCESS;
}

} // xocl

cl_int
clReleaseMemObject(cl_mem memobj)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clReleaseMemObject(memobj);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
