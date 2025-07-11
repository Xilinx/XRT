// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifdef _WIN32
#include <cstring>
#include <tuple>
#include <vector>
#include "core/common/windows/win_utils.h"
#include "wrapper/tracer.h"
#include "common/trace_utils.h"
#include <windows.h>
#include <detours.h>
#include <psapi.h>

constexpr const char* wrapper_lib_name = "xrt_wrapper.dll";
constexpr const char* xrt_coreutil_name = XBRACER_XRT_COREUTIL_LIB;

// vector to store function hooking information
// format of each element is <"mangled_name", wrapper_func_addr, original_func_addr>
static std::vector<std::tuple<const char*, PVOID, PVOID>> hook_funcs_map;

static
void
store_hook_funcs()
{
  HMODULE wrapper_dll_h = GetModuleHandleA(wrapper_lib_name);
  if (!wrapper_dll_h)
    xbtracer_pcritical("failed to get handle of \"", std::string(wrapper_lib_name), "\",",
                       sys_dep_get_last_err_msg(), ".");
  HMODULE xrt_dll_h = GetModuleHandleA(xrt_coreutil_name);
  if (!xrt_dll_h)
    xbtracer_pcritical("failed to get handle of \"", std::string(xrt_coreutil_name), "\",",
                       sys_dep_get_last_err_msg(), ".");
  for (uint32_t i = 1; i < get_size_of_func_mangled_map(); i += 2) {
    const char* func_s = func_mangled_map[i - 1];
    const char* mangled_name = func_mangled_map[i];
    FARPROC paddr_o = GetProcAddress(xrt_dll_h, mangled_name);
    FARPROC paddr_w = GetProcAddress(wrapper_dll_h, mangled_name);
    if (paddr_o) {
      if (!paddr_w) {
        // TODO: debug message for now, as we haven't implement all APIs
        xbtracer_pdebug("\"", std::string(wrapper_lib_name), "\" doesn't have \"",
                       std::string(func_s), "\"; ", std::string(mangled_name), ".");
      }
      else {
        std::tuple<const char*, PVOID, PVOID> fmap(mangled_name, reinterpret_cast<PVOID>(paddr_w),
                                                   reinterpret_cast<PVOID>(paddr_o));
        hook_funcs_map.push_back(std::move(fmap));
      }
    }
    else {
      xbtracer_pdebug("\"", std::string(xrt_coreutil_name), "\" doesn't have \"",
                       std::string(func_s), "\"; ", std::string(mangled_name), ".");
    }
  }
}

typedef BOOL(WINAPI* CreateProcessA_t)(LPCSTR lpApplicationName,
                                       LPSTR lpCommandLine,
                                       LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                       LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                       BOOL bInheritHandles,
                                       DWORD dwCreationFlags,
                                       LPVOID lpEnvironment,
                                       LPCSTR lpCurrentDirectory,
                                       LPSTARTUPINFOA lpStartupInfo,
                                       LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL(WINAPI* CreateProcessW_t)(LPCWSTR lpApplicationName,
                                       LPWSTR lpCommandLine,
                                       LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                       LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                       BOOL bInheritHandles,
                                       DWORD dwCreationFlags,
                                       LPVOID lpEnvironment,
                                       LPCWSTR lpCurrentDirectory,
                                       LPSTARTUPINFOW lpStartupInfo,
                                       LPPROCESS_INFORMATION lpProcessInformation);

static
BOOL
WINAPI
HookCreateProcessA(
    LPCSTR lpApplicationName,
    LPSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCSTR lpCurrentDirectory,
    LPSTARTUPINFOA lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
) {
  xbtracer_pdebug(__func__, " called with command line: ", lpCommandLine, ".");
  // Call the original CreateProcess function
  CreateProcessA_t oCreateProcessA = (CreateProcessA_t)xbtracer_get_original_func_addr("CreateProcessA");
  BOOL ret = DetourCreateProcessWithDllA(
      lpApplicationName,
      lpCommandLine,
      lpProcessAttributes,
      lpThreadAttributes,
      bInheritHandles,
      dwCreationFlags,
      lpEnvironment,
      lpCurrentDirectory,
      lpStartupInfo,
      lpProcessInformation,
      "xrt_wrapper.dll",
      oCreateProcessA);
  if (!ret)
    xbtracer_pcritical("failed to call the original CreateProcssA for: ", lpCommandLine,
                       ", ", sys_dep_get_last_err_msg(), ".");
  return ret;
}

static
BOOL
WINAPI
HookCreateProcessW(
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
) {
  xbtracer_pdebug(__func__);
  std::wcout << lpApplicationName << std::endl;
  std::wcout << lpCommandLine << std::endl;
  // Call the original CreateProcess function
  CreateProcessW_t oCreateProcessW = (CreateProcessW_t)xbtracer_get_original_func_addr("CreateProcessW");
  BOOL ret = DetourCreateProcessWithDllW(
      lpApplicationName,
      lpCommandLine,
      lpProcessAttributes,
      lpThreadAttributes,
      bInheritHandles,
      dwCreationFlags,
      lpEnvironment,
      lpCurrentDirectory,
      lpStartupInfo,
      lpProcessInformation,
      "xrt_wrapper.dll",
      oCreateProcessW);
  if (!ret)
    xbtracer_pcritical("failed to call the original CreateProcssW,", sys_dep_get_last_err_msg(), ".");
  return ret;
}

static
void
store_hook_win_funcs()
{
  std::map<const char*, PVOID> func_map;
  func_map["CreateProcessA"] = reinterpret_cast<PVOID>(&HookCreateProcessA);
  func_map["CreateProcessW"] = reinterpret_cast<PVOID>(&HookCreateProcessW);
  const char* win_dll_name = "kernel32.dll";
  for (const auto& pair : func_map) {
    const char* func_name = pair.first;
    FARPROC paddr_o = GetProcAddress(GetModuleHandleA(win_dll_name), func_name);
    if (!paddr_o)
      xbtracer_pcritical("failed to get ", func_name, " address, ", sys_dep_get_last_err_msg(), ".");
    std::tuple<const char*, PVOID, PVOID> fmap(func_name, pair.second,
                                             reinterpret_cast<PVOID>(paddr_o));
    hook_funcs_map.push_back(std::move(fmap));
    xbtracer_pdebug("Hooked win API: ", func_name, ".");
  }
}

static
int
detour_attach_xrt_funcs()
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  for (auto &fmap : hook_funcs_map) {
    const char* mangled_name = std::get<0>(fmap);
    PVOID paddr_w = std::get<1>(fmap);
    PVOID& paddr_o_r = std::get<2>(fmap);

    LONG ret = DetourAttach(&paddr_o_r, paddr_w);
    if (ret != NO_ERROR) {
      xbtracer_pcritical("failed to setup detour for \"", std::string(mangled_name), "\", ",
                         ret, ".");
    }
    xbtracer_pdebug("attach detour for \"", std::string(mangled_name), "\".");
  }
  DetourTransactionCommit();

  return 0;
}

static
int
detour_detach_xrt_funcs()
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  for (auto &fmap : hook_funcs_map) {
    const char* mangled_name = std::get<0>(fmap);
    PVOID paddr_w = std::get<1>(fmap);
    PVOID& paddr_o_r = std::get<2>(fmap);

    LONG ret = DetourDetach(&paddr_o_r, paddr_w);
    if (ret != NO_ERROR)
      xbtracer_pcritical("failed to detach detour for \"", std::string(mangled_name), "\", ",
                         ret, ".");
    xbtracer_pdebug("detach detour for \"", std::string(mangled_name), "\".");
  }
  DetourTransactionCommit();

  return 0;
}

BOOL APIENTRY
DllMain(HMODULE hmodule, DWORD  ul_reason_for_call, LPVOID lp_reserved)
{
  (void)hmodule;
  (void)lp_reserved;
  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    xbtracer_pdebug("attaching \"", std::string(wrapper_lib_name), "\".");
    store_hook_funcs();
    store_hook_win_funcs();
    detour_attach_xrt_funcs();
  } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
    xbtracer_pdebug("detaching \"", std::string(wrapper_lib_name), "\".");
    // Restore the original CreateProcess function
    detour_detach_xrt_funcs();
  }
  return TRUE;
}

proc_addr_type
xbtracer_get_original_func_addr(const char* symbol)
{
  for (auto &fmap : hook_funcs_map) {
    const char* mangled_name = std::get<0>(fmap);

    if (!strcmp(symbol, mangled_name)) {
      PVOID paddr_o_r = std::get<2>(fmap);
      return reinterpret_cast<proc_addr_type>(paddr_o_r);
    }
  }
  return nullptr;
}

#endif // _WIN32
