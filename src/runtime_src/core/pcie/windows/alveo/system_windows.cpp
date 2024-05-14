// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// This file is delivered with core library (libxrt_core), see
// core/pcie/windows/CMakeLists.txt.  To prevent compilation of this
// file from importing symbols from libxrt_core we define this source
// file to instead export with same macro as used in libxrt_core.
#define XCL_DRIVER_DLL_EXPORT
#define XRT_CORE_PCIE_WINDOWS_SOURCE

// Local - Include files
#include "system_windows.h"
#include "core/pcie/driver/windows/alveo/include/XoclUser_INTF.h"
#include "device_windows.h"
#include "mgmt.h"

// System - Include files
#include <chrono>
#include <ctime>
#include <map>
#include <memory>
#include <regex>
#include <thread>
#include <setupapi.h>
#include <windows.h>

#ifdef _WIN32
# pragma warning (disable : 4996)
#endif

#pragma comment (lib, "Setupapi.lib")

// Singleton registers with base class xrt_core::system
// during static global initialization
static xrt_core::system_windows singleton;

namespace xrt_core {

system*
system_child_ctor()
{
  static system_windows sw;
  return &sw;
}
void
system_windows::
get_driver_info(boost::property_tree::ptree& /*pt*/)
{
  //TODO
  // _pt.put("xocl",      driver_version("xocl"));
  // _pt.put("xclmgmt",   driver_version("xclmgmt"));
}

std::pair<device::id_type, device::id_type>
system_windows::
get_total_devices(bool is_user) const
{
  unsigned int count = is_user ? xclProbe() : mgmtpf::probe();
  return std::make_pair(count, count);
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
system_windows::
get_bdf_info(device::id_type id, bool is_user) const
{
  GUID guid = GUID_DEVINTERFACE_XOCL_USER;
  auto hdevinfo = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  SP_DEVINFO_DATA dev_info_data;
  dev_info_data.cbSize = sizeof(dev_info_data);
  DWORD size;
  SetupDiEnumDeviceInfo(hdevinfo, id, &dev_info_data);
  SetupDiGetDeviceRegistryProperty(hdevinfo, &dev_info_data, SPDRP_LOCATION_INFORMATION,
                                    nullptr, nullptr, 0, &size);
  std::string buf(static_cast<size_t>(size), 0);
  SetupDiGetDeviceRegistryProperty(hdevinfo, &dev_info_data, SPDRP_LOCATION_INFORMATION,
                                    nullptr, (PBYTE)buf.data(), size, nullptr);

  std::regex regex("\\D+(\\d+)\\D+(\\d+)\\D+(\\d+)");
  std::smatch match;
  uint16_t bdf[4];
  if (std::regex_search(buf, match, regex))
    std::transform(match.begin() + 1, match.end(), bdf,
                    [](const auto& m) {
                      return static_cast<uint16_t>(std::stoi(m.str()));
                    });

  bdf[3] = (is_user ? 1 : 0);
  return std::make_tuple(bdf[0], bdf[1], bdf[2], bdf[3]);
}

void
system_windows::
scan_devices(bool /*verbose*/, bool /*json*/) const
{
  // TODO
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
program_plp(const device* dev, const std::vector<char> &buffer, bool force) const
{
  mgmtpf::plp_program(dev->get_mgmt_handle(), reinterpret_cast<const axlf*>(buffer.data()), force);

  // asynchronously check if the download is complete
  std::this_thread::sleep_for(std::chrono::seconds(5));
  const static int program_timeout_sec = 15;
  uint64_t plp_status = RP_DOWNLOAD_IN_PROGRESS;
  int retry_count = 0;
  while (retry_count++ < program_timeout_sec) {
    mgmtpf::plp_program_status(dev->get_mgmt_handle(), plp_status);

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
