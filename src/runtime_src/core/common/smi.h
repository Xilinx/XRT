// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
// Local include files
#include "config.h"

#include <string>

namespace xrt_core::smi {

std::string 
get_smi_config();

/* Needs to be exported so its available within XB utilities like xrt-smi*/
XRT_CORE_COMMON_EXPORT
std::string
get_default_smi_config();

} // namespace xrt_core::smi
