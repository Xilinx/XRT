/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef COMMON_INFO_AIE_H
#define COMMON_INFO_AIE_H

#include <boost/property_tree/json_parser.hpp>

// Local - Include Files
#include "device.h"

namespace xrt_core {
namespace aie {

// Get AIE core information for this device
boost::property_tree::ptree
aie_core(const xrt_core::device * device);

// Get AIE shim information for this device
boost::property_tree::ptree
aie_shim(const xrt_core::device * device);

// Get AIE mem information for this device
boost::property_tree::ptree
aie_mem(const xrt_core::device * device);

boost::property_tree::ptree
aie_partition(const xrt_core::device* device);

}} // aie, xrt_core

#endif
