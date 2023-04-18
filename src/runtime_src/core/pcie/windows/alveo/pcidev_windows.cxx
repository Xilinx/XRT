// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidev_windows.h"
#include "mgmt.h"

#include <chrono>
#include <ctime>
#include <map>
#include <memory>
#include <regex>
#include <thread>
#include <setupapi.h>
#include <windows.h>

namespace xrt_core { namespace pci {
	
  std::shared_ptr<device> 
  pcidev_windows::create_device(device::handle_type handle, device::id_type id) const
  {
	  return (!handle) ? std::shared_ptr<xrt_core::device_windows>(new xrt_core::device_windows(mgmtpf::open(id), id, false)) : //mgmtpf
                       std::shared_ptr<xrt_core::device_windows>(new xrt_core::device_windows(handle, id, true)); //userpf
  }

  device::handle_type
  pcidev_windows::create_shim(device::id_type id) const
  {
	  return xclOpen(id, nullptr, XCL_QUIET);
  }

  std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  pcidev_windows::get_bdf_info(device::id_type id, bool is_user) const
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

} } // namespace xrt_core :: pci