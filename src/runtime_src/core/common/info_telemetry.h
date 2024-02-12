// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef COMMON_INFO_TELEMETRY_H
#define COMMON_INFO_TELEMETRY_H

// Local - Include Files
#include "device.h"

#include <boost/property_tree/ptree.hpp>

namespace xrt_core {
namespace telemetry {

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
telemetry_info(const xrt_core::device* device);

}} // telemetry, xrt_core

#endif
