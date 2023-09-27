/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_SOURCE

#include "aie_trace.h"

#include <boost/algorithm/string.hpp>

#include "core/common/message.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

constexpr uint32_t MAX_TILES = 400;
constexpr uint64_t ALIGNMENT_SIZE = 4096;

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  AieTrace_WinImpl::AieTrace_WinImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      : AieTraceImpl(database, metadata)
  {
    

  }

  void AieTrace_WinImpl::updateDevice()
  {
    // compile-time trace
    if (!metadata->getRuntimeMetrics()) {
      return;
    }

    // Set metrics for counters and trace events
    if (!setMetricsSettings(metadata->getDeviceID(), metadata->getHandle())) {
      std::string msg("Unable to configure AIE trace control and events. No trace will be generated.");
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }
  }

  // No CMA checks on Win
  uint64_t AieTrace_WinImpl::checkTraceBufSize(uint64_t size)
  {
    return size;
  }

  void AieTrace_WinImpl::flushAieTileTraceModule(){

  }


  void AieTrace_WinImpl::pollTimers(uint64_t index, void* handle) 
  {
    // TBD: poll AIE timers similar to Edge implementation
    (void)index;
    (void)handle;
  }
  
  bool AieTrace_WinImpl::setMetricsSettings(uint64_t deviceId, void* handle)
  {
    // Gather data to send to PS Kernel
    (void)deviceId;
    (void)handle;
    return false;
    
  }

  void AieTrace_WinImpl::freeResources() {
    //TODO

  }

  
}  // namespace xdp
