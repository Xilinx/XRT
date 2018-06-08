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
#include "context.h"

namespace xocl {

sampler::
sampler(context* ctx, bool norm_mode, cl_addressing_mode addr_mode, cl_filter_mode filter_mode)
  : m_context(nullptr), m_norm_mode(norm_mode), m_addr_mode(addr_mode), m_filter_mode(filter_mode)
{}

sampler::
~sampler()
{}

} // xocl


