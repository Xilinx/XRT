// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// Local - Include files
#include "core/common/error.h"

// System - Include Files
#include <windows.h>
#include <string>
#include <thread>

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

static std::string
get_bios_version() {
  struct SMBIOSData {
    uint8_t  Used20CallingMethod;
    uint8_t  SMBIOSMajorVersion;
    uint8_t  SMBIOSMinorVersion;
    uint8_t  DmiRevision;
    uint32_t  Length;
    uint8_t  SMBIOSTableData[1];
  };

  DWORD bios_size = GetSystemFirmwareTable('RSMB', 0, NULL, 0);
  if (bios_size > 0) {
    std::vector<char> bios_vector(bios_size);
    auto bios_data = reinterpret_cast<SMBIOSData*>(bios_vector.data());
    // Retrieve the SMBIOS table
    GetSystemFirmwareTable('RSMB', 0, bios_data, bios_size);
    return std::to_string(bios_data->SMBIOSMajorVersion) + "." + std::to_string(bios_data->SMBIOSMinorVersion);
  }
  return "unknown";
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
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "BuildLab", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("release", value);
  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentVersion", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("version", value);

  pt.put("machine", getmachinename());

  BufferSize = sizeof value;
  RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("distribution", value);

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
  RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System", "SystemBiosVersion", RRF_RT_ANY, NULL, (PVOID)&value, &BufferSize);
  pt.put("bios_vendor", value);
  pt.put("bios_version", get_bios_version());
}

} //xrt_core::sysinfo
