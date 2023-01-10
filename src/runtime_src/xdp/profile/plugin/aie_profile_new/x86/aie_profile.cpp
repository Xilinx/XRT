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

#define XDP_SOURCE 

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <memory>
#include <cstring>

#include "aie_profile.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/plugin/aie_profile_new/aie_profile_metadata.h"

namespace xdp {

  using severity_level = xrt_core::message::severity_level;

  AieProfile_x86Impl::AieProfile_x86Impl(VPDatabase* database, std::shared_ptr<AieProfileMetadata> metadata)
      : AieProfileImpl(database, metadata) {}

  void AieProfile_x86Impl::updateDevice()
  {
    //TODO
  }

  void AieProfile_x86Impl::poll(uint32_t index, void* handle)
  {
    //TODO
  }

  bool AieProfile_x86Impl::checkAieDevice(uint64_t deviceId, void* handle)
  {
    //TODO
    return true;
  }
}
