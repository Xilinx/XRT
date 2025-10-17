// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef XDP_PLUGIN_ML_TIMELINE_IMPL_H
#define XDP_PLUGIN_ML_TIMELINE_IMPL_H

#include "core/include/xrt/xrt_hw_context.h"

namespace xdp {

  // Each record timer entry has 32bit ID and 32bit AIE High Timer + 32bit AIE Low Timer value.
  constexpr uint32_t RECORD_TIMER_ENTRY_SZ_IN_BYTES = 3*sizeof(uint32_t);

  class VPDatabase;

  class MLTimelineImpl
  {
    protected :
      VPDatabase* db = nullptr;
      uint32_t mBufSz;
      uint32_t mNumBufSegments;

    public:
      MLTimelineImpl(VPDatabase* dB, uint32_t sz)
        : db(dB),
          mBufSz(sz),
          mNumBufSegments(1)
      {}

      MLTimelineImpl() = delete;

      virtual ~MLTimelineImpl() {}

      virtual void updateDevice(void*, uint64_t) = 0;
      virtual void finishflushDevice(void*, uint64_t) = 0;
  };

}
#endif

