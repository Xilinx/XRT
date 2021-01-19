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

// class system_error - Propagation of OS system errors
//
// Use system_error for system (OS) propagation of errors.  Convert
// shim level OS errors into a system_error
class system_error : public std::system_error
{
public:
  system_error(int ec, const std::error_category& cat, const std::string& what = "")
    : std::system_error(std::abs(ec), cat, what)
  {}
  
  explicit
  system_error(int ec, const std::string& what = "")
    : system_error(ec, std::system_category(), what)
  {}

  // Retrive underlying code for return plain error code
  int
  value() const
  {
    return code().value();
  }

  int
  get() const
  {
    return value();
  }

  int
  get_code() const
  {
    return value();
  }
};

// class generic_error - Propagation of generic errors
//
// Use generic_error for propagation of error codes originating
// in user space.   Error codes are expected to POSIX error
// codes.
class generic_error : public system_error
{
public:
  explicit
  generic_error(int ec, const std::string& what = "")
    : system_error(std::abs(ec), std::generic_category(), what)
  {}
};

// User space error with POSIX error code
// Same as generic_error except default EINVAL ctor from message
class error : public generic_error
{
public:
  explicit
  error(int ec, const std::string& what = "")
    : generic_error(std::abs(ec), what)
  {}

  explicit
  error(const std::string& what)
    : generic_error(EINVAL, what)
  {}
};

// Internal unexpected error
using internal_error = std::runtime_error;
 

XRT_CORE_COMMON_EXPORT
void
send_exception_message(const char* msg, const char* tag="XRT");

XRT_CORE_COMMON_EXPORT
void
send_exception_message(const std::string& msg, const char* tag="XRT");

} // xrt_core

#endif
