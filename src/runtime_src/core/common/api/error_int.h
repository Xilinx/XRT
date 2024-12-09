/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef _XRT_COMMON_ERROR_INT_H_
#define _XRT_COMMON_ERROR_INT_H_

// This file defines implementation extensions to the XRT Error APIs.
#include "core/include/xrt/experimental/xrt_error.h"
#include "core/common/config.h"
#include <boost/property_tree/ptree.hpp>

namespace xrt_core { namespace error_int {

XRT_CORE_COMMON_EXPORT
void
get_error_code_to_json(xrtErrorCode ecode, boost::property_tree::ptree &pt);

}} // error_int, xrt_core

#endif
