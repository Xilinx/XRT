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
#include "xocl/config.h"
#include "command_queue.h"
#include "context.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/context.h"
#include "xocl/core/error.h"
#include "xocl/api/api.h"
#include <CL/cl.h>

#ifdef _WIN32
# pragma warning( disable : 4245)
#endif


namespace xocl { namespace detail {

namespace command_queue {

void
validOrError(const cl_command_queue command_queue)
{
  if (!command_queue)
    throw error(CL_INVALID_COMMAND_QUEUE, "Invalid NULL command queue");
  context::validOrError(xocl(command_queue)->get_context());
}

void
validOrError(cl_command_queue_properties properties)
{
  cl_bitfield valid = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE;
  if(properties & (~valid))
    throw error(CL_INVALID_VALUE, "Invalid command queue property");
}

void
validOrError(const cl_device_id device, cl_command_queue_properties properties)
{
  validOrError(properties);

  cl_command_queue_properties supported = 0;
  api::clGetDeviceInfo(device,CL_DEVICE_QUEUE_PROPERTIES,sizeof(cl_command_queue_properties),&supported,nullptr);
  if(properties & (~supported))
    throw error(CL_INVALID_QUEUE_PROPERTIES, "Invalid command queue property");
}

}

}} // detail,xocl
