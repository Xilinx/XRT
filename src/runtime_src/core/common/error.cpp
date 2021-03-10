/**
 * Copyright (C) 2020 Xilinx, Inc
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

#define XRT_CORE_COMMON_SOURCE
#include "error.h"
#include "message.h"

namespace xrt_core {

void
send_exception_message(const char* msg, const char* tag)
{
  message::send(message::severity_level::error, tag, msg);
}

void
send_exception_message(const std::string& msg, const char* tag)
{
  message::send(message::severity_level::error, tag, msg);
}

} // xrt_core
