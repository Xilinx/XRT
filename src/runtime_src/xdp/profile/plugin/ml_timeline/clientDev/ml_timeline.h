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

#ifndef XDP_PLUGIN_ML_TIMELINE_CLIENTDEV_IMPL_H
#define XDP_PLUGIN_ML_TIMELINE_CLIENTDEV_IMPL_H

#include "xdp/config.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_impl.h"

namespace xdp {

  class ResultBOContainer;
  class MLTimelineClientDevImpl : public MLTimelineImpl
  {
    std::unique_ptr<ResultBOContainer> mResultBOHolder;
    public :
      MLTimelineClientDevImpl(VPDatabase* dB, uint32_t sz);

      ~MLTimelineClientDevImpl();

      virtual void updateDevice(void* hwCtxImpl);
      virtual void finishflushDevice(void* hwCtxImpl, uint64_t implId = 0);
  };

}

#endif

