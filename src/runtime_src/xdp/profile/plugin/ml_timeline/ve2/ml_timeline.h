// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef XDP_PLUGIN_ML_TIMELINE_VE2_IMPL_H
#define XDP_PLUGIN_ML_TIMELINE_VE2_IMPL_H

#include "xdp/config.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_impl.h"

namespace xdp {

  class ResultBOContainer;
  class MLTimelineVE2Impl : public MLTimelineImpl
  {
    std::unique_ptr<ResultBOContainer> mResultBOHolder;
    public :
      MLTimelineVE2Impl(VPDatabase* dB, uint32_t sz);

      ~MLTimelineVE2Impl();

      virtual void updateDevice(void* devH, uint64_t devId = 0);
      virtual void finishflushDevice(void* hwCtxImpl, uint64_t implId = 0);
  };

}

#endif

