// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include <boost/filesystem/path.hpp>

namespace xrt_core::detail {

bfs::path
xilinx_xrt()
{
#if defined (__aarch64__) || defined (__arm__)
  return bfs::path("/usr");
#else
  return bfs::path("/opt/xilinx/xrt");
#endif
}

} // xrt_core::detail

