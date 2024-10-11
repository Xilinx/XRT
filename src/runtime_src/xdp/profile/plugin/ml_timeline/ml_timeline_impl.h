/**
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_PLUGIN_ML_TIMELINE_IMPL_H
#define XDP_PLUGIN_ML_TIMELINE_IMPL_H

#include "core/include/xrt/xrt_hw_context.h"

namespace xdp {

  class VPDatabase;

  class MLTimelineImpl
  {
    protected :
      VPDatabase* db = nullptr;
      uint32_t mBufSz;

    public:
      MLTimelineImpl(VPDatabase* dB)
        : db(dB)
      {}

      MLTimelineImpl() = delete;

      virtual ~MLTimelineImpl() {}

      virtual void updateDevice(void*) = 0;
      virtual void finishflushDevice(void*, uint64_t) = 0;

      void setBufSize(uint32_t sz)
      {
        mBufSz = sz;
      }
  };

}
#endif

