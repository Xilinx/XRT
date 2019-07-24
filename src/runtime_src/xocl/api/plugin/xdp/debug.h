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

#ifndef xocl_api_debug_h
#define xocl_api_debug_h
#include <functional>

struct axlf;

/**
 * This file contains the API for adapting the xocl data structures to
 * the infrastructure for debugging of the binary.
 */
namespace xocl { 

namespace debug {

using cb_reset_type = std::function<void (const axlf* xclbin)>;

void
register_cb_reset (cb_reset_type && cb);

void
reset(const axlf* xclbin);

}} // debug,xocl

#endif


