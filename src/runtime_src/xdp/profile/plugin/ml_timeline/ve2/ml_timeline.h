/**
 * Copyright (C) 2025 Advanced Micro Devices, Inc. - All rights reserved
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

      virtual void updateDevice(void* devH);
      virtual void finishflushDevice(void* hwCtxImpl, uint64_t implId = 0);
  };

}

#endif

