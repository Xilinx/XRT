/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
 * 
 * Author: Sonal Santan
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

#include "core/common/device.h"

#include <string>

namespace xrt_core {
  std::vector<char>
  device_mem_read(device* device, uint64_t start_addr, uint64_t size);

  void
  device_mem_write(device* device, uint64_t start_addr, std::vector<char>& src);
}

#endif /* MEMACCESS_H */
