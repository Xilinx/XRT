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

#include "xrt.h"
#include "core/common/xrt_profiling.h"
#include "boost/any.hpp"
#include <boost/property_tree/ptree.hpp>

 // To be simplified
#include "core/pcie/driver/windows/include/XoclUser_INTF.h"

__declspec(dllexport) void queryDeviceWithQR(xclDeviceHandle handle, boost::any & _returnValue, int QR_ID, const std::type_info & _typeInfo, uint64_t statClass);

__declspec(dllexport) void shim_get_ip_layout(xclDeviceHandle handle, struct ip_layout **ipLayout, DWORD size);

__declspec(dllexport) DWORD shim_get_ip_layoutsize(xclDeviceHandle handle);

__declspec(dllexport) DWORD shim_get_mem_topology(xclDeviceHandle handle, struct mem_topology *topoInfo);
__declspec(dllexport) DWORD shim_get_mem_rawinfo(xclDeviceHandle handle, struct mem_raw_info *memRaw);

namespace xocl { // shared implementation

} // xocl

#endif
