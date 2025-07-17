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

  /** 
   * @brief   Base class for AI Engine trace implementations
   * @details Trace configurations can be done in different ways depending
   *          on the platform.  For example, platforms like the VCK5000 or
   *          discovery platform, where the host code runs on the x86 and the 
   *          AIE is not directly accessible, will require configuration be 
   *          done via PS kernel.
   */ 
  class AieTraceImpl {
  public:
    /**
     * @brief AIE Trace implementation constructor
     * @param database Profile database for storing results and configuation 
     * @param metadata Design-specific AIE metadata typically taken from xclbin
     */
    AieTraceImpl(VPDatabase* database, std::shared_ptr<AieTraceMetadata> metadata)
      :db(database), metadata(metadata) {}

    AieTraceImpl() = delete;
    /// @brief AIE Trace implementation destructor
    virtual ~AieTraceImpl() {};

  protected:
    /// @brief Database for configuration and results
    VPDatabase* db = nullptr;

    /// @brief Trace metadata parsed from user settings
    std::shared_ptr<AieTraceMetadata> metadata;

  public:
    /// @brief Update device (e.g., after loading xclbin)
    virtual void updateDevice() = 0;

    /// @brief Stop and release resources (e.g., counters, ports)
    virtual void freeResources() = 0;

    /**
     * @brief Poll AIE timers (for system timeline only)
     * @param index  Device index number
     * @param handle Pointer to device handle
     */
    virtual void pollTimers(uint64_t index, void* handle) = 0;

    /**
     * @brief Verify correctness of trace buffer size
     * @param size Requested size of trace buffer
     */
    virtual uint64_t checkTraceBufSize(uint64_t size) = 0;

    /**
     * @brief   Flush trace modules by forcing end events
     * @details Trace modules buffer partial packets. At end of run, these need to be 
     *          flushed using a custom end event. This applies to trace windowing and 
     *          passive tiles like memory and interface.
     */
    virtual void flushTraceModules() = 0;

    /**
     * @brief Set AIE device instance
     * @param handle Pointer to device handle
     * @return Pointer to AIE device instance
     */
    virtual void* setAieDeviceInst(void* handle, uint64_t deviceID) = 0;
  };

} // namespace xdp

#endif
