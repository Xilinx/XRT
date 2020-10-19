/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef _XMA_BUFFERCLS_H_
#define _XMA_BUFFERCLS_H_

#include <cstdint>
#include <cstring>
#include <string>
#include <atomic>
#include "xrt.h"

namespace xma_core {
namespace plg {

/**
 * class buffer : abstraction for xma buffer
 */
class buffer
{
private:
    void*    dummy{nullptr};
    uint8_t* data{nullptr};
    uint64_t size;
    uint64_t paddr;
    int32_t  bank_index;
    int32_t  dev_index;
    xclBufferHandle boHandle;
    std::atomic<int32_t> ref_cnt{0};
    bool     device_only_buffer;
    xclDeviceHandle dev_handle;
    uint32_t reserved[4];

  ~buffer();

public:
  int32_t
  read_ddr(int32_t offset, uint64_t size) const;

  int32_t
  write_ddr(int32_t offset, uint64_t size) const;

  buffer(int32_t dev_idx, int32_t bank_idx, uint64_t sz, bool dev_only);
  void destroy();

}; //class buffer

}} //namespace xma_core->plg
#endif
