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

#include "program.h"
#include "xocl/core/program.h"
#include "xocl/core/device.h"
#include "xocl/core/error.h"

namespace xocl { namespace detail {

namespace program {

void
validOrError(cl_program program)
{
  // CL_INVALID_PROGRAM if program is not a valid program object.
  if (!program)
    throw error(CL_INVALID_PROGRAM,"program is nullptr");
}

void
validExecutableOrError(const cl_program program)
{
  // CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built
  // executable for any device in program.
  auto xp = xocl(program);
  auto dr = xp->get_device_range();
#ifndef _WIN32
  auto itr = std::find_if(dr.begin(),dr.end(),
                          [xp](const device* dev)
                          {return xp->get_build_status(dev)==CL_BUILD_SUCCESS; });
#else
  auto itr = dr.begin();
  auto end = dr.end();
  for (; itr != end; ++itr) {
    auto dev = (*itr);
    if (xp->get_build_status(dev)==CL_BUILD_SUCCESS)
      break;
  }
#endif
  if (itr == dr.end())
    throw error(CL_INVALID_PROGRAM_EXECUTABLE,"no executable for program");
}

} // program

}} // detail,xocl
