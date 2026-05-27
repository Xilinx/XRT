// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

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

class message_dispatch
{
public:
  virtual ~message_dispatch() = default;
  static message_dispatch* make_dispatcher(const std::string& choice);
  virtual void send(severity_level l, const char* tag, const char* msg) = 0;
};

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
