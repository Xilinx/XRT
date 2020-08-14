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

#ifndef core_common_util_error_h
#define core_common_util_error_h

#include "core/common/config.h"
#include <stdexcept>
#include <string>
#include <system_error>

namespace xrt_core {

class error : public std::runtime_error
{
  int m_code;
public:
  error(int ec, const std::string& what = "")
    : std::runtime_error(what), m_code(ec)
  {}

  explicit
  error(const std::string& what)
    : std::runtime_error(what), m_code(0)
  {}

  int
  get() const
  {
    return m_code;
  }

  unsigned int
  get_code() const
  {
    return get();
  }
};

class system_error : public std::system_error
{
public:
  system_error(int ec, const std::string& what = "")
    : std::system_error(ec, std::system_category(), what)
  {}
};

XRT_CORE_COMMON_EXPORT
void
send_exception_message(const char* msg, const char* tag="XRT");

XRT_CORE_COMMON_EXPORT
void
send_exception_message(const std::string& msg, const char* tag="XRT");

} // xrt_core

#endif
