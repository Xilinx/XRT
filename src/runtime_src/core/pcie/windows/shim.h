/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef _XRT_CORE_PCIE_WINDOWS_SHIM_H
#define _XRT_CORE_PCIE_WINDOWS_SHIM_H

#include "core/pcie/windows/config.h"
#include "xrt.h"
#include "core/common/xrt_profiling.h"
#include "core/pcie/driver/windows/include/XoclUser_INTF.h"

struct FeatureRomHeader;

namespace userpf {

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_rom_info(xclDeviceHandle hdl, FeatureRomHeader* value);


XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_device_info(xclDeviceHandle hdl, XOCL_DEVICE_INFORMATION* value);

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_mem_topology(xclDeviceHandle hdl, char* buffer, size_t len);

} // userpf


#endif
