// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
#ifndef COMMON_INFO_MEMORY_H
#define COMMON_INFO_MEMORY_H

// Local - Include Files
#include "device.h"

namespace xrt_core {
namespace memory {

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
memory_topology(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
dynamic_regions(const xrt_core::device * device);

}} // memory, xrt

#endif
