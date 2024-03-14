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

#ifndef AIE_PROFILE_IMPL_H
#define AIE_PROFILE_IMPL_H

#include "aie_profile_metadata.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  // AIE profile configurations can be done in different ways depending
  // on the platform.  For example, platforms like the VCK5000 or
  // discovery platform, where the host code runs on the x86 and the AIE
  // is not directly accessible, will require configuration be done via
  // PS kernel. 
  class AieProfileImpl
  {

  protected:
    VPDatabase* db = nullptr;
    std::shared_ptr<AieProfileMetadata> metadata;

  public:
    AieProfileImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      :db(database), metadata(metadata) {}

    AieProfileImpl() = delete;
    virtual ~AieProfileImpl() {};

    virtual void updateDevice() = 0;
    virtual void poll(const uint32_t index, void* handle) = 0;
    virtual void freeResources() = 0;
  };

} // namespace xdp

#endif