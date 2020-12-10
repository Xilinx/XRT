/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef xocl_core_error_h_
#define xocl_core_error_h_

#include "xrt/util/error.h"
#include "xrt/util/message.h"
#include <iostream>

#define DBG_EXCEPT_LOCK_FAILED     0x8000
#define DBG_EXCEPT_NOBUF_HANDLE    0x8001
#define DBG_EXCEPT_DBG_DISABLED    0x8002
#define DBG_EXCEPT_NO_DEVICE       0x8003
#define DBG_EXCEPT_NO_DBG_ACTION   0x8004
#define DBG_EXCEPT_INVALID_OBJECT  0x8005

namespace xocl {

// For now, no xocl specific exceptions
// If introduce, then derive off of xrt_xocl::error since xocl
// code catches xrt_xocl::error
using error = xrt_xocl::error;

inline void
send_exception_message(const char* msg)
{
  xrt_xocl::message::send(xrt_xocl::message::severity_level::error, msg);
}

} // xocl

#endif
