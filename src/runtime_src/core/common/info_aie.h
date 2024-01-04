// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021 Xilinx, Inc
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved

#ifndef COMMON_INFO_AIE_H
#define COMMON_INFO_AIE_H

#include <boost/property_tree/json_parser.hpp>

// Local - Include Files
#include "device.h"

namespace xrt_core {
namespace aie {

// Get AIE core information for this device
XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
aie_core(const xrt_core::device * device);

// Get AIE shim information for this device
XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
aie_shim(const xrt_core::device * device);

// Get AIE mem information for this device
XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
aie_mem(const xrt_core::device * device);

}} // aie, xrt_core

#endif
