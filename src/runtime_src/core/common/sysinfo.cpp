// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
// Local - Include files
#include "sysinfo.h"
#include "detail/sysinfo.h"
#include "system.h"

// System - Include Files
#include "gen/version.h"

namespace xrt_core::sysinfo {

void
get_xrt_info(boost::property_tree::ptree& pt)
{
  pt.put("version",    xrt_build_version);
  pt.put("branch",     xrt_build_version_branch);
  pt.put("hash",       xrt_build_version_hash);
  pt.put("build_date", xrt_build_version_date);
  xrt_core::get_driver_info(pt);
}

void
get_xrt_build_info(boost::property_tree::ptree& pt)
{
  pt.put("version",    xrt_build_version);
  pt.put("branch",     xrt_build_version_branch);
  pt.put("hash",       xrt_build_version_hash);
  pt.put("build_date", xrt_build_version_date);
}

void
get_os_info(boost::property_tree::ptree& pt)
{
  xrt_core::sysinfo::detail::get_os_info(pt);
}

} //xrt_core::sysinfo
