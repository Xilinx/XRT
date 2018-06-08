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

#include "pipe.h"
#include "xocl/api/detail/command_queue.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/pipe.h"
#include "xocl/core/error.h"

namespace xocl { namespace pmd { namespace detail {

namespace pipe {

void
validOrError(const cl_pipe pipe)
{
  if (!pipe)
    throw error(CL_INVALID_VALUE,"pipe is nullptr");
}

void
validOrError(const cl_pipe pipe, const cl_command_queue command_queue)
{
  validOrError(pipe);
  ::xocl::detail::command_queue::validOrError(command_queue);
  if (xocl::xocl(command_queue)->get_device() != xocl::xocl(pipe)->get_device())
    throw error(CL_INVALID_DEVICE,"pipe and command queue device are different");

  if (!(xocl::xocl(command_queue)->get_properties() & CL_QUEUE_DPDK))
    throw error(CL_INVALID_COMMAND_QUEUE,"properties do not specifiy CL_QUEUE_DPDK");
}

} // pipe

}}} // pmd,detail,xocl


