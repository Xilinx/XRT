/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
 *
 * Simple command line utility to inetract with SDX PCIe devices
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
#ifndef MEMACCESS_H
#define MEMACCESS_H

// Local includes
#include "core/common/device.h"

// System includes
#include <string>
namespace xrt_core {

// This function safely reads from a device's memory banks. It will
// ensure that the read attempts start/end on memory bank borders
// when applicable. This prevents reading from an unused bank or
// writing out of bounds. The memory bank usage may not be 
// contiguous and this function account for these situations.
XRT_CORE_COMMON_EXPORT
std::vector<char>
device_mem_read(device* device, const uint64_t start_addr, const uint64_t size);

// This function safely writes to a device's memory banks. It will
// ensure that the write attempts start/end on memory bank borders
// when applicable. This prevents writing to an unused bank or
// writing out of bounds. The memory bank usage may not be 
// contiguous and this function account for these situations.
XRT_CORE_COMMON_EXPORT
void
device_mem_write(device* device, const uint64_t start_addr, const std::vector<char>& src);
}

#endif /* MEMACCESS_H */
