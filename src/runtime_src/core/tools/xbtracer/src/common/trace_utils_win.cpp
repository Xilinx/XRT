// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifdef _WIN32

#include <cerrno>
#include <cstdlib>
#include <common/trace_utils.h>
#include "core/common/windows/win_utils.h"
#include <windows.h>
#include <tlhelp32.h>

int
setenv_os(const char* name, const char* val)
{
  if (SetEnvironmentVariable(name, val))
    return 0;
  return -EINVAL;
}

int
getenv_os(const char* name, char *buf, size_t len)
{
  DWORD rlen = GetEnvironmentVariable(name, buf, static_cast<DWORD>(len));
  if (rlen > (DWORD)len) {
    buf[0] = 0;
    return -EINVAL;
  }
  return static_cast<int>(rlen);
}

int
localtime_os(std::tm& tm, const std::time_t& t)
{
  return static_cast<int>(localtime_s(&tm, &t));
}

uint32_t
getpid_current_os()
{
  DWORD pid = GetCurrentProcessId();
  return static_cast<uint32_t>(pid);
}

int
inject_library(HANDLE hprocess, const char* lib_path)
{
  // Get the address of LoadLibraryA in kernel32.dll
  HMODULE hkernel32 = GetModuleHandle("kernel32.dll");
  if (!hkernel32)
    xbtracer_pcritical("inject \"", std::string(lib_path),
                       "\" failed, failed to get handle to kernel32.dll.");

  FARPROC load_lib_addr = GetProcAddress(hkernel32, "LoadLibraryA");
  if (!load_lib_addr)
    xbtracer_pcritical("inject \"", std::string(lib_path),
                       "\" failed, failed to get address of LoadLibraryA.");

  // Allocate memory in the target process for the library path
  void* remote_mem = VirtualAllocEx(hprocess, nullptr, strlen(lib_path) + 1,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remote_mem)
    xbtracer_pcritical("inject \"", std::string(lib_path),
                       "\" failed, failed to allocate memory in target process.");

  // Write the library path to the allocated memory
  if (!WriteProcessMemory(hprocess, remote_mem, lib_path, strlen(lib_path) + 1, nullptr)) {
    VirtualFreeEx(hprocess, remote_mem, 0, MEM_RELEASE);
    xbtracer_pcritical("inject \"", std::string(lib_path),
                        "\" failed, failed to write library path to target process memory.");
  }

  // Create a remote thread in the target process to load the library
  HANDLE hthread = CreateRemoteThread(hprocess, nullptr, 0, (LPTHREAD_START_ROUTINE)load_lib_addr,
                                      remote_mem, 0, nullptr);
  if (!hthread) {
    VirtualFreeEx(hprocess, remote_mem, 0, MEM_RELEASE);
    xbtracer_pcritical("inject \"", std::string(lib_path),
                       "\" failed, failed to create remote thread in target process,",
                       sys_dep_get_last_err_msg(), ".");
  }

  // Wait for the remote thread to finish
  WaitForSingleObject(hthread, INFINITE);

  // Clean up
  CloseHandle(hthread);
  VirtualFreeEx(hprocess, remote_mem, 0, MEM_RELEASE);

  return 0;
}

lib_handle_type
load_library_os(const char* path)
{
  return LoadLibraryA(path);
}

void
close_library_os(lib_handle_type handle)
{
  if (handle)
    FreeLibrary(handle);
}

proc_addr_type
get_proc_addr_os(lib_handle_type handle, const char* symbol)
{
  HMODULE hmodule = static_cast<HMODULE>(handle);
  FARPROC paddr = GetProcAddress(hmodule, symbol);

  if (!paddr) {
    xbtracer_perror("failed to get address of symbol \"", symbol, "\".");
    return nullptr;
  }
  return paddr;
}

#endif // _WIN32
