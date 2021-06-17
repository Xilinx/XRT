/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef xrt_core_sensor_h_
#define xrt_core_sensor_h_

// Local - Include Files
#include "config.h"
#include "device.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

// System - Include Files
#include <iostream>


namespace xrt_core {
namespace sensor {

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_power_rails(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_thermals(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_power_consumption(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_fans(const xrt_core::device * device);

}} // sensor, xrt_core

#endif
