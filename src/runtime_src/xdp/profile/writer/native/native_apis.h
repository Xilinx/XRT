/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc - All rights reserved
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

#ifndef NATIVE_APIS_DOT_H
#define NATIVE_APIS_DOT_H

namespace xdp {
namespace native {

constexpr const char* APIs[] = {
  "xrt::bo::bo",
  "xrt::bo::size",
  "xrt::bo::address",
  "xrt::bo::export_buffer",
  "xrt::bo::sync",
  "xrt::bo::map",
  "xrt::bo::write",
  "xrt::bo::read",
  "xrt::bo::copy",
  "xrtBOAllocUserPtr",
  "xrtBOAlloc",
  "xrtBOSubAlloc",
  "xrtBOImport",
  "xrtBOExport",
  "xrtBOFree",
  "xrtBOSize",
  "xrtBOSync",
  "xrtBOMap",
  "xrtBOWrite",
  "xrtBORead",
  "xrtBOCopy",
  "xrtBOAddress",
  "xrt::device::device",
  "xrt::device::load_xclbin",
  "xrt::device::get_xclbin_uuid",
  "xrt::device::reset",
  "xrt::device::get_xclbin_section",
  "xrtDeviceOpen",
  "xrtDeviceOpenByBDF",
  "xrtDeviceClose",
  "xrtDeviceLoadXclbin",
  "xrtDeviceLoadXclbinFile",
  "xrtDeviceLoadXclbinHandle",
  "xrtDeviceLoadXclbinUUID",
  "xrtDeviceGetXclbinUUID",
  "xrtDeviceToXclDevice",
  "xrtDeviceOpenFromXcl",
  "xrt::error::error",
  "xrt::error::get_timestamp",
  "xrt::error::get_error_code",
  "xrt::error::to_string",
  "xrtErrorGetLast",
  "xrtErrorGetString",
  "xrt::graph::reset",
  "xrt::graph::get_timestamp",
  "xrt::graph::run",
  "xrt::graph::wait",
  "xrt::graph::suspend",
  "xrt::graph::resume",
  "xrt::graph::end",
  "xrt::graph::update_port",
  "xrt::graph::read_port",
  "xrt::run::run",
  "xrt::run::start",
  "xrt::run::wait",
  "xrt::run::state",
  "xrt::run::set_event",
  "xrt::run::get_ert_packet",
  "xrt::kernel::kernel",
  "xrt::kernel::read_register",
  "xrt::kernel::write_register",
  "xrt::kernel::group_id",
  "xrt::kernel::offset",
  "xrtPLKernelOpen",
  "xrtPLKernelOpenExclusive",
  "xrtKernelClose",
  "xrtRunOpen",
  "xrtKernelArgGroupId",
  "xrtKernelArgOffset",
  "xrtKernelReadRegister",
  "xrtKernelWriteRegister",
  "xrtKernelRun",
  "xrtRunClose",
  "xrtRunState",
  "xrtRunWait",
  "xrtRunWaitFor",
  "xrtRunSetCallback",
  "xrtRunStart",
  "xrtRunUpdateArg",
  "xrtRunUpdateArgV",
  "xrtRunSetArg",
  "xrtRunSetArgV",
  "xrtRunGetArgV",
  "xrtRunGetArgVPP",
  "xrtXclbinAllocFilename",
  "xrtXclbinAllocRawData",
  "xrtXclbinFreeHandle",
  "xrtXclbinGetXSAName",
  "xrtXclbinGetUUID",
  "xrtXclbinGetData",
  "xrtXclbinUUID",
  "xrt::psrun::psrun",
  "xrt::psrun::start",
  "xrt::psrun::wait",
  "xrt::psrun::state",
  "xrt::psrun::set_event",
  "xrt::psrun::get_ert_packet",
  "xrt::pskernel::kernel",
  "xrt::pskernel::offset",
  "xrtPSKernelOpen",
  "xrtPSKernelOpenExclusive",
  "xrtPSKernelClose",
  "xrtPSRunOpen",
  "xrtPSKernelArgGroupId",
  "xrtPSKernelArgOffset",
  "xrtPSKernelRun",
  "xrtPSRunClose",
  "xrtPSRunState",
  "xrtPSRunWait",
  "xrtPSRunWaitFor",
  "xrtPSRunSetCalback",
  "xrtPSRunStart"
};

} // end namespace native
} // end namespace xdp

#endif
