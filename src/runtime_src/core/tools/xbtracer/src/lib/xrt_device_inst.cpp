// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#define XCL_DRIVER_DLL_EXPORT
#include "capture.h"
#include "logger.h"

#include <filesystem>
#include <iostream>

namespace xtx = xrt::tools::xbtracer;
namespace fs = std::filesystem;

// NOLINTNEXTLINE(cert-err58-cpp)
static const xtx::xrt_ftbl& dtbl = xtx::xrt_ftbl::get_instance();

/*
 *  Device class instrumented methods
 * */
namespace xrt {
device::device(unsigned int index)
{
  auto func = "xrt::device::device(unsigned int)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.device.ctor, this, index);
   /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, index);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

device::device(const std::string& bdf)
{
  auto func = "xrt::device::device(const std::string&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.device.ctor_bdf, this, bdf);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, bdf);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

device::device(xclDeviceHandle dhdl)
{
  auto func = "xrt::device::device(xclDeviceHandle)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.device.ctor_dhdl, this, dhdl);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, dhdl);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

uuid device::register_xclbin(const xrt::xclbin& xclbin)
{
  auto func = "xrt::device::register_xclbin(const xrt::xclbin&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, xclbin.get_handle().get());
  uuid muuid;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.device.register_xclbin, muuid, xclbin);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, muuid);
  return muuid;
}

uuid device::load_xclbin(const axlf* xclbin)
{
  auto func = "xrt::device::load_xclbin(const axlf*)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &xclbin);
  uuid muuid;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.device.load_xclbin_axlf, muuid, xclbin);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, muuid);
  return muuid;
}

uuid device::load_xclbin(const std::string& fnm)
{
  auto func = "xrt::device::load_xclbin(const std::string&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, fnm);
  uuid muuid;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.device.load_xclbin_fnm, muuid, fnm);
  size_t file_size = fs::file_size(fnm);
  std::vector<unsigned char> buffer(file_size);
  std::ifstream file(fnm);
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-reinterpret-cast)
  if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
    std::cerr << "Failed to read " << fnm << "\n";
  }
  xtx::membuf xclbin(buffer.data(), file_size);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, muuid, "xclbin", xclbin);
  return muuid;
}

uuid device::load_xclbin(const xrt::xclbin& xclbin)
{
  auto func = "xrt::device::load_xclbin(const xrt::xclbin&)";
  XRT_TOOLS_XBT_FUNC_ENTRY(func, xclbin.get_handle().get());
  uuid muuid;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.device.load_xclbin_obj, muuid, xclbin);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, muuid);
  return muuid;
}

uuid device::get_xclbin_uuid() const
{
  auto func = "xrt::device::get_xclbin_uuid()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  uuid muuid;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.device.get_xclbin_uuid, muuid);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, muuid);
  return muuid;
}

void device::reset()
{
  auto func = "xrt::device::reset()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  XRT_TOOLS_XBT_CALL_METD(dtbl.device.reset);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

}  // namespace xrt
