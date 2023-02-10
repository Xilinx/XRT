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

enum class vmr_status_type {
  boot_on_default = 0,
  has_fpt,
};

XRT_CORE_COMMON_EXPORT
bool
get_vmr_status(const xrt_core::device* device, vmr_status_type status);

}} // vmr, xrt

#endif
