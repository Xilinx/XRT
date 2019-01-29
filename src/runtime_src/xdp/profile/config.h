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

#ifndef xdp_config_h_
#define xdp_config_h_

#include "xocl/core/debug.h"
#include "xrt/util/debug.h"

#ifdef XDP_VERBOSE
# define XDP_DEBUG(...) xrt::debug(__VA_ARGS__)
# define XDP_LOG(format,...) ::xocl::logf(format, ##__VA_ARGS__)
#else
# define XDP_DEBUG(...)
# define XDP_LOG(...)
#endif

#endif