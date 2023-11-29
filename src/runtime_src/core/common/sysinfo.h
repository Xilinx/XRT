// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _xrt_core_common_sysinfo_h_
#define _xrt_core_common_sysinfo_h_

// Local - Include Files
#include "config.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>

namespace xrt_core::sysinfo {

XRT_CORE_COMMON_EXPORT
void
get_xrt_info(boost::property_tree::ptree&);

XRT_CORE_COMMON_EXPORT
void
get_xrt_build_info(boost::property_tree::ptree&);

XRT_CORE_COMMON_EXPORT
void
get_os_info(boost::property_tree::ptree&);

}

#endif
