/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file implements XRT error APIs as declared in
// core/include/experimental/xrt_ini.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_ini.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/xrt/experimental/xrt_ini.h"

#include "core/common/config_reader.h"
#include "core/common/error.h"

namespace xrt::ini {

void
set(const std::string& key, const std::string& value)
{
  xrt_core::config::detail::set(key, value);
}

} // namespace xrt::ini

////////////////////////////////////////////////////////////////
// xrt_ini C API implmentations (xrt_ini.h)
////////////////////////////////////////////////////////////////
int
xrtIniStringSet(const char* key, const char* value)
{
  try {
    xrt::ini::set(key, value);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

int
xrtIniUintSet(const char* key, unsigned int value)
{
  try {
    xrt::ini::set(key, std::to_string(value));
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}
