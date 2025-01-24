/**
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_DEBUG_IMPL_H
#define AIE_DEBUG_IMPL_H

#include "aie_debug_metadata.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  // AIE debugging can be done in different ways depending on the platform.
  class AieDebugImpl
  {

  protected:
    VPDatabase* db = nullptr;
    std::shared_ptr<AieDebugMetadata> metadata;

  public:
    AieDebugImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata)
      : db(database), metadata(metadata) {}

    AieDebugImpl() = delete;
    virtual ~AieDebugImpl() {};

    virtual void updateDevice() = 0;
    virtual void updateAIEDevice(void* handle)=0;
    virtual void poll(const uint64_t index, void* handle) = 0;

    const std::map<module_type, const char*> moduleTypes = {
      {module_type::core, "AIE"},
      {module_type::dma, "DMA"},
      {module_type::shim, "Interface"},
      {module_type::mem_tile, "Memory Tile"}
    };
  };

} // namespace xdp

#endif