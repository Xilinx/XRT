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
#include "core/common/query_requests.h"

#include "xclbin_int.h" // Non public xclbin APIs
#include "native_profile.h"

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

// Necessary to disambiguate the call for profiling_wrapper
static std::shared_ptr<xrt_core::device>
alloc_device_index(unsigned int index)
{
  return xrt_core::get_userpf_device(index) ;
}

static std::shared_ptr<xrt_core::device>
alloc_device_handle(xclDeviceHandle dhdl)
{
  return xrt_core::get_userpf_device(dhdl) ;
}

// Helper functions for extracting xrt_core::device query request values
namespace query {

// Return the raw value of a query_request.  Compile type validation
// that the query request result type matches the xrt::info::device
// return type
template <xrt::info::device param, typename QueryRequestType>
boost::any
raw(const xrt_core::device* device)
{
  static_assert(std::is_same<
                typename QueryRequestType::result_type,
                typename xrt::info::param_traits<xrt::info::device, param>::return_type
                >::value, "query type mismatch");
  return device->query<QueryRequestType>();
}

// Return the converted query request.  Conversion is per converter
// argument Compile type validation that the query request result type
// matches the xrt::info::device return type
template <xrt::info::device param, typename QueryRequestType, typename Converter>
boost::any
to_value(const xrt_core::device* device, Converter conv)
{
  auto val = conv(xrt_core::device_query<QueryRequestType>(device));
  static_assert(std::is_same<
                decltype(val),
                typename xrt::info::param_traits<xrt::info::device, param>::return_type
                >::value, "query type mismatch");
  return val;
}

// Return the query request as a std::string using the
// query_request::to_string converter. Compile time asserts that
// xrt::info::device param return type is std::string
template <xrt::info::device param, typename QueryRequestType>
boost::any
to_string(const xrt_core::device* device)
{
  return to_value<param, QueryRequestType>(device, [](const auto& q) { return QueryRequestType::to_string(q); });
}

} // query

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
  : handle(xdp::native::profiling_wrapper(__func__, "xrt::device",
	   alloc_device_index, index))
{}

device::
device(const std::string& bdf)
  : device(xrt_core::get_device_id(bdf))
{}

device::
device(xclDeviceHandle dhdl)
  : handle(xdp::native::profiling_wrapper(__func__, "xrt::device",
	   alloc_device_handle, dhdl))
{}

uuid
device::
load_xclbin(const struct axlf* top)
{
  return xdp::native::profiling_wrapper(__func__, "xrt::device", [this, top]{
    xrt::xclbin xclbin{top};
    handle->load_xclbin(xclbin);
    return xclbin.get_uuid();
  });
}

uuid
device::
load_xclbin(const std::string& fnm)
{
  return xdp::native::profiling_wrapper(__func__, "xrt::device", [this, &fnm]{
    xrt::xclbin xclbin{fnm};
    handle->load_xclbin(xclbin);
    return xclbin.get_uuid();
  });
}

uuid
device::
load_xclbin(const xclbin& xclbin)
{
  return xdp::native::profiling_wrapper(__func__, "xrt::device",
  [this, &xclbin]{
    handle->load_xclbin(xclbin);
    return xclbin.get_uuid();
  });
}

uuid
device::
get_xclbin_uuid() const
{
  return xdp::native::profiling_wrapper(__func__, "xrt::device", [this]{
    return handle->get_xclbin_uuid();
  });
}

device::
operator xclDeviceHandle() const
{
  return handle->get_device_handle();
}

void
device::
reset()
{
  return xdp::native::profiling_wrapper(__func__, "xrt::device", [this]{
    handle.reset();
  });
}

std::pair<const char*, size_t>
device::
get_xclbin_section(axlf_section_kind section, const uuid& uuid) const
{
  return xdp::native::profiling_wrapper(__func__, "xrt::device",
    [this, section, &uuid]{
      return handle->get_axlf_section_or_error(section, uuid);
    });
}

boost::any
device::
get_info(info::device param) const
{
  switch (param) {
  case info::device::name :                   // std::string
    return query::raw<info::device::name, xrt_core::query::rom_vbnv>(handle.get());
  case info::device::bdf :                    // std::string
    return query::to_string<info::device::bdf, xrt_core::query::pcie_bdf>(handle.get());
  case info::device::kdma :                   // uint32_t
    return query::raw<info::device::kdma, xrt_core::query::kds_numcdmas>(handle.get());
  case info::device::max_clock_frequency_mhz: // unsigned long
    return query::to_value<info::device::max_clock_frequency_mhz, xrt_core::query::clock_freqs_mhz>
      (handle.get(), [](const auto& freqs) {
        unsigned long max = 0;
        for (const auto& val : freqs) { max = std::max(max, std::stoul(val)); }
        return max;
      });
  case info::device::m2m :                    // bool
    try {
      return query::to_value<info::device::m2m, xrt_core::query::m2m>
        (handle.get(), [](const auto& val) { return bool(val); });
    }
    catch (const std::exception&) {
      return false;
    }
  case info::device::nodma :                  // bool
    return query::to_value<info::device::nodma, xrt_core::query::nodma>
      (handle.get(), [](const auto& val) { return bool(val); });
  }

  throw std::runtime_error("internal error: unreachable");
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
    return xdp::native::profiling_wrapper(__func__, nullptr, [index]{
      auto device = xrt_core::get_userpf_device(index);
      device_cache[device.get()] = device;
      return device.get();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

xrtDeviceHandle
xrtDeviceOpenByBDF(const char* bdf)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [bdf]{
      return xrtDeviceOpen(xrt_core::get_device_id(bdf));
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

int
xrtDeviceClose(xrtDeviceHandle dhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl]{
      free_device(dhdl);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtDeviceLoadXclbin(xrtDeviceHandle dhdl, const axlf* top)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl, top]{
      xrt::xclbin xclbin{top};
      auto device = get_device(dhdl);
      device->load_xclbin(xclbin);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtDeviceLoadXclbinFile(xrtDeviceHandle dhdl, const char* fnm)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl, fnm]{
      xrt::xclbin xclbin{fnm};
      auto device = get_device(dhdl);
      device->load_xclbin(xclbin);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtDeviceLoadXclbinHandle(xrtDeviceHandle dhdl, xrtXclbinHandle xhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl, xhdl]{
      auto device = get_device(dhdl);
      device->load_xclbin(xrt_core::xclbin_int::get_xclbin(xhdl));
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtDeviceLoadXclbinUUID(xrtDeviceHandle dhdl, const xuid_t uuid)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl, uuid]{
      auto device = get_device(dhdl);
      device->load_xclbin(uuid);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtDeviceGetXclbinUUID(xrtDeviceHandle dhdl, xuid_t out)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl, out]{
      auto device = get_device(dhdl);
      auto uuid = device->get_xclbin_uuid();
      uuid_copy(out, uuid.get());
      return 0;
    });
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
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl]{
      auto device = get_device(dhdl);
      return device->get_device_handle();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}

xrtDeviceHandle
xrtDeviceOpenFromXcl(xclDeviceHandle dhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, nullptr, [dhdl]{
      auto device = xrt_core::get_userpf_device(dhdl);

      // Only one xrt unmanaged device per xclDeviceHandle
      // xrtDeviceClose removes the handle from the cache
      if (device_cache.count(device.get()))
        throw xrt_core::error(EINVAL, "Handle is already in use");
      device_cache[device.get()] = device;
      return device.get();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}
