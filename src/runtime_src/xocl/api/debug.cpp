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

#include "debug.h"
#include "xocl/xclbin/xclbin.h"

namespace xocl { namespace debug {

cb_reset_type cb_reset;

void
register_cb_reset (cb_reset_type&& cb)
{
  cb_reset = std::move(cb);
}

void
reset(const xocl::xclbin& xclbin)
{
  if (cb_reset)
    cb_reset(xclbin);
}

}} // debug,xocl



