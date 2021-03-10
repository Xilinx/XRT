/**
 * Copyright (C) 2020 Xilinx, Inc
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
#include "lib/xma_buffer.hpp"
#include "app/xmaerror.h"
#include <stdexcept>

namespace xma_core {
namespace plg {



buffer::buffer(xclDeviceHandle dhdl, xrt::bo::flags flags, memory_group grp, uint64_t sz)
:xrt_bo{dhdl, sz, flags, grp} 
{
  //TODO
  throw std::runtime_error(" --- TODO --");
}

int32_t
buffer::read_ddr(int32_t offset, uint64_t size) const
{
  //TODO

  return XMA_ERROR;
}

int32_t
buffer::write_ddr(int32_t offset, uint64_t size) const
{
  //TODO

  return XMA_ERROR;
}

}} //namespace xma_core->plg

