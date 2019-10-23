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

#ifndef xrtcore_message_h_
#define xrtcore_message_h_
#include "core/common/config.h"
#include "core/common/config_reader.h"
#include <string>
#include <cstdio>
#include <vector>

namespace xrt_core { namespace message {

//modeled based on syslog severity.
enum class severity_level : unsigned short
{
  XRT_EMERGENCY,
  XRT_ALERT,
  XRT_CRITICAL,
  XRT_ERROR,
  XRT_WARNING,
  XRT_NOTICE,
  XRT_INFO,
  XRT_DEBUG
};


XRT_CORE_COMMON_EXPORT
void
send(severity_level l, const char* tag, const char* msg);

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
      send(severity_level::XRT_ERROR, tag, "Illegal arguments in log format string");
      return;
    }
    
    std::vector<char> buf(sz+1);
    snprintf(buf.data(), sz, format, args ...);
    send(l, tag, buf.data());
  }
}

}} // message,xrt

#endif
