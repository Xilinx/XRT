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

#ifndef core_common_dlopen_h_
#define core_common_dlopen_h_

#ifndef (_WIN32)
# include <dlfcn.h>
#else
# include <windows.h>
#endif

namespace xrt_core {

#ifndef (_WIN32)
inline void*
dlopen(const char* dllname, int flags)
{
  return ::dlopen(dllname,flags);
}

inline void
dlclose(void* handle)
{
  ::dlclose(handle);
}

#else
inline void*
dlopen(const char* dllname, int)
{
  return ::LoadLibrary(dllname);
}

inline void
dlclose(void* handle)
{
  ::CloseHandle(handle);
}
#endif

} // xrt_core

#endif
