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

#ifndef xdp_profile_h
#define xdp_profile_h

/**
 * This file contains the API for adapting the xocl
 * data structures to the profiling infrastructure.
 *
 * The implementation of this API still requires old "xcl" data
 * hence profile.cpp currently lives under runtime_src/api/profile.cpp
 */

#include "xocl/core/object.h"
#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include <utility>
#include <string>

namespace XCL {
void register_xocl_profile_callbacks();
}
#endif


