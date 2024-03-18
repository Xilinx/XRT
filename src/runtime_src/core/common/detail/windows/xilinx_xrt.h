// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/debug.h"
#include "core/common/dlfcn.h"

#pragma warning(disable : 4005)
#include <windows.h>
#include <ntstatus.h>

#if defined(XRT_WINDOWS_HAS_WDK)
# include <d3dkmthk.h>
#endif

#include <filesystem>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>

#if defined(XRT_WINDOWS_HAS_WDK)
namespace xrt_core::detail::windows {

// D3DKMTQueryAdapterInfo returns path rooted in \SystemRoot\, but no
// other API understands this path, so replace it with the actual
// system root.
static std::string
replace_systemroot(std::string str)
{
  auto pos = str.find("\\SystemRoot\\");
  if (pos != 0)
    return str;

  char system_root[MAX_PATH] = {};
  if (GetWindowsDirectoryA(system_root, sizeof(system_root)) == 0)
    throw std::runtime_error("Unable to get Windows directory");
  
  str.replace(pos, std::strlen("\\SystemRoot"), system_root);
  return str;
}

// Windows APIs return wchar_t strings, but we want to use std::string.
// Convert to utf8 multibyte string.
static std::string
utf8(const std::wstring& wstr)
{
  auto size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
  std::string str(size, 0);
  auto result = WideCharToMultiByte(
     CP_UTF8,      // CodePage
     0,            // dwFlags conversion type must be 0 for CP_UTF8
     wstr.data(),  // lpWideCharStr
     -1,           // cchWideChar -1 indicates null terminated string
     str.data(),   // lpMultiByteStr output buffer
     size,         // cbMultiByte size of output buffer
     NULL,         // lpDefaultChar, NULL implies system default 
     NULL);        // lpUsedDefaultChar must be NULL for CP_UTF8

  // strip included null terminator from std::string
  while (str.back() == '\0')
    str.pop_back();

  if (result == 0)
    throw std::runtime_error("Unable to convert wide string to multi-byte");

  return str;
}

// Manage gdi dll loading and symbol lookup
class gdilib
{
  using dll_guard = std::unique_ptr<void, decltype(&xrt_core::dlclose)>;
  dll_guard dll;

public:
  gdilib(const char* dllnm)
    : dll(xrt_core::dlopen(dllnm, 0), xrt_core::dlclose)
  {}

  template <typename FunctionType>
  FunctionType
  get(const char* symbol) const
  {
    if (auto dllsym = xrt_core::dlsym(dll.get(), symbol))
      return static_cast<FunctionType>(dllsym);

    throw std::runtime_error("No such symbol '" + std::string(symbol) + "' in gdi32.dll");
  }
};

static gdilib gdi("gdi32.dll");

// Abstraction for adapter opened by D3DKMTEnumAdapters3.  Move semantics for
// storing in container, while making sure all enumerated adapters are eventually 
// closed using D3DKMTCloseAdapter
struct adapter
{
  D3DKMT_ADAPTERINFO m_info = {};

  adapter() = default;

  ~adapter()
  {
    if (!m_info.hAdapter)
      return;

    D3DKMT_CLOSEADAPTER close_adapter_args = {};
    close_adapter_args.hAdapter = m_info.hAdapter;
    D3DKMTCloseAdapter(&close_adapter_args);
  }

  // Movable
  adapter(adapter&& rhs)
    : m_info(std::move(rhs.m_info))
  {
    // Avoid double close
    rhs.m_info.hAdapter = 0;
  }

  // Not copyable
  adapter(const adapter&) = delete;
  adapter& operator=(const adapter&) = delete;

  D3DKMT_HANDLE
  handle() const
  {
    return m_info.hAdapter;
  }

  // Query the driver store path for this adapter
  std::string
  driver_store_path() const
  {
    auto adapter_info = gdi.get<PFND3DKMT_QUERYADAPTERINFO>("D3DKMTQueryAdapterInfo");

    D3DKMT_QUERYADAPTERINFO query_adapter_info = {};
    D3DDDI_QUERYREGISTRY_INFO query_registry_info = {};
    query_adapter_info.hAdapter = handle();
    query_adapter_info.Type = KMTQAITYPE_QUERYREGISTRY;
    query_adapter_info.pPrivateDriverData = &query_registry_info;
    query_adapter_info.PrivateDriverDataSize = sizeof(query_registry_info);
    query_registry_info.QueryType = D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH;
    auto status = adapter_info(&query_adapter_info);
    if (status != STATUS_SUCCESS)
      throw std::runtime_error("D3DKMTQueryAdapterInfo failed KMTQAITYPE_QUERYREGISTRY");

    if (query_registry_info.Status != D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW)
      throw std::runtime_error("Unexpected D3DDDI_QUERYREGISTRY_STATUS");

    // Save the size of the output value
    // Valid only when Status == D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW
    auto output_value_size = query_registry_info.OutputValueSize; 

    // Allocate variable sized query registey info buffer and query again
    std::vector<char> buffer(sizeof(D3DDDI_QUERYREGISTRY_INFO) + output_value_size);
    std::memcpy(buffer.data(), &query_registry_info, sizeof(D3DDDI_QUERYREGISTRY_INFO));
    query_adapter_info.pPrivateDriverData = buffer.data();
    query_adapter_info.PrivateDriverDataSize = static_cast<UINT>(buffer.size());
    status = adapter_info(&query_adapter_info);
    if (status != STATUS_SUCCESS)
      throw std::runtime_error("D3DKMTQueryAdapterInfo failed KMTQAITYPE_QUERYREGISTRY");

    // Interpret the data, throw on error
    auto query_info = reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO*>(buffer.data());
    if (query_info->Status != D3DDDI_QUERYREGISTRY_STATUS_SUCCESS)
      throw std::runtime_error("D3DDDI_QUERYREGISTRY_STATUS_SUCCESS failed");

    // Return the driver path.
    // Account for OutputString being WCHAR[], whereas OutputValueSize is bytes
    std::wstring wstr{query_info->OutputString, query_info->OutputString + output_value_size / sizeof(wchar_t)};
    return replace_systemroot(utf8(wstr));
  }
};

// Abstraction to manage list of enumerated adapters
struct adapter_list
{
  std::vector<adapter> m_adapters;

  adapter_list()
  {
    // Determine size of adapter list
    D3DKMT_ENUMADAPTERS3 enum_adapters_args = {};
    enum_adapters_args.Filter.IncludeComputeOnly = 1;
    auto enum_adapters = gdi.get<PFND3DKMT_ENUMADAPTERS3>("D3DKMTEnumAdapters3");
    auto status = enum_adapters(&enum_adapters_args);
    if (status != STATUS_SUCCESS)
      throw std::runtime_error("D3DKMTEnumAdapters3 failed ");

    m_adapters.resize(enum_adapters_args.NumAdapters);

    // Enumerate adapters
    enum_adapters_args.pAdapters = reinterpret_cast<D3DKMT_ADAPTERINFO*>(m_adapters.data());
    status = enum_adapters(&enum_adapters_args);
    if (status != STATUS_SUCCESS)
      throw std::runtime_error("D3DKMTEnumAdapters3 failed");
    m_adapters.resize(enum_adapters_args.NumAdapters);
  }

  // Return first matching adapter
  const adapter*
  find(const std::string& match)
  {
    const std::wstring driver_description{match.begin(), match.end()};
    auto adapter_info = gdi.get<PFND3DKMT_QUERYADAPTERINFO>("D3DKMTQueryAdapterInfo");
    for (const auto& adapter : m_adapters) {
      D3DKMT_QUERYADAPTERINFO query_adapter_info_args = {};
      D3DKMT_DRIVER_DESCRIPTION query_driver_description = {};
      query_adapter_info_args.hAdapter = adapter.handle();
      query_adapter_info_args.Type = KMTQAITYPE_DRIVER_DESCRIPTION;
      query_adapter_info_args.pPrivateDriverData = &query_driver_description;
      query_adapter_info_args.PrivateDriverDataSize = sizeof(query_driver_description);
      auto status = adapter_info(&query_adapter_info_args);
      if (status != STATUS_SUCCESS)
        throw std::runtime_error("D3DKMTQueryAdapterInfo failed KMTQAITYPE_DRIVER_DESCRIPTION");
      
      if (std::wstring(query_driver_description.DriverDescription) == driver_description)
        return &adapter;
    }

    return nullptr;
  }
};

} // xrt_core::detail::windows
#endif // XRT_WINDOWS_HAS_WDK

namespace xrt_core::detail {

namespace sfs = std::filesystem;

sfs::path
xilinx_xrt()
{
#if defined(XRT_WINDOWS_HAS_WDK)
  // For wdf make sure to continue loading from same location as coreutil
  windows::adapter_list adapters;

  // Tight coupling with KMD driver description string
  auto adapter = adapters.find("NPU Compute Accelerator Device");

  // If no matching adapter found, return coreutil path (legacy)
  if (!adapter)
    return sfs::path{xrt_core::dlpath("xrt_coreutil.dll")}.parent_path();

  return adapter->driver_store_path();
#else
  // Without WDK we can't query the driver store path, so return coreutil path
  return sfs::path{xrt_core::dlpath("xrt_coreutil.dll")}.parent_path();
#endif
}

std::vector<sfs::path>
platform_repo_path()
{
  // For time being, platform repo is same as xilinx_xrt
  return {xilinx_xrt()};
}

} // xrt_core::detail

