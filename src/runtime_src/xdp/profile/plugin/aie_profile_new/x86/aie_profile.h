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

#ifndef AIE_PROFILE_H
#define AIE_PROFILE_H

#include <cstdint>

#include "xdp/profile/plugin/aie_profile_new/aie_profile_impl.h"

namespace xdp {
  
  class AieProfile_x86Impl : public AieProfileImpl{
    public:
      // AieProfile_x86Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      //   : AieProfileImpl(database, metadata){}
      AieProfile_x86Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata);

      ~AieProfile_x86Impl() = default;

      void updateDevice();
      void poll(uint32_t index, void* handle);
      bool checkAieDevice(uint64_t deviceId, void* handle);
  };

}   

#endif
