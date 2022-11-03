/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_TRACE_DOT_H
#define AIE_TRACE_DOT_H

#include <cstdint>

#include "xdp/profile/plugin/aie_trace_new/aie_trace_impl.h"

namespace xdp {

  class AieTrace_x86Impl : public AieTraceImpl{
    public:

      AieTrace_x86Impl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
        : AieTraceImpl(database, metadata){}
      ~AieTrace_x86Impl() = default;
      virtual void updateDevice();
      bool setMetrics(uint64_t deviceId, void* handle);
      uint64_t checkTraceBufSize(uint64_t size);
      void parseMessages(uint8_t* messages);
  };

}   

#endif
