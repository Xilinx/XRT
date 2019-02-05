/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef _XDP_XOCL_PROFILE_CB_H
#define _XDP_XOCL_PROFILE_CB_H

/**
 * This file contains the registered profiling callbacks
 */

#include "xocl_profile.h"
#include "xocl/api/plugin/xdp/profile.h"

#include <map>
#include <sstream>
#include <utility>
#include <string>

namespace xdp {
void register_xocl_profile_callbacks();
}
#endif
