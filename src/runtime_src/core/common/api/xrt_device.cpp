/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_device.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_device.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common

#include "core/include/experimental/xrt_device.h"
#include "core/include/experimental/xrt_aie.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "xclbin_int.h" // Non public xclbin APIs

#include <map>
#include <vector>
#include <fstream>

#ifdef _WIN32
# pragma warning( disable : 4244 )
#endif

namespace {

// C-API handles that must be explicitly closed. Corresponding managed
// handles are inserted in this map.  When the unmanaged handle is
// closed, it is removed from this map and underlying buffer is
// deleted if no other shared ptrs exists for this buffer
static std::map<xrtDeviceHandle, std::shared_ptr<xrt_core::device>> device_cache;

static std::shared_ptr<xrt_core::device>
get_device(xrtDeviceHandle dhdl)
{
  auto itr = device_cache.find(dhdl);
  if (itr == device_cache.end())
    throw xrt_core::error(-EINVAL, "No such device handle");
  return (*itr).second;
}


static void
free_device(xrtDeviceHandle dhdl)
{
  if (device_cache.erase(dhdl) == 0)
    throw xrt_core::error(-EINVAL, "No such device handle");
}

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
}

} // unnamed namespace

namespace xrt_core { namespace device_int {

std::shared_ptr<xrt_core::device>
get_core_device(xrtDeviceHandle dhdl)
{
  return get_device(dhdl); // handle check
}

xclDeviceHandle
get_xcl_device_handle(xrtDeviceHandle dhdl)
{
  auto device = get_device(dhdl); // handle check
  return device->get_device_handle();  // shim handle
}

}} // device_int, xrt_core


namespace xrt {

////////////////////////////////////////////////////////////////
// xrt_bo C++ API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////

device::
device(unsigned int index)
  : handle(xrt_core::get_userpf_device(index))
{}

device::
device(xclDeviceHandle dhdl)
  : handle(xrt_core::get_userpf_device(dhdl))
{}

uuid
device::
load_xclbin(const struct axlf* top)
{
  xrt::xclbin xclbin{top};
  handle->load_xclbin(xclbin);
  return xclbin.get_uuid();
}

uuid
device::
load_xclbin(const std::string& fnm)
{
  xrt::xclbin xclbin{fnm};
  handle->load_xclbin(xclbin);
  return xclbin.get_uuid();
}

uuid
device::
load_xclbin(const xclbin& xclbin)
{
  handle->load_xclbin(xclbin);
  return xclbin.get_uuid();
}

uuid
device::
get_xclbin_uuid() const
{
  return handle->get_xclbin_uuid();
}

device::
operator xclDeviceHandle() const
{
  return handle->get_device_handle();
}

std::pair<const char*, size_t>
device::
get_xclbin_section(axlf_section_kind section, const uuid& uuid) const
{
  return handle->get_axlf_section_or_error(section, uuid);
}

} // xrt

#ifdef XRT_ENABLE_AIE
////////////////////////////////////////////////////////////////
// xrt_aie_device C++ API implmentations (xrt_aie.h)
////////////////////////////////////////////////////////////////
namespace xrt { namespace aie {

void
device::
reset_array()
{
  auto handle = get_handle();
  handle->reset_aie();
}

}} // namespace aie, xrt
#endif

////////////////////////////////////////////////////////////////
// xrt_device API implmentations (xrt_device.h)
////////////////////////////////////////////////////////////////
xrtDeviceHandle
xrtDeviceOpen(unsigned int index)
{
  try {
    auto device = xrt_core::get_userpf_device(index);
    device_cache[device.get()] = device;
    return device.get();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;
}

int
xrtDeviceClose(xrtDeviceHandle dhdl)
{
  try {
    free_device(dhdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return (errno = ex.get());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return (errno = 0);
  }
}

int
xrtDeviceLoadXclbin(xrtDeviceHandle dhdl, const axlf* top)
{
  try {
    xrt::xclbin xclbin{top};
    auto device = get_device(dhdl);
    device->load_xclbin(xclbin);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return (errno = ex.get());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return (errno = 0);
  }
}

int
xrtDeviceLoadXclbinFile(xrtDeviceHandle dhdl, const char* fnm)
{
  try {
    xrt::xclbin xclbin{fnm};
    auto device = get_device(dhdl);
    device->load_xclbin(xclbin);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return (errno = ex.get());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return (errno = 0);
  }
}

int
xrtDeviceLoadXclbinHandle(xrtDeviceHandle dhdl, xrtXclbinHandle xhdl)
{
  try {
    auto device = get_device(dhdl);
    device->load_xclbin(xrt_core::xclbin_int::get_xclbin(xhdl));
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return (errno = ex.get());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return (errno = 0);
  }
}

int
xrtDeviceGetXclbinUUID(xrtDeviceHandle dhdl, xuid_t out)
{
  try {
    auto device = get_device(dhdl);
    auto uuid = device->get_xclbin_uuid();
    uuid_copy(out, uuid.get());
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return 1;
  }
}

xclDeviceHandle
xrtDeviceToXclDevice(xrtDeviceHandle dhdl)
{
  try {
    auto device = get_device(dhdl);
    return device->get_device_handle();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;
}

xrtDeviceHandle
xrtDeviceOpenFromXcl(xclDeviceHandle dhdl)
{
  try {
    auto device = xrt_core::get_userpf_device(dhdl);

    // Only one xrt unmanaged device per xclDeviceHandle
    // xrtDeviceClose removes the handle from the cache
    if (device_cache.count(device.get()))
      throw xrt_core::error(-EINVAL, "Handle is already in use");
    device_cache[device.get()] = device;
    return device.get();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    errno = 0;
  }
  return nullptr;
}
