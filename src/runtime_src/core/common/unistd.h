/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef core_common_unistd_h_
#define core_common_unistd_h_

#ifndef _WIN32
#include <unistd.h>
#else
#include <Shlobj.h>
#endif

namespace xrt_core {

inline int
getpagesize()
{
#ifndef _WIN32
  return ::getpagesize();
#else
  return 4096;
#endif
}

inline bool
is_user_privileged()
{
#ifndef _WIN32
  return (getuid() == 0) || (geteuid() == 0);
#else
  return IsUserAnAdmin();
#endif
}


} // xrt_core
#endif
