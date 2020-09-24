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

// This file is delivered with core library (libxrt_core), see
// core/pcie/windows/CMakeLists.txt.  To prevent compilation of this
// file from importing symbols from libxrt_core we define this source
// file to instead export with same macro as used in libxrt_core.
#define XCL_DRIVER_DLL_EXPORT
#define XRT_CORE_PCIE_WINDOWS_SOURCE
#include "system_windows.h"
#include "device_windows.h"
#include "gen/version.h"
#include "mgmt.h"
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <ctime>
#include <windows.h>

#include <boost/format.hpp>

#ifdef _WIN32
# pragma warning (disable : 4996)
#endif

namespace {

// Singleton registers with base class xrt_core::system
// during static global initialization
static xrt_core::system_windows singleton;

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

}

namespace xrt_core {

system*
system_child_ctor()
{
  static system_windows sw;
  return &sw;
}
void
system_windows::
get_xrt_info(boost::property_tree::ptree& /*pt*/)
{
  //TODO
  // _pt.put("xocl",      driver_version("xocl"));
  // _pt.put("xclmgmt",   driver_version("xclmgmt"));
}

void
system_windows::
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

  MEMORYSTATUSEX mem;
  mem.dwLength = sizeof(mem);
  GlobalMemoryStatusEx(&mem);
  pt.put("memory_bytes", (boost::format("0x%llx") % mem.ullTotalPhys).str());

  pt.put("cores", std::thread::hardware_concurrency());
}

std::pair<device::id_type, device::id_type>
system_windows::
get_total_devices(bool is_user) const
{
  unsigned int count = is_user ? xclProbe() : mgmtpf::probe();
  return std::make_pair(count, count);
}

void
system_windows::
scan_devices(bool verbose, bool json) const
{
  std::cout << "TO-DO: scan_devices\n";
  verbose = verbose;
  json = json;
}

std::shared_ptr<device>
system_windows::
get_userpf_device(device::id_type id) const
{
  return xrt_core::get_userpf_device(xclOpen(id, nullptr, XCL_QUIET));
}

std::shared_ptr<device>
system_windows::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_windows>(new device_windows(handle, id, true));
}

std::shared_ptr<device>
system_windows::
get_mgmtpf_device(device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_windows>(new device_windows(mgmtpf::open(id), id, false));
}

void
system_windows::
program_plp(std::shared_ptr<device> dev, const std::vector<char> &buffer) const
{
  mgmtpf::plp_program(dev->get_mgmt_handle(), reinterpret_cast<const axlf*>(buffer.data()));

  // asynchronously check if the download is complete
  std::this_thread::sleep_for(std::chrono::seconds(5));
  const static int program_timeout_sec = 15;
  uint64_t plp_status = RP_DOWNLOAD_IN_PROGRESS;
  int retry_count = 0;
  while (retry_count < program_timeout_sec) {
    mgmtpf::plp_program_status(dev->get_mgmt_handle(), plp_status);
	retry_count++;

    // check plp status
    if(plp_status == RP_DOWLOAD_SUCCESS)
      break;
    else if (plp_status == RP_DOWLOAD_FAILED)
      throw xrt_core::error("PLP programmming failed");

    if (retry_count == program_timeout_sec)
      throw xrt_core::error("PLP programmming timed out");

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

} // xrt_core
