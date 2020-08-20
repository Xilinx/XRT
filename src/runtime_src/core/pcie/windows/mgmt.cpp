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
#define XRT_CORE_PCIE_WINDOWS_SOURCE
#include "mgmt.h"
#include "xclfeatures.h"
#include "core/common/message.h"

#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <initguid.h>

// To be simplified
#include "core/pcie/driver/windows/include/XoclMgmt_INTF.h"

#include <limits>
#include <cassert>
#include <regex>

#pragma warning(disable : 4100 4996)
#pragma comment (lib, "Setupapi.lib")

namespace { // private implementation details

/*
 * wordcopy()
 *
 * Copy bytes word (32bit) by word.
 * Neither memcpy, nor std::copy work as they become byte copying
 * on some platforms.
 */
inline void* wordcopy(void *dst, const void* src, size_t bytes)
{
    // assert dest is 4 byte aligned
    assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

    using word = uint32_t;
    auto d = reinterpret_cast<word*>(dst);
    auto s = reinterpret_cast<const word*>(src);
    auto w = bytes/sizeof(word);

    for (size_t i=0; i<w; ++i)
        d[i] = s[i];

    return dst;
}

static bool is_admin()
{
  HANDLE m_hdl = nullptr;
  TOKEN_ELEVATION elevation;
  DWORD dwSize;

  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &m_hdl))
    throw std::runtime_error("Failed to get Process Token : " + GetLastError());
  
  if (!GetTokenInformation(m_hdl, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
	CloseHandle(m_hdl);
    throw std::runtime_error("Failed to get Token Information : " + GetLastError());
  }
  
  return elevation.TokenIsElevated;
}

struct mgmt
{
  unsigned int m_idx = std::numeric_limits<unsigned int>::max();
  HANDLE m_hdl = nullptr;
  char* bar_address = nullptr;

  // create mgmt object, open the device, store the device handle
  mgmt(unsigned int devidx) : m_idx(devidx)
  {
    GUID guid = GUID_XILINX_PF_INTERFACE;

    auto device_info = SetupDiGetClassDevs(&guid, NULL,  NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

    SP_DEVICE_INTERFACE_DATA device_interface;
    device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    if (!SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, m_idx, &device_interface))
      throw std::runtime_error("No such card " + std::to_string(m_idx));

    ULONG size = 0;
    if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &size, NULL)
        && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      throw std::runtime_error("SetupDiGetDeviceInterfaceDetail - get length failed");

    auto device_detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(size));
    if (!device_detail)
      throw std::runtime_error("Cannot allocate device detail, out of memory");
    device_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, device_detail, size, NULL, NULL)) {
      free(device_detail);
      throw std::runtime_error("SetupDiGetDeviceInterfaceDetail - get detail failed");
    }

    m_hdl = CreateFile(device_detail->DevicePath,
                       GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL,
                       OPEN_EXISTING,
                       0,
                       NULL);

    free(device_detail);

    if (m_hdl == INVALID_HANDLE_VALUE)
      throw std::runtime_error("CreateFile failed with error " + std::to_string(GetLastError()));

    // map the first bar
    DWORD bytes = 0;
    auto status = DeviceIoControl
      (m_hdl,
       XCLMGMT_OID_GET_BAR_ADDR,
       NULL,
       0,
       &bar_address,
       sizeof(void*),
       &bytes,
       NULL);

    if (!status || bytes != sizeof(void*)) {
        CloseHandle(m_hdl);
        throw std::runtime_error("Could not map BAR");
    }

  }

  // destruct mgmt object, close the device
  ~mgmt()
  {
    // close the device
    CloseHandle(m_hdl);
  }

  void
  read_bar(uint64_t offset, void* buf, uint64_t len)
  {
    wordcopy(buf, bar_address + offset, len);
  }

  void
  write_bar(uint64_t offset, const void* buf, uint64_t len)
  {
    wordcopy(bar_address + offset, buf, len);
  }

  void
  get_device_info(XCLMGMT_IOC_DEVICE_INFO* value)
  {
    DWORD bytes = 0;
    auto status = DeviceIoControl(
        m_hdl,
        XCLMGMT_OID_GET_IOC_DEVICE_INFO,
        value,
        sizeof(XCLMGMT_IOC_DEVICE_INFO),
        value,
        sizeof(XCLMGMT_IOC_DEVICE_INFO),
        &bytes,
        NULL);

    if (!status)// || bytes != sizeof(XCLMGMT_IOC_DEVICE_INFO))
      throw std::runtime_error("DeviceIoControl XCLMGMT_OID_DEVICE_INFO failed");
  }

  void
  get_rom_info(FeatureRomHeader* value)
  {
    XCLMGMT_IOC_DEVICE_INFO device_info;
    get_device_info(&device_info);

    std::memcpy(value, &device_info.rom_hdr, sizeof(FeatureRomHeader));
  }

  void
  get_bdf_info(uint16_t bdf[3])
  {
    // TODO: code share with shim
    GUID guid = GUID_XILINX_PF_INTERFACE;
    auto hdevinfo = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    SP_DEVINFO_DATA dev_info_data;
    dev_info_data.cbSize = sizeof(dev_info_data);
    DWORD size;
    SetupDiEnumDeviceInfo(hdevinfo, m_idx, &dev_info_data);
    SetupDiGetDeviceRegistryProperty(hdevinfo, &dev_info_data, SPDRP_LOCATION_INFORMATION,
                                     nullptr, nullptr, 0, &size);
    std::string buf(static_cast<size_t>(size), 0);
    SetupDiGetDeviceRegistryProperty(hdevinfo, &dev_info_data, SPDRP_LOCATION_INFORMATION,
                                     nullptr, (PBYTE)buf.data(), size, nullptr);

    std::regex regex("\\D+(\\d+)\\D+(\\d+)\\D+(\\d+)");
    std::smatch match;
    if (std::regex_search(buf, match, regex))
      std::transform(match.begin() + 1, match.end(), bdf,
                     [](const auto& m) {
                       return static_cast<uint16_t>(std::stoi(m.str()));
                     });
  }

  void
  get_flash_addr(uint64_t& value)
  {
    DWORD bytes = 0;
    auto status = DeviceIoControl
		(m_hdl,
		XCLMGMT_OID_GET_QSPI_INFO,
		NULL,
		0,
		&value,
		sizeof(uint64_t),
		&bytes,
		NULL);

    if (!status)
      throw std::runtime_error("DeviceIoControl XCLMGMT_OID_GET_QSPI_INFO failed");
  }

}; // struct mgmt

mgmt*
get_mgmt_object(xclDeviceHandle handle)
{
  // TODO: Do some sanity check
  return reinterpret_cast<mgmt*>(handle);
}
}

namespace mgmtpf {

unsigned int
probe()
{
  GUID guid = GUID_XILINX_PF_INTERFACE;

  HDEVINFO device_info =
    SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (device_info == INVALID_HANDLE_VALUE)
    throw std::runtime_error("GetDevices INVALID_HANDLE_VALUE");

  SP_DEVICE_INTERFACE_DATA device_interface;
  device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  // Determine how many devices are present
  int count = 0;
  while (SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, count++, &device_interface)) {}

  // Compensate for last failing call
  if (--count == 0)
    throw std::runtime_error("No Xilinx U250 devices are present in the system");

  // Initialize each device
  for (int idx = 0; idx < count; ++idx) {
    if (!SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, idx, &device_interface))
      throw std::runtime_error("Unexpected error");

    // get required buffer size
    ULONG size = 0;
    if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &size, NULL)
        && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      throw std::runtime_error("SetupDiGetDeviceInterfaceDetail - get length failed");

    // allocate space for device interface detail
    auto dev_detail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size));
    if (!dev_detail)
      throw std::runtime_error("HeapAlloc failed");
    dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    // get device interface detail
    if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, dev_detail, size, NULL, NULL))
      throw std::runtime_error("SetupDiGetDeviceInterfaceDetail - get detail failed");

    HeapFree(GetProcessHeap(), 0, dev_detail);
  }

  SetupDiDestroyDeviceInfoList(device_info);

  return count;
}

xclDeviceHandle
open(unsigned int device_index)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "mgmt::open()");
  try {
    return new mgmt(device_index);
  }
  catch (const std::exception& ex) {
    xrt_core::message::
      send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "mgmt::open() failed with `%s`", ex.what());
	if(!is_admin())
		xrt_core::message::
		send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "Administrative privileges required");
    return nullptr;
  }
}

void
close(xclDeviceHandle hdl)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "mgmt::close()");
  auto mgmt = get_mgmt_object(hdl);
  delete mgmt;
}

void
read_bar(xclDeviceHandle hdl, uint64_t addr, void* buf, uint64_t len)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "mgmt::read_bar()");
  auto mgmt = get_mgmt_object(hdl);
  mgmt->read_bar(addr, buf, len);
}

void
write_bar(xclDeviceHandle hdl, uint64_t addr, const void* buf, uint64_t len)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "write_bar()");
  auto mgmt = get_mgmt_object(hdl);
  mgmt->write_bar(addr, buf, len);
}

void
get_device_info(xclDeviceHandle hdl, XCLMGMT_IOC_DEVICE_INFO* value)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_device_info()");
  auto mgmt = get_mgmt_object(hdl);
  mgmt->get_device_info(value);
}

void
get_rom_info(xclDeviceHandle hdl, FeatureRomHeader* value)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_rom_info()");
  auto mgmt = get_mgmt_object(hdl);
  mgmt->get_rom_info(value);
}

void
get_bdf_info(xclDeviceHandle hdl, uint16_t bdf[3])
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_bdf_info()");
  auto mgmt = get_mgmt_object(hdl);
  mgmt->get_bdf_info(bdf);
}

void
get_flash_addr(xclDeviceHandle hdl, uint64_t& addr)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_flash_addr()");
  auto mgmt = get_mgmt_object(hdl);
  mgmt->get_flash_addr(addr);
}

} // mgmt
