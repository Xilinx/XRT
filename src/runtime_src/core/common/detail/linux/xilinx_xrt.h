// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include <boost/filesystem/path.hpp>
#include <stdexcept>
#include <string>
namespace xrt_core::detail {

namespace bfs = boost::filesystem;

bfs::path
xilinx_xrt()
{
#if defined (__aarch64__) || defined (__arm__)
  return bfs::path("/usr");
#else
  return bfs::path("/opt/xilinx/xrt");
#endif
}

bfs::path
xclbin_path(const std::string& xclbin)
{
  throw std::runtime_error("xclbin repo path not yet implemented on Linux");
}

} // xrt_core::detail

