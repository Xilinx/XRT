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

#include "kernel.h"
#include "xocl/core/kernel.h"
#include "xocl/core/device.h"
#include "xocl/core/program.h"
#include "xocl/core/error.h"

namespace xocl { namespace detail {

namespace kernel {

void 
validOrError(cl_kernel kernel)
{
  if (!kernel)
    throw error(CL_INVALID_KERNEL,"kernel is nullptr");
}

void
validOrError(const cl_device_id device, const cl_kernel kernel)
{
  validOrError(kernel);

  if (device && !xocl(kernel)->get_program()->has_device(xocl(device)))
    throw error(CL_INVALID_DEVICE,"device not associated with kernel");
  if (!device && xocl(kernel)->get_program()->num_devices()>1)
    throw error(CL_INVALID_DEVICE,"device not specified");
}

void
validArgsOrError(const cl_kernel kernel)
{
  for (const auto& arg : xocl(kernel)->get_indexed_xargument_range())
    if (!arg->is_set())
      throw xocl::error(CL_INVALID_KERNEL_ARGS,"Kernel arg '" + arg->get_name() + "' is not set");
}

} // kernel

}} // detail,xocl



