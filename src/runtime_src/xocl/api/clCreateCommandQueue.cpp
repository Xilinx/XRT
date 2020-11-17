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

#include "xocl/core/command_queue.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/error.h"
#include "detail/context.h"
#include "detail/device.h"
#include "detail/command_queue.h"

#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

namespace xocl {

static void
validOrError(cl_context                  context,
             cl_device_id                device,
             cl_command_queue_properties properties)
{
  if(!config::api_checks())
    return;

  detail::context::validOrError(context);
  detail::device::validOrError(device);
  detail::command_queue::validOrError(device,properties);
}


static cl_command_queue
clCreateCommandQueue(cl_context                  context,
                     cl_device_id                device,
                     cl_command_queue_properties properties,
                     cl_int *                    errcode_ret)
{
  validOrError(context,device,properties);

  auto command_queue =
    std::make_unique<xocl::command_queue>(xocl::xocl(context), xocl::xocl(device), properties);
  xocl::assign(errcode_ret,CL_SUCCESS);
  return command_queue.release();
}

} // xocl


cl_command_queue
clCreateCommandQueue(cl_context                  context,
                     cl_device_id                device,
                     cl_command_queue_properties properties,
                     cl_int *                    errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clCreateCommandQueue
      (context, device, properties, errcode_ret);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = ex.get();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = CL_OUT_OF_HOST_MEMORY;
  }
  return nullptr;
}


