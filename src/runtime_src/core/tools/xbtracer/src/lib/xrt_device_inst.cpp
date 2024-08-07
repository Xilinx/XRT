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

}  // namespace xrt
