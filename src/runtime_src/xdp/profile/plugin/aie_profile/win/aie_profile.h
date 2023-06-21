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

#ifndef AIE_PROFILE_H
#define AIE_PROFILE_H

#include <cstdint>

#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_impl.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

  class AieProfile_WinImpl : public AieProfileImpl {
   public:
    AieProfile_WinImpl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);
    ~AieProfile_WinImpl() = default;

    void updateDevice();
    void poll(uint32_t index, void* handle);
    void freeResources();
    bool setMetricsSettings(uint64_t deviceId, void* handle);

   private:
      XAie_DevInst aieDevInst = { 0 };
     
  };

}  // namespace xdp

#endif
