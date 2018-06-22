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

#include "context.h"
#include "command_queue.h"
#include "xocl/core/context.h"
#include "xocl/core/memory.h"
#include "xocl/core/error.h"
#include "xocl/core/property.h"

namespace xocl { namespace detail {

namespace context {

void
validOrError(const cl_context context)
{
  if (!context)
    throw error(CL_INVALID_CONTEXT,"context is nullptr");
}

void
validOrError(const cl_context context,const std::vector<cl_mem>& mem_objects)
{
  validOrError(context);
  if (std::any_of(mem_objects.begin(),mem_objects.end(),
                  [context](cl_mem mem) { return xocl(mem)->get_context() != context; }))
    throw error(CL_INVALID_CONTEXT,"context different from memory context");
}

void
validOrError(const cl_context_properties* properties)
{
  xocl::property_list<cl_context_properties> context_properties(properties);
  for (auto prop : context_properties) {
    auto key = prop.get_key();
    if (key==CL_CONTEXT_PLATFORM)
      ;
    else if (key==CL_CONTEXT_INTEROP_USER_SYNC)
      ;
    else
      throw error(CL_INVALID_PROPERTY,"bad context property '" + std::to_string(key) + "'");
  }
}

} // context

}} // detail,xocl


