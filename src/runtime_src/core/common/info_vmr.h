// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef COMMON_INFO_VMR_H
#define COMMON_INFO_VMR_H

// Local - Include Files
#include "device.h"

namespace xrt_core {
namespace vmr {

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
vmr_info(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
bool
is_default_boot(const xrt_core::device* device);

}} // vmr, xrt

#endif
