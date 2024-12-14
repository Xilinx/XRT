// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#define XRT_API_SOURCE         // in same dll as api
#include "capture.h"
#include "logger.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
namespace xtx = xrt::tools::xbtracer;

// NOLINTNEXTLINE(cert-err58-cpp)
static const xtx::xrt_ftbl& dtbl = xtx::xrt_ftbl::get_instance();

/*
 *  Xclbin class instrumented methods
 * */
namespace xrt {
xclbin::xclbin(const std::string& fnm)
{
  auto func = "xrt::xclbin::xclbin(const std::string&)";
  try
  {
    XRT_TOOLS_XBT_CALL_CTOR(dtbl.xclbin.ctor_fnm, this, fnm);
    /* As pimpl will be updated only after ctor call*/
    XRT_TOOLS_XBT_FUNC_ENTRY(func, fnm);

    // Usage in xclbin constructor
    std::vector<unsigned char> buffer;
    xtx::read_file(fnm, buffer);

    xtx::membuf xclbin(buffer.data(), buffer.size());
    XRT_TOOLS_XBT_FUNC_EXIT(func, "xclbin", xclbin);
  }
  catch (const std::exception& ex)
  {
    std::cout << "Exception: " << ex.what() << '\n';
  }
}

xclbin::xclbin(const std::vector<char>& data)
{
  auto func = "xrt::xclbin::xclbin(const std::vector<char>&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.xclbin.ctor_raw, this, data);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &data);
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-reinterpret-cast)
  xtx::membuf data_buf(reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())), data.size());
  XRT_TOOLS_XBT_FUNC_EXIT(func, "data_buf", data_buf);
}

xclbin::xclbin(const axlf* maxlf)
{
  auto func = "xrt::xclbin::xclbin(const axlf*)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.xclbin.ctor_axlf, this, maxlf);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &maxlf);
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-reinterpret-cast)
  xtx::membuf maxlf_buf(reinterpret_cast<unsigned char*>(const_cast<axlf*>(maxlf)), sizeof(axlf));
  XRT_TOOLS_XBT_FUNC_EXIT(func, "maxlf_buf", maxlf_buf);
}
}  // namespace xrt
