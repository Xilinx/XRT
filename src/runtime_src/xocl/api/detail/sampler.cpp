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

#include "sampler.h"
#include "xocl/core/sampler.h"
#include "xocl/core/error.h"

namespace xocl { namespace detail {

namespace sampler {

void
validOrError(const cl_sampler sampler)
{
  if (!sampler)
    throw error(CL_INVALID_SAMPLER,"sampler is nullptr");
}

} // sampler

}} // detail,xocl


