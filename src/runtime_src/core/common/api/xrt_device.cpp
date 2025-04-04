// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_device.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_device.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#define XRT_API_SOURCE         // in same dll as coreutil
#include "core/include/xrt/xrt_device.h"
#include "core/include/xrt/xrt_aie.h"

#include "core/common/device.h"
#include "core/common/info_aie.h"
#include "core/common/info_memory.h"
#include "core/common/info_platform.h"
#include "core/common/info_vmr.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/sensor.h"
#include "core/common/system.h"
#include "core/common/trace.h"
#include "core/common/sysinfo.h"

#include "device_int.h"
#include "handle.h"
#include "hw_queue.h"
#include "native_profile.h"
#include "xclbin_int.h"

#include <boost/property_tree/json_parser.hpp>

#include <fstream>
#include <map>
#include <vector>

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
  XRT_TRACE_POINT_SCOPE(xrt_device_alloc_index);
  auto dev = xrt_core::get_userpf_device(index);

  dev->get_usage_logger()->log_device_info(dev.get());

  return dev;
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
static auto
raw(const xrt_core::device* device)
{
  static_assert(std::is_same<
                typename QueryRequestType::result_type,
                typename xrt::info::param_traits<xrt::info::device, param>::return_type
                >::value, "query type mismatch");
  return xrt_core::device_query<QueryRequestType>(device);
}

// Return the converted query request.  Conversion is per converter
// argument Compile type validation that the query request result type
// matches the xrt::info::device return type
template <xrt::info::device param, typename QueryRequestType, typename Converter>
static auto
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
static std::string
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

// The implementation of get_info is templated to support
// deprecated boost::any return type as well as std::any
template <typename ReturnType>
static ReturnType
get_info(const xrt_core::device* device, xrt::info::device param, const xrt::detail::abi& abi)
{
  switch (param) {
  case xrt::info::device::bdf : // std::string
    return to_string<xrt::info::device::bdf, xrt_core::query::pcie_bdf>(device);
  case xrt::info::device::interface_uuid : // std::string
    return to_value<xrt::info::device::interface_uuid, xrt_core::query::interface_uuids>
      (device, [](const auto& iids) {
        return (iids.size() == 1)
          ? xrt_core::query::interface_uuids::to_uuid(iids[0])
          : xrt::uuid {};
      });
  case xrt::info::device::kdma : // uint32_t
    try {
      return raw<xrt::info::device::kdma, xrt_core::query::kds_numcdmas>(device);
    }
    catch (const std::exception&) {
      return 0;
    }
  case xrt::info::device::max_clock_frequency_mhz: // unsigned long
    return to_value<xrt::info::device::max_clock_frequency_mhz, xrt_core::query::clock_freqs_mhz>
      (device, [](const auto& freqs) {
        unsigned long max = 0;
        for (const auto& val : freqs) { max = std::max(max, std::stoul(val)); }
        return max;
      });
  case xrt::info::device::m2m : // bool
    try {
      return to_value<xrt::info::device::m2m, xrt_core::query::m2m>
        (device, [](const auto& val) { return bool(val); });
    }
    catch (const std::exception&) {
      return false;
    }
  case xrt::info::device::name : // std::string
    return raw<xrt::info::device::name, xrt_core::query::rom_vbnv>(device);
  case xrt::info::device::nodma : // bool
    return to_value<xrt::info::device::nodma, xrt_core::query::nodma>
      (device, [](const auto& val) { return bool(val); });
  case xrt::info::device::offline :
    return raw<xrt::info::device::offline, xrt_core::query::is_offline>(device);
  case xrt::info::device::electrical : // std::string
    return json_str(xrt_core::sensor::read_electrical(device), abi);
  case xrt::info::device::thermal : // std::string
    return json_str(xrt_core::sensor::read_thermals(device), abi);
  case xrt::info::device::mechanical : // std::string
    return json_str(xrt_core::sensor::read_mechanical(device), abi);
  case xrt::info::device::memory : // std::string
    return json_str(xrt_core::memory::memory_topology(device), abi);
  case xrt::info::device::platform : // std::string
    return json_str(xrt_core::platform::platform_info(device), abi);
  case xrt::info::device::pcie_info : // std::string
    return json_str(xrt_core::platform::pcie_info(device), abi);
  case xrt::info::device::dynamic_regions : // std::string
    return json_str(xrt_core::memory::dynamic_regions(device), abi);
  case xrt::info::device::vmr : //std::string
    return json_str(xrt_core::vmr::vmr_info(device), abi);
  case xrt::info::device::aie : // std::string
    return json_str(xrt_core::aie::aie_core(device), abi);
  case xrt::info::device::aie_shim : // std::string
    return json_str(xrt_core::aie::aie_shim(device), abi);
  case xrt::info::device::aie_mem : // std::string
    return json_str(xrt_core::aie::aie_mem(device), abi);
  case xrt::info::device::host : // std::string
    boost::property_tree::ptree pt;
    xrt_core::sysinfo::get_xrt_info(pt);
    return json_str(pt, abi);
  }

  throw std::runtime_error("internal error: unreachable");
}

} // query

} // unnamed namespace

namespace xrt {

// Implementation of base device exception class
class device::error_impl
{
public:
  std::string m_message;

  explicit
  error_impl(std::string message)
    : m_message(std::move(message))
  {}
};

} // xrt

namespace xrt_core::device_int {

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

std::cv_status
exec_wait(const xrt::device& device, const std::chrono::milliseconds& timeout_ms)
{
  return xrt_core::hw_queue::exec_wait(device.get_handle().get(), timeout_ms);
}

} // xrt_core::device_int

namespace xrt {

////////////////////////////////////////////////////////////////
// xrt_device C++ API implmentations (xrt_device.h)
////////////////////////////////////////////////////////////////
device::error::
error(const std::string& message)
  : detail::pimpl<device::error_impl>(std::make_shared<device::error_impl>(message))
{}

const char*
device::error::
what() const noexcept
{
  return handle->m_message.c_str();
}           

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
  XRT_TRACE_POINT_SCOPE(xrt_device_load_xclbin);
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
  XRT_TRACE_POINT_SCOPE(xrt_device_load_xclbin);
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
  XRT_TRACE_POINT_SCOPE(xrt_device_load_xclbin);
  return xdp::native::profiling_wrapper("xrt::device::load_xclbin",
  [this, &xclbin]{
    handle->load_xclbin(xclbin);
    return xclbin.get_uuid();
  });
}

uuid
device::
register_xclbin(const xclbin& xclbin)
{
  XRT_TRACE_POINT_SCOPE(xrt_device_register_xclbin);
  return xdp::native::profiling_wrapper("xrt::device::register_xclbin",
  [this, &xclbin]{
    handle->record_xclbin(xclbin);
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

// Deprecated but referenced in binaries using xrt-2.11.x and earlier
boost::any
device::
get_info(info::device param) const
{
  // Old binaries call get_info without ABI and by
  // default will use current ABI version
  return query::get_info<boost::any>(handle.get(), param, xrt::detail::abi{});
}

// Deprecated but referenced in binaries using xrt-2.12.x
// Also used in any binary compiled as c++14, which is
// deprecated use of XRT as of xrt-2.13.x where XRT is now
// built with c++17
boost::any
device::
get_info(info::device param, const xrt::detail::abi& abi) const
{
  return query::get_info<boost::any>(handle.get(), param, abi);
}

#ifndef XRT_NO_STD_ANY
// Main get_info entry for binaries built with c++17.  XRT
// itself is built as c++17 starting xrt-2.13.x, but supports
// a back door build.sh option to switch compilaton to c++14
// in which case XRT_NO_STD_ANY is defined
std::any
device::
get_info_std(info::device param, const xrt::detail::abi& abi) const
{
  return query::get_info<std::any>(handle.get(), param, abi);
}
#endif

// Equality means that both device objects refer to the same
// underlying device.  The underlying device is opened based
// on device id
bool
operator== (const device& d1, const device& d2)
{
  return d1.get_handle()->get_device_id() == d2.get_handle()->get_device_id();
}

device::
~device()
{}

} // xrt

////////////////////////////////////////////////////////////////
// xrt_aie_device C++ API implmentations (xrt_aie.h)
////////////////////////////////////////////////////////////////
namespace xrt::aie {

// User passes column index relative to the partition and context id
// of that partition. Get patition info using the context id passed and
// convert relative column index to absolute using this info
static uint16_t
get_abs_col(const xrt_core::device* device, pid_t pid, uint16_t context_id, uint16_t col)
{
  auto data = xrt_core::device_query_default<xrt_core::query::aie_partition_info>(device, {});
  for (const auto& entry : data) {
    // compare PID and context id
    if (entry.pid != pid || std::stoi(entry.metadata.id) != context_id)
      continue;

    auto abs_col = col + entry.start_col;
    if (abs_col >= entry.start_col + entry.num_cols)
      throw std::out_of_range("col index out of range");
    return abs_col;
  }
  throw std::runtime_error("requested context_id not found");
}

static void
is_4byte_aligned_or_throw(uint32_t offset)
{
  if ((offset & 0x3) != 0)
    throw std::runtime_error("address is not 4 byte aligned");
}

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

std::vector<char>
device::
read_aie_mem(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t offset, uint32_t size) const
{
  return xdp::native::profiling_wrapper("xrt::aie::device::read_aie_mem",
    [this, &col, row, offset, size, context_id, pid] {
      try {
        // calculate absolute col index
        auto abs_col = get_abs_col(get_handle().get(), pid, context_id, col);
        is_4byte_aligned_or_throw(offset); // DRC check
        return get_handle()->read_aie_mem(abs_col, row, offset, size);
      }
      catch (const xrt_core::query::no_such_key&) {
        throw std::runtime_error("read_aie_mem is not supported on this platform");
      }
    });
}

size_t
device::
write_aie_mem(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t offset, const std::vector<char>& data)
{
  return xdp::native::profiling_wrapper("xrt::aie::device::write_aie_mem",
    [this, &col, row, offset, &data, context_id, pid] {
      try {
        // calculate absolute col index
        auto abs_col = get_abs_col(get_handle().get(), pid, context_id, col);
        is_4byte_aligned_or_throw(offset); // DRC check
        return get_handle()->write_aie_mem(abs_col, row, offset, data);
      }
      catch (const xrt_core::query::no_such_key&) {
        throw std::runtime_error("write_aie_mem is not supported on this platform");
      }
    });
}
uint32_t
device::
read_aie_reg(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t reg_addr) const
{
  return xdp::native::profiling_wrapper("xrt::device::read_aie_reg",
    [this, &col, row, reg_addr, context_id, pid] {
      try {
        // calculate absolute col index
        auto abs_col = get_abs_col(get_handle().get(), pid, context_id, col);
        is_4byte_aligned_or_throw(reg_addr); // DRC check
        return get_handle()->read_aie_reg(abs_col, row, reg_addr);
      }
      catch (const xrt_core::query::no_such_key&) {
        throw std::runtime_error("read_aie_reg is not supported on this platform");
      }
    });
}

bool
device::
write_aie_reg(pid_t pid, uint16_t context_id, uint16_t col, uint16_t row, uint32_t reg_addr, uint32_t reg_val)
{
  return xdp::native::profiling_wrapper("xrt::device::write_aie_reg",
    [this, &col, row, reg_addr, &reg_val, context_id, pid] {
      try {
        // calculate absolute col index
        auto abs_col = get_abs_col(get_handle().get(), pid, context_id, col);
        is_4byte_aligned_or_throw(reg_addr); // DRC check
        return get_handle()->write_aie_reg(abs_col, row, reg_addr, reg_val);
      }
      catch (const xrt_core::query::no_such_key&) {
        throw std::runtime_error("write_aie_reg is not supported on this platform");
      }
    });
}
} // xrt::aie

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
      xrt::xclbin xclbin{std::string{fnm}};
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
