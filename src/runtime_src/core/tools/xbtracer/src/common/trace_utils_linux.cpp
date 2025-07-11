// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifdef __linux__

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>
#include <common/trace_utils.h>

int
setenv_os(const char* name, const char* val)
{
  return setenv(name, val, 1);
}

int
getenv_os(const char* name, char* buf, uint32_t len)
{
  const char *tmpstr = getenv(name);

  if (!tmpstr)
    return 0;

  size_t env_len = strlen(tmpstr);
  if ((len + 1) < env_len)
    return -EINVAL;
  strcpy(buf, tmpstr);

  return env_len;
}

int
localtime_os(std::tm& tm, const std::time_t& t)
{
  if (!localtime_r(&t, &tm))
    return -EINVAL;
  return 0;
}

uint32_t
getpid_current_os(void)
{
  pid_t pid = getpid();

  return (uint32_t)(pid & 0xFFFFFFFFU);
}

lib_handle_type
load_library_os(const char* path)
{
  return dlopen(path, RTLD_LAZY);
}

void
close_library_os(lib_handle_type handle)
{
  if (handle)
    dlclose(handle);
}

proc_addr_type
get_proc_addr_os(lib_handle_type handle, const char* symbol)
{
  void* paddr = dlsym(handle, symbol);

  if (!paddr) {
    xbtracer_perror("failed to get address of symbol \"", symbol, "\".");
    return nullptr;
  }
  return paddr;
}

#endif // __linux__
