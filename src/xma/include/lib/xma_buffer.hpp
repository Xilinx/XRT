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

#ifndef _XMA_BUFFER_PLGCLS_H_
#define _XMA_BUFFER_PLGCLS_H_

#include "core/include/experimental/xrt_bo.h"
#include "xrt.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace xma_core {
namespace plg {

using memory_group = xrtMemoryGroup;

/* class buffer : abstraction for xma buffer */
class buffer
{
private:
    void*    dummy{nullptr};
    xrt::bo  xrt_bo;
    int32_t  bank_index;
    int32_t  dev_index;
    std::atomic<int32_t> ref_cnt{0};//For use by plugins when shared by plugins
    bool     device_only_buffer;
    void*    dummy2{nullptr};

public:
  int32_t
  read_ddr(int32_t offset, uint64_t size) const;

  int32_t
  write_ddr(int32_t offset, uint64_t size) const;

  buffer(xclDeviceHandle dhdl, xrt::bo::flags flags, memory_group grp, uint64_t sz);
  ~buffer(); //bo_impl automatically does free buffer when shared_ptr ref_cnt is zero


}; //class buffer

}} //namespace xma_core->plg
#endif
