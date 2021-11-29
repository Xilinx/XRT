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

#include "core/include/xrt/xrt_device.h"
#include "core/include/xrt/xrt_aie.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/sensor.h"
#include "core/common/info_memory.h"
#include "core/common/info_platform.h"
#include "core/common/query_requests.h"

#include "handle.h"
#include "native_profile.h"
#include "xclbin_int.h" // Non public xclbin APIs

#include <boost/property_tree/json_parser.hpp>

#include <map>
#include <vector>
#include <fstream>

#ifdef _WIN32
# pragma warning( disable : 4244 )
#endif

namespace {

// C-API handles that must be explicitly closed but corresponding
// implementation could be shared.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static xrt_core::handle_map<xrtDeviceHandle, std::shared_ptr<xrt_core::device>> device_cache;

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

// Return a json string conforming to ABI schema
// ABI remains unused until we actually have a new schema  
static std::string
json_str(const boost::property_tree::ptree& pt, const xrt::detail::abi&) 
{
  std::stringstream ss;
  boost::property_tree::write_json(ss, pt);
  return ss.str();
};

} // query

} // unnamed namespace

namespace xrt_core { namespace device_int {

std::shared_ptr<xrt_core::device>
get_core_device(xrtDeviceHandle dhdl)
{
  return device_cache.get_or_error(dhdl);
}

xclDeviceHandle
get_xcl_device_handle(xrtDeviceHandle dhdl)
{
  auto device = device_cache.get_or_error(dhdl);
  return device->get_device_handle();  // shim handle
}

}} // device_int, xrt_core


namespace xrt {

////////////////////////////////////////////////////////////////
// xrt_bo C++ API implmentations (xrt_bo.h)
////////////////////////////////////////////////////////////////

device::
device(unsigned int index)
  : handle(xdp::native::profiling_wrapper("xrt::device::device",
	   alloc_device_index, index))
{}

device::
device(const std::string& bdf)
  : device(xrt_core::get_device_id(bdf))
{}

device::
device(xclDeviceHandle dhdl)
  : handle(xdp::native::profiling_wrapper("xrt::device::device",
	   alloc_device_handle, dhdl))
{}

uuid
device::
load_xclbin(const struct axlf* top)
{
  return xdp::native::profiling_wrapper("xrt::device::load_xclbin", [this, top]{
    xrt::xclbin xclbin{top};
    handle->load_xclbin(xclbin);
    return xclbin.get_uuid();
  });
}

uuid
device::
load_xclbin(const std::string& fnm)
{
  return xdp::native::profiling_wrapper("xrt::device::load_xclbin", [this, &fnm]{
    xrt::xclbin xclbin{fnm};
    handle->load_xclbin(xclbin);
    return xclbin.get_uuid();
  });
}

uuid
device::
load_xclbin(const xclbin& xclbin)
{
  return xdp::native::profiling_wrapper("xrt::device::load_xclbin",
  [this, &xclbin]{
    handle->load_xclbin(xclbin);
    return xclbin.get_uuid();
  });
}

uuid
device::
get_xclbin_uuid() const
{
  return xdp::native::profiling_wrapper("xrt::device::get_xclbin_uuid", [this]{
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
  return xdp::native::profiling_wrapper("xrt::device::reset", [this]{
    handle.reset();
  });
}

std::pair<const char*, size_t>
device::
get_xclbin_section(axlf_section_kind section, const uuid& uuid) const
{
  return xdp::native::profiling_wrapper("xrt::device::get_xclbin_section",
    [this, section, &uuid]{
      return handle->get_axlf_section_or_error(section, uuid);
    });
}

boost::any
device::
get_info(info::device param, const xrt::detail::abi& abi) const
{
  switch (param) {
  case info::device::bdf :                    // std::string
    return query::to_string<info::device::bdf, xrt_core::query::pcie_bdf>(handle.get());
  case info::device::interface_uuid :         // std::string
    return query::to_value<info::device::interface_uuid, xrt_core::query::interface_uuids>
      (handle.get(), [](const auto& iids) {
        return (iids.size() == 1)
          ? xrt_core::query::interface_uuids::to_uuid(iids[0])
          : uuid {};
      });
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
  case info::device::name :                   // std::string
    return query::raw<info::device::name, xrt_core::query::rom_vbnv>(handle.get());
  case info::device::nodma :                  // bool
    return query::to_value<info::device::nodma, xrt_core::query::nodma>
      (handle.get(), [](const auto& val) { return bool(val); });
  case info::device::offline :
    return query::raw<info::device::offline, xrt_core::query::is_offline>(handle.get());
  case info::device::electrical :            // std::string
    return query::json_str(xrt_core::sensor::read_electrical(handle.get()), abi);
  case info::device::thermal :               // std::string
    return query::json_str(xrt_core::sensor::read_thermals(handle.get()), abi);
  case info::device::mechanical :            // std::string
    return query::json_str(xrt_core::sensor::read_mechanical(handle.get()), abi);
  case info::device::memory :                // std::string
    return query::json_str(xrt_core::memory::memory_topology(handle.get()), abi);
  case info::device::platform :              // std::string
    return query::json_str(xrt_core::platform::platform_info(handle.get()), abi);
  case info::device::pcie_info :                  // std::string
    return query::json_str(xrt_core::platform::pcie_info(handle.get()), abi);
  case info::device::dynamic_regions :         // std::string
    return query::json_str(xrt_core::memory::xclbin_info(handle.get()), abi);
  case info::device::host :                   // std::string
    boost::property_tree::ptree pt;
    xrt_core::get_xrt_build_info(pt);
    return query::json_str(pt, abi);
  }

  throw std::runtime_error("internal error: unreachable");
}

// Deprecated but left for support of old existing binaries in the
// field that reference this symbol. Unused since xrt-2.12.x
boost::any
device::
get_info(info::device param) const
{
  // Old binaries call get_info without ABI and by
  // default will use current ABI version
  return get_info(param, xrt::detail::abi{});
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
  auto core_device = get_handle();
  core_device->reset_aie();
}

void
device::
open_context(xrt::aie::device::access_mode am)
{
  auto core_device = get_handle();
  core_device->open_aie_context(am);
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
    return xdp::native::profiling_wrapper(__func__, [index]{
      auto device = xrt_core::get_userpf_device(index);
      auto handle = device.get();
      device_cache.add(handle, std::move(device));
      return handle;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
    return xdp::native::profiling_wrapper(__func__, [bdf]{
      return xrtDeviceOpen(xrt_core::get_device_id(bdf));
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
    return xdp::native::profiling_wrapper(__func__, [dhdl]{
      device_cache.remove_or_error(dhdl);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtDeviceLoadXclbin(xrtDeviceHandle dhdl, const axlf* top)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, top]{
      xrt::xclbin xclbin{top};
      auto device = device_cache.get_or_error(dhdl);
      device->load_xclbin(xclbin);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtDeviceLoadXclbinFile(xrtDeviceHandle dhdl, const char* fnm)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, fnm]{
      xrt::xclbin xclbin{fnm};
      auto device = device_cache.get_or_error(dhdl);
      device->load_xclbin(xclbin);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtDeviceLoadXclbinHandle(xrtDeviceHandle dhdl, xrtXclbinHandle xhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, xhdl]{
      auto device = device_cache.get_or_error(dhdl);
      device->load_xclbin(xrt_core::xclbin_int::get_xclbin(xhdl));
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtDeviceLoadXclbinUUID(xrtDeviceHandle dhdl, const xuid_t uuid)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, uuid]{
      auto device = device_cache.get_or_error(dhdl);
      device->load_xclbin(uuid);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtDeviceGetXclbinUUID(xrtDeviceHandle dhdl, xuid_t out)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl, out]{
      auto device = device_cache.get_or_error(dhdl);
      auto uuid = device->get_xclbin_uuid();
      uuid_copy(out, uuid.get());
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

xclDeviceHandle
xrtDeviceToXclDevice(xrtDeviceHandle dhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [dhdl]{
      auto device = device_cache.get_or_error(dhdl);
      return device->get_device_handle();
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
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
    return xdp::native::profiling_wrapper(__func__, [dhdl]{
      auto device = xrt_core::get_userpf_device(dhdl);

      // Only one xrt unmanaged device per xclDeviceHandle
      // xrtDeviceClose removes the handle from the cache
      if (device_cache.count(device.get()))
        throw xrt_core::error(EINVAL, "Handle is already in use");

      auto handle = device.get();
      device_cache.add(handle, std::move(device));
      return handle;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return nullptr;
}
