// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// Local - Include files
#include "core/common/error.h"

// System - Include Files
#include <windows.h>
#include <string>
#include <thread>
#include <array>

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
  std::array<char, 128> value{}; //NOLINT(cppcoreguidelines-avoid-magic-numbers)
  DWORD BufferSize = static_cast<DWORD>(value.size());
  
  LONG result = RegGetValueA(
    HKEY_LOCAL_MACHINE,
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
    "ProductName",
    RRF_RT_ANY,
    NULL,
    value.data(),
    &BufferSize
  );
  
  std::string productName(value.data());
  
  // Windows 11 detection: Check if build number >= 22000
  BufferSize = static_cast<DWORD>(value.size());
  result = RegGetValueA(
    HKEY_LOCAL_MACHINE,
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
    "CurrentBuildNumber",
    RRF_RT_ANY,
    NULL,
    value.data(),
    &BufferSize
  );
  
  if (result == ERROR_SUCCESS) {
    int buildNumber = std::stoi(std::string(value.data()));
    // Windows 11 starts at build 22000
    // https://learn.microsoft.com/en-us/answers/questions/586619/windows-11-build-ver-is-still-10-0-22000-194
    if (buildNumber >= 22000 && productName.find("Windows 10") != std::string::npos) { //NOLINT(cppcoreguidelines-avoid-magic-numbers)
      size_t pos = productName.find("Windows 10");
      productName.replace(pos, 10, "Windows 11"); //NOLINT(cppcoreguidelines-avoid-magic-numbers)
    }
  }
  
  return productName;
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

  DWORD minor = 0;
  DWORD minorSize = sizeof(minor);
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "UBR", RRF_RT_REG_DWORD, nullptr, &minor, &minorSize);

  //major.minor
  std::string version(value);
  version+= "." + std::to_string(minor);

  pt.put("release", version);

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

  //processor name
  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("processor", value);
}

bool
is_advanced()
{
  DWORD value = 0;
  DWORD valueSize = sizeof(value);
  DWORD valueType;
  LONG result = RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\ControlSet001\\Services\\IpuMcdmDriver", "XRTSMIAdvanced", RRF_RT_REG_DWORD, &valueType, &value, &valueSize);
  if ((result == ERROR_SUCCESS) && (valueType == REG_DWORD) && (value == 1))
    return true;

  result = RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\ControlSet001\\Services\\Npu2McdmDriver", "XRTSMIAdvanced", RRF_RT_REG_DWORD, &valueType, &value, &valueSize);
  if ((result == ERROR_SUCCESS) && (valueType == REG_DWORD) && (value == 1))
    return true;

  return false;
}

} //xrt_core::sysinfo
