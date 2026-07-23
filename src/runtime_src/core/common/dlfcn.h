// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc. All rights reserved.
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_dlopen_h_
#define core_common_dlopen_h_

#ifndef _WIN32
# include <dlfcn.h>
#else
# include <filesystem>
# include <stdexcept>
# include <string>
# include <windows.h>
# ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#  define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
# endif
# define RTLD_LAZY 0
# define RTLD_GLOBAL 0
# define RTLD_NOW 0
#endif

namespace xrt_core {

#ifndef _WIN32
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

inline const char*
dlerror()
{
  return ::dlerror();
}

inline void*
dlsym(void* handle, const char* symbol)
{
  return ::dlsym(handle,symbol);
}
#endif

#ifdef _WIN32
// dlopen(const char* dllname, int) -
// We follow Microsoft DLL search-order guidance for all runtime
// loads; separately, SDK plugin discovery via environment variable is
// an explicit trust boundary and is not protected by that guidance.
// Specifically, the environment can define XILINX_XRT or
// AMD_NPU_SDK_PATH, both of which are considered user trusted
// boundaries from where DLLs can be loaded if needed.
inline void*
dlopen(const char* dllname, int)
{
  namespace sfs = std::filesystem;

  if (!dllname || !*dllname)
    throw std::runtime_error("Empty DLL path");

  sfs::path path{dllname};
  uint32_t flags = 0;
  if (path.has_parent_path() && !path.parent_path().empty()) {
    if (!path.is_absolute())
      path = sfs::absolute(path);

    // Make sure any dependencies are satisfied first from same
    // directory as the dll being loaded.  Even when the main DLL
    // comes from a trusted absolute path, a missing or ambiguous
    // dependency could otherwise be satisfied from CWD. This flag
    // makes “same directory as the plugin” win first.
    flags = LOAD_WITH_ALTERED_SEARCH_PATH;
  }
  else {
    // Bare filename: restrict search to System32 (never CWD or PATH).
    flags = LOAD_LIBRARY_SEARCH_SYSTEM32;
  }

  return ::LoadLibraryExA(path.string().c_str(), nullptr, flags);
}

inline void
dlclose(void* handle)
{
  ::FreeLibrary(HMODULE(handle));
}

inline const char*
dlerror()
{
  return "";
}

inline void*
dlsym(void* handle, const char* symbol)
{
  return ::GetProcAddress(HMODULE(handle), symbol);
}

inline std::string
dlpath(const char* dllname)
{
  char dll_path[MAX_PATH];
  if (!::GetModuleFileName(::GetModuleHandle(dllname), dll_path, MAX_PATH))
    throw std::runtime_error("Get handle of " + std::string(dllname ? dllname : "") + " failed");

  return dll_path;
}

#endif

} // xrt_core

#endif
