/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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

// This file implements XRT error APIs as declared in
// core/include/experimental/xrt_error.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/xrt/experimental/xrt_error.h"

#include "native_profile.h"
#include "device_int.h"
#include "error_int.h"
#include "core/include/xclerr_int.h"
#include "core/common/error.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"

#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include <map>
#include <cstring>

#ifdef _WIN32
# pragma warning( disable : 4996)
#endif

namespace {

static auto code_to_string = [] (auto& map, auto code, const std::string& msg)
{
  auto itr = map.find(code);
  if (itr == map.end())
    throw xrt_core::system_error(EINVAL, msg + std::to_string(code));
  return itr->second;
};

static std::string
error_number_to_string(xrtErrorNum err)
{
  const std::map<xrtErrorNum, std::string> map {
    {XRT_ERROR_NUM_FIRWWALL_TRIP,     "FIREWALL_TRIP"},
    {XRT_ERROR_NUM_TEMP_HIGH,         "TEMP_HIGH"},
    {XRT_ERROR_NUM_AIE_SATURATION,    "AIE_SATURATION"},
    {XRT_ERROR_NUM_AIE_FP,            "AIE_FP"},
    {XRT_ERROR_NUM_AIE_STREAM,        "AIE_STREAM"},
    {XRT_ERROR_NUM_AIE_ACCESS,        "AIE_ACCESS"},
    {XRT_ERROR_NUM_AIE_BUS,           "AIE_BUS"},
    {XRT_ERROR_NUM_AIE_INSTRUCTION,   "AIE_INSTRUCTION"},
    {XRT_ERROR_NUM_AIE_ECC,           "AIE_ECC"},
    {XRT_ERROR_NUM_AIE_LOCK,          "AIE_LOCK"},
    {XRT_ERROR_NUM_AIE_DMA,           "AIE_DMA"},
    {XRT_ERROR_NUM_AIE_MEM_PARITY,    "AIE_MEM_PARITY"},
  };

  return code_to_string(map, err, "Unknown error number");
}

static std::string
error_driver_to_string(xrtErrorDriver err)
{
  const std::map<xrtErrorDriver, std::string> map {
    {XRT_ERROR_DRIVER_XOCL,    "DRIVER_XOCL"},
    {XRT_ERROR_DRIVER_XCLMGMT, "DRIVER_XCLMGMT"},
    {XRT_ERROR_DRIVER_ZOCL,    "DRIVER_ZOCL"},
    {XRT_ERROR_DRIVER_AIE,     "DRIVER_AIE"}
  };

  return code_to_string(map, err, "Unknown error driver");
}

static std::string
error_severity_to_string(xrtErrorSeverity err)
{
  const std::map<xrtErrorSeverity, std::string> map {
    {XRT_ERROR_SEVERITY_EMERGENCY, "SEVERITY_EMERGENCY"},
    {XRT_ERROR_SEVERITY_ALERT,     "SEVERITY_ALERT"},
    {XRT_ERROR_SEVERITY_CRITICAL,  "SEVERITY_CRITICAL"},
    {XRT_ERROR_SEVERITY_ERROR,     "SEVERITY_ERROR"},
    {XRT_ERROR_SEVERITY_WARNING,   "SEVERITY_WARNING"},
    {XRT_ERROR_SEVERITY_NOTICE,    "SEVERITY_NOTICE"},
    {XRT_ERROR_SEVERITY_INFO,      "SEVERITY_INFO"},
    {XRT_ERROR_SEVERITY_DEBUG,     "SEVERITY_DEBUG"},
  };

  return code_to_string(map, err, "Unknown error severity");
}

static std::string
error_module_to_string(xrtErrorModule err)
{
  const std::map<xrtErrorModule, std::string> map {
    {XRT_ERROR_MODULE_FIREWALL,   "MODULE_FIREWALL"},
    {XRT_ERROR_MODULE_CMC,        "MODULE_CMC"},
    {XRT_ERROR_MODULE_AIE_CORE,   "MODULE_AIE_CORE"},
    {XRT_ERROR_MODULE_AIE_MEMORY, "MODULE_AIE_MEMORY"},
    {XRT_ERROR_MODULE_AIE_SHIM,   "MODULE_AIE_SHIM"},
    {XRT_ERROR_MODULE_AIE_NOC,    "MODULE_AIE_NOC"},
    {XRT_ERROR_MODULE_AIE_PL,     "MODULE_AIE_PL"},
  };

  return code_to_string(map, err, "Unknown error module");
}

static std::string
error_class_to_string(xrtErrorClass err)
{
  const std::map<xrtErrorClass, std::string> map {
    {XRT_ERROR_CLASS_SYSTEM,   "CLASS_SYSTEM"},
    {XRT_ERROR_CLASS_AIE,      "CLASS_AIE"},
    {XRT_ERROR_CLASS_HARDWARE, "CLASS_HARDWARE"},
  };

  return code_to_string(map, err, "Unknown error class");
}

static std::string
error_code_to_string(xrtErrorCode ecode)
{
  auto fmt = boost::format
    ("Error Number (%d): %s\n"
     "Error Driver (%d): %s\n"
     "Error Severity (%d): %s\n"
     "Error Module (%d): %s\n"
     "Error Class (%d): %s")
    % XRT_ERROR_NUM(ecode) % error_number_to_string(xrtErrorNum(XRT_ERROR_NUM(ecode)))
    % XRT_ERROR_DRIVER(ecode) % error_driver_to_string(xrtErrorDriver(XRT_ERROR_DRIVER(ecode)))
    % XRT_ERROR_SEVERITY(ecode) % error_severity_to_string(xrtErrorSeverity(XRT_ERROR_SEVERITY(ecode)))
    % XRT_ERROR_MODULE(ecode) % error_module_to_string(xrtErrorModule(XRT_ERROR_MODULE(ecode)))
    % XRT_ERROR_CLASS(ecode) % error_class_to_string(xrtErrorClass(XRT_ERROR_CLASS(ecode)));

  return fmt.str();
}

static void
error_code_to_json(xrtErrorCode ecode, boost::property_tree::ptree &pt)
{
  pt.put("class.code", XRT_ERROR_CLASS(ecode));
  pt.put("class.string", error_class_to_string(xrtErrorClass(XRT_ERROR_CLASS(ecode))));
  pt.put("module.code", XRT_ERROR_MODULE(ecode));
  pt.put("module.string", error_module_to_string(xrtErrorModule(XRT_ERROR_MODULE(ecode))));
  pt.put("severity.code", XRT_ERROR_SEVERITY(ecode));
  pt.put("severity.string", error_severity_to_string(xrtErrorSeverity(XRT_ERROR_SEVERITY(ecode))));
  pt.put("driver.code", XRT_ERROR_DRIVER(ecode));
  pt.put("driver.string", error_driver_to_string(xrtErrorDriver(XRT_ERROR_DRIVER(ecode))));
  pt.put("number.code", XRT_ERROR_NUM(ecode));
  pt.put("number.string", error_number_to_string(xrtErrorNum(XRT_ERROR_NUM(ecode))));
}

static std::string
error_time_to_string(xrtErrorTime time)
{
  return std::to_string(time);
}

// Necessary for disambiguating the make_shared call in the profiling_wrapper
static std::shared_ptr<xrt::error_impl>
alloc_error_from_device(const xrt_core::device* device, xrtErrorClass ecl)
{
  return std::make_shared<xrt::error_impl>(device, ecl);
}

static std::shared_ptr<xrt::error_impl>
alloc_error_from_code(xrtErrorCode ecode, xrtErrorTime timestamp)
{
  return std::make_shared<xrt::error_impl>(ecode, timestamp) ;
}

} // namespace

namespace xrt_core::error_int {

void
get_error_code_to_json(xrtErrorCode ecode, boost::property_tree::ptree &pt)
{
  return error_code_to_json(ecode, pt);
}

} // xrt_core::error_int

namespace xrt {

// class error_impl - class for error objects
//
// Life time of error objects is managed through shared pointers.
// An error object is freed when last reference is released.
class error_impl
{
  xrtErrorCode m_errcode = 0;
  xrtErrorTime m_timestamp = 0;
  std::string  m_ex_error_str;
public:
  error_impl(const xrt_core::device* device, xrtErrorClass ecl)
  {
    //Code for new format; Binary array of error structs in sysfs
    try {
      auto buf = xrt_core::device_query<xrt_core::query::xocl_errors>(device);
      if (buf.empty())
        return;
      if (device->get_ex_error_support() == true) {
        auto ect = xrt_core::query::xocl_errors::to_ex_value(buf, ecl);
        m_errcode = std::get<0>(ect);
        m_timestamp = std::get<1>(ect);
        uint64_t ex_error_code = std::get<2>(ect);
        m_ex_error_str = xrt_core::device_query<xrt_core::query::xocl_ex_error_code2string>(device, ex_error_code);
      }
      else {
        auto ect = xrt_core::query::xocl_errors::to_value(buf, ecl);
        std::tie(m_errcode, m_timestamp) = ect;
        m_ex_error_str = "";
      }
    } catch (const xrt_core::query::no_such_key&) {
      // Ignoring for now. Check below for edge if not available
      // query table of zocl doesn't have xocl_errors key
    }

    //Below code will be removed after zocl changes for new format
    auto errors = xrt_core::device_query<xrt_core::query::error>(device);
    for (auto& line : errors) {
      auto ect = xrt_core::query::error::to_value(line);
      if (XRT_ERROR_CLASS(ect.first) != ecl)
        continue;
      if (m_errcode)
        throw xrt_core::system_error(ERANGE,"Multiple errors for specified error class");
      std::tie(m_errcode, m_timestamp) = ect;
    }
  }

  error_impl(xrtErrorCode ecode, xrtErrorTime timestamp)
    : m_errcode(ecode), m_timestamp(timestamp)
  {}

  [[nodiscard]] xrtErrorCode
  get_error_code() const
  {
    return m_errcode;
  }

  [[nodiscard]] xrtErrorTime
  get_timestamp() const
  {
    return m_timestamp;
  }

  std::string
  to_string()
  {
    if (!m_errcode)
      return "No async error was detected";

    auto fmt = boost::format
      ("%s\n"
       "Timestamp: %s")
      % error_code_to_string(m_errcode)
      % error_time_to_string(m_timestamp)
      % m_ex_error_str;

    return fmt.str();
  }

};

} //namespace

////////////////////////////////////////////////////////////////
// xrt_xclbin C++ API implementations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
namespace xrt {

error::
error(const xrt::device& device, xrtErrorClass ecl)
  : handle(xdp::native::profiling_wrapper("xrt::error::error",
           alloc_error_from_device, device.get_handle().get(), ecl))
{}

error::
error(xrtErrorCode code, xrtErrorTime timestamp)
  : handle(xdp::native::profiling_wrapper("xrt::error::error",
	   alloc_error_from_code, code, timestamp))
{}

xrtErrorTime
error::
get_timestamp() const
{
  return xdp::native::profiling_wrapper("xrt::error::get_timestamp", [this]{
    return handle->get_timestamp();
  });
}

xrtErrorCode
error::
get_error_code() const
{
  return xdp::native::profiling_wrapper("xrt::error::get_error_code", [this]{
    return handle->get_error_code();
  });
}

std::string
error::
to_string() const
{
  return xdp::native::profiling_wrapper("xrt::error::to_string", [this]{
    return handle->to_string();
  });
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// xrt_xclbin C API implmentations (xrt_xclbin.h)
////////////////////////////////////////////////////////////////
int
xrtErrorGetLast(xrtDeviceHandle dhdl, xrtErrorClass ecl, xrtErrorCode* error, uint64_t* timestamp)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, ecl, error, timestamp]{
      auto handle = xrt::error_impl(xrt_core::device_int::get_core_device(dhdl).get(), ecl);
      *error = handle.get_error_code();
      *timestamp = handle.get_timestamp();
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

int
xrtErrorGetString(xrtDeviceHandle, xrtErrorCode error, char* out, size_t len, size_t* out_len)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [error, out, len, out_len]{
      auto str = error_code_to_string(error);

      if (out_len)
        *out_len = str.size() + 1;

      if (!out)
        return 0;

      auto cp_len = std::min(len-1, str.size());
      std::strncpy(out, str.c_str(), cp_len);
      out[cp_len] = 0;

      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}
