/*
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "core/common/device.h"
#include "core/include/xrt/detail/xclbin.h"

#include <vector>

namespace xrt_core { namespace xclbin { namespace swemu {

// Create binary section "IP_LAYOUT", "CONNECTIVITY", "MEM_TOPOLOGY"
// from XML meta data section in xclbin.  This is used to allow sw
// emulation to use the native XRT APIs.  Supposedly a temporary
// work-around until the xclbin contains the necessary sections for
// software emulation.
std::vector<char>
get_axlf_section(const device* device, const axlf* top, axlf_section_kind kind);

// For xrt_xclbin.cpp
// ip_layout will be nullptr, until it has been created.  It must be
// created before this API can be used to create connectivity section.
// See comments in code that use this API
std::vector<char>
get_axlf_section(const axlf* top, const ::ip_layout* ip_layout, axlf_section_kind kind);

}}} // swemu, xclbin, xrt_core
