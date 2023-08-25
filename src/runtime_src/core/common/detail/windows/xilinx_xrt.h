// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/dlfcn.h"
#include <boost/filesystem/path.hpp>

namespace xrt_core::detail {

bfs::path
xilinx_xrt()
{
  return bfs::path(xrt_core::dlpath("xrt_coreutil.dll")).parent_path();
}

} // xrt_core::detail

