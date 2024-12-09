/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef xrtcore_message_h_
#define xrtcore_message_h_
#include "core/common/config.h"
#include "core/common/config_reader.h"
#include "core/include/xrt.h"
#include "core/include/xrt/experimental/xrt_message.h"
#include <string>
#include <cstdio>
#include <vector>

namespace xrt_core { namespace message {

using severity_level = xrt::message::level;

XRT_CORE_COMMON_EXPORT
void
send(severity_level l, const char* tag, const char* msg);

void
sendv(severity_level l, const char* tag, const char* format, va_list args);

inline void
send(severity_level l, const std::string& tag, const std::string& msg)
{
  send(l, tag.c_str(), msg.c_str());
}

template <typename ...Args>
void
send(severity_level l, const char* tag, const char* format, Args ... args)
{
  int ver = xrt_core::config::get_verbosity();
  int lev = static_cast<int>(l);

  if (ver >= lev) {
    auto sz = snprintf(nullptr, 0, format, args ...);
    if (sz < 0) {
      send(severity_level::error, tag, "Illegal arguments in log format string");
      return;
    }

    std::vector<char> buf(sz + 1);
    snprintf(buf.data(), sz + 1, format, args ...);
    send(l, tag, buf.data());
  }
}

}} // message,xrt_core

#endif
