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

#define XDP_PLUGIN_SOURCE

#include "aie_driver_common_util.h"
#include "xdp/profile/database/static_info/aie_util.h"

#include <boost/algorithm/string.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>
#include <set>

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

#include "core/common/message.h"

// ***************************************************************
// Anonymous namespace for helper functions local to this file
// ***************************************************************
namespace xdp::aie {
  using severity_level = xrt_core::message::severity_level;

} // namespace xdp::aie