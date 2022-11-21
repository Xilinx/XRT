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

#include <boost/algorithm/string.hpp>

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "aie_profile_metadata.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

    AieProfileMetadata::AieProfileMetadata(uint64_t deviceID, void* handle)
  : deviceID(deviceID)
  , handle(handle)
  {}

  void AieProfileMetadata::parsePollingInterval()
  {
    // Get polling interval (in usec; minimum is 100)
    mPollingInterval = xrt_core::config::get_aie_profile_settings_interval_us();
    if (1000 == mPollingInterval) {
      // If set to default value, then check for old style config 
      mPollingInterval = xrt_core::config::get_aie_profile_interval_us();
      if (1000 != mPollingInterval) {
        xrt_core::message::send(severity_level::warning, "XRT", 
          "The xrt.ini flag \"aie_profile_interval_us\" is deprecated and will be removed in future release. Please use \"interval_us\" under \"AIE_profile_settings\" section.");
      }
    }
  }


}