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

#include "aie_trace_metadata.h"
#include <cstdint>
#include <memory>

namespace xdp {
  
  class VPDatabase;

  // AIE trace configurations can be done in different ways depending
  // on the platform.  For example, platforms like the VCK5000 or
  // discovery platform, where the host code runs on the x86 and the AIE
  // is not directly accessible, will require configuration be done via
  // PS kernel. 
  class AieTraceImpl {
  public:
    AieTraceImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      :db(database), metadata(metadata) {}

    AieTraceImpl() = delete;
    virtual ~AieTraceImpl() {};

  protected:
    /****************************************************************************
     * Database for configuration and results
     ***************************************************************************/
    VPDatabase* db = nullptr;

    /****************************************************************************
     * Trace metadata parsed from user settings
     ***************************************************************************/
    std::shared_ptr<AieTraceMetadata> metadata;

  public:
    /****************************************************************************
     * Update device (e.g., after loading xclbin)
     ***************************************************************************/
    virtual void updateDevice() = 0;

    /****************************************************************************
     * Stop and release resources (e.g., counters, ports)
     ***************************************************************************/
    virtual void freeResources() = 0;

    /****************************************************************************
     * Poll AIE timers (for system timeline only)
     ***************************************************************************/
    virtual void pollTimers(uint64_t index, void* handle) = 0;

    /****************************************************************************
     * Verify correctness of trace buffer size
     ***************************************************************************/
    virtual uint64_t checkTraceBufSize(uint64_t size) = 0;

    /****************************************************************************
     * Flush trace modules by forcing end events
     *
     * Trace modules buffer partial packets. At end of run, these need to be 
     * flushed using a custom end event. This applies to trace windowing and 
     * passive tiles like memory and interface.
     *
     ***************************************************************************/
    virtual void flushTraceModules() = 0;
  };

} // namespace xdp

#endif
