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

#ifndef AIE_TRACE_IMPL_H
#define AIE_TRACE_IMPL_H

#include <cstdint>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"

#include "aie_trace_metadata.h"

namespace xdp {

  // AIE trace configurations can be done in different ways depending
  // on the platform.  For example, platforms like the VCK5000 or
  // discovery platform, where the host code runs on the x86 and the AIE
  // is not directly accessible, will require configuration be done via
  // PS kernel. 
  class AieTraceImpl
  {

  protected:
    VPDatabase* db = nullptr;
    std::shared_ptr<AieTraceMetadata> metadata;

  public:
    AieTraceImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      :db(database), metadata(metadata) {}

    AieTraceImpl() = delete;
    virtual ~AieTraceImpl() {};

    virtual void updateDevice() = 0;
    virtual void flushDevice() = 0;
    virtual void finishFlushDevice() = 0;
    virtual uint64_t checkTraceBufSize(uint64_t size) = 0;
  };

} // namespace xdp

#endif
