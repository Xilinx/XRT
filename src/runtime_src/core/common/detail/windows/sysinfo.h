// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// Local - Include files
#include "core/common/error.h"

// System - Include Files
#include <windows.h>
#include <string>
#include <locale>
#include <codecvt>
#include <thread>
#include <comdef.h>
#include <wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>
#include <boost/format.hpp>

#ifdef _WIN32
# pragma warning (disable : 4996)
#endif

namespace {

static std::string
getmachinename()
{
  std::string machine;
  SYSTEM_INFO sysInfo;

  // Get hardware info
  ZeroMemory(&sysInfo, sizeof(SYSTEM_INFO));
  GetSystemInfo(&sysInfo);
  // Set processor architecture
  switch (sysInfo.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    machine = "x86_64";
    break;
  case PROCESSOR_ARCHITECTURE_IA64:
    machine = "ia64";
    break;
  case PROCESSOR_ARCHITECTURE_INTEL:
    machine = "x86";
    break;
  case PROCESSOR_ARCHITECTURE_UNKNOWN:
  default:
    machine = "unknown";
    break;
  }

  return machine;
}

static std::string
getmachinedistribution()
{
  HRESULT hres;

  // Initialize COM
  hres = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hres))
    throw xrt_core::error("Failed to initialize COM library. Cannot get machine distribution information");

  // Set COM security levels
  hres = CoInitializeSecurity(
        NULL, 
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities
        NULL                         // Reserved
        );
  if (FAILED(hres)) {
    CoUninitialize();
    throw xrt_core::error("Failed to initialize security. Cannot get machine distribution information");
  }

  // Obtain initial locator to WMI
  IWbemLocator *pWbemLocator = NULL;
  hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pWbemLocator);
  if (FAILED(hres)) {
    CoUninitialize();
    throw xrt_core::error("Failed to obtain locator. Cannot get machine distribution information");
  }

  // Connect to WMI
  IWbemServices *pWbemServices = NULL;
  hres = pWbemLocator->ConnectServer(
         _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
         NULL,                    // User name, NULL indicated current in these args
         NULL,                    // User password
         0,                       // Locale
         NULL,                    // Security flags
         0,                       // Authority
         NULL,                    // Context object
         &pWbemServices           // pointer to IWbemServices proxy
         );
  if (FAILED(hres)) {
    pWbemLocator->Release();
    CoUninitialize();
    throw xrt_core::error("Failed to connect to WMI. Cannot get machine distribution information");
  }

  // Make WMI Request
  IEnumWbemClassObject* pEnum = NULL;
  hres = pWbemServices->ExecQuery(bstr_t("WQL"), bstr_t(L"Select Caption from Win32_OperatingSystem"), WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum);
  if (FAILED(hres)) {
    pWbemServices->Release();
    pWbemLocator->Release();
    CoUninitialize();
    throw xrt_core::error("WMI Query failed. Cannot get machine distribution information");
  }
  ULONG uObjectCount = 0;
  IWbemClassObject *pWmiObject = NULL;
  hres = pEnum->Next(WBEM_INFINITE, 1, &pWmiObject, &uObjectCount);
  if (FAILED(hres)) {
    pWbemServices->Release();
    pWbemLocator->Release();
    pEnum->Release();
    CoUninitialize();
    throw xrt_core::error("WMI Query failed. Cannot get machine distribution information");
  }
  VARIANT cvtDistribution;
  VariantInit(&cvtDistribution);
  hres = pWmiObject->Get(L"Caption", 0, &cvtDistribution, 0, 0);
  if (FAILED(hres)) {
    VariantClear(&cvtDistribution);
    pWmiObject->Release();
    pWbemServices->Release();
    pWbemLocator->Release();
    pEnum->Release();
    CoUninitialize();
    throw xrt_core::error("WMI Query failed. Cannot get machine distribution information");
  }
  _bstr_t bstrValue(cvtDistribution.bstrVal);
  std::wstring wstrValue(bstrValue);
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  std::string distribution = converter.to_bytes(wstrValue);

  // Cleanup
  VariantClear(&cvtDistribution);
  pWmiObject->Release();
  pWbemServices->Release();
  pWbemLocator->Release();
  pEnum->Release();
  CoUninitialize();
  return distribution;
}

static std::string
osNameImpl()
{
  OSVERSIONINFO vi;
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (GetVersionEx(&vi) == 0)
    throw xrt_core::error("Cannot get OS version information");
  switch (vi.dwPlatformId)
  {
  case VER_PLATFORM_WIN32s:
      return "Windows 3.x";
  case VER_PLATFORM_WIN32_WINDOWS:
      return vi.dwMinorVersion == 0 ? "Windows 95" : "Windows 98";
  case VER_PLATFORM_WIN32_NT:
      return "Windows NT";
  default:
      return "Unknown";
  }
}

} //end anonymous namespace

namespace xrt_core::sysinfo::detail {

void
get_os_info(boost::property_tree::ptree &pt)
{
  char value[128];
  DWORD BufferSize = sizeof value;

  pt.put("sysname", osNameImpl());
  //Reassign buffer size since it get override with size of value by RegGetValueA() call
  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("release", value);

  pt.put("machine", getmachinename());

  pt.put("distribution", getmachinedistribution());

  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\SystemInformation", "SystemProductName", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("model", value);

  
  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName", "ComputerName", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("hostname", value);

  MEMORYSTATUSEX mem;
  mem.dwLength = sizeof(mem);
  GlobalMemoryStatusEx(&mem);
  pt.put("memory_bytes", (boost::format("0x%llx") % mem.ullTotalPhys).str());

  pt.put("cores", std::thread::hardware_concurrency());

  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "BIOSVendor", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("bios_vendor", value);
  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "BIOSVersion", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("bios_version", value);
}

} //xrt_core::sysinfo
