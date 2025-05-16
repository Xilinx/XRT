/**
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc - All rights reserved
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

namespace xdp::native {

// This set of strings are all the native XRT APIs we're monitoring, and
// match the strings passed in by the profiling_wrapper function.
// We use this to put together a final summary table of how many times each
// function was called and statistics on execution time.  It is a constexpr
// to avoid creation of any strings at the point of the call to reduce
// how long it takes to capture that the function was called.  The overhead
// is instead at the loading time of the library and the end of the application

constexpr const char* APIs[] = {

  // Functions profiled in aie/xrt_graph.cpp
  "xrt::graph::reset",
  "xrt::graph::get_timestamp",
  "xrt::graph::run",
  "xrt::graph::wait",
  "xrt::graph::suspend",
  "xrt::graph::resume",
  "xrt::graph::end",
  "xrt::graph::update_port",
  "xrt::graph::read_port",
  "xrt::aie::profiling::start",
  "xrt::aie::profiling::read",
  "xrt::aie::profiling::stop",

  // Functions profiled in xrt_bo.cpp
  "xrt::bo::bo",
  "xrt::bo::size",
  "xrt::bo::address",
  "xrt::bo::memory_group",
  "xrt::bo::get_flags",
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
  "xrtBOAllocFromXcl",
  "xrtBOFree",
  "xrtBOSize",
  "xrtBOSync",
  "xrtBOMap",
  "xrtBOWrite",
  "xrtBORead",
  "xrtBOCopy",
  "xrtBOAddress",

  // Functions profiled in xrt_device.cpp
  "xrt::device::device",
  "xrt::device::load_xclbin",
  "xrt::device::register_xclbin",
  "xrt::device::get_xclbin_uuid",
  "xrt::device::reset",
  "xrt::device::get_xclbin_section",
  "xrt::aie::device::read_aie_mem",
  "xrt::aie::device::write_aie_mem",
  "xrt::device::read_aie_reg",
  "xrt::device::write_aie_reg",
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

  // Functions profiled in xrt_error.cpp
  "xrt::error::error",
  "xrt::error::get_timestamp",
  "xrt::error::get_error_code",
  "xrt::error::to_string",
  "xrtErrorGetLast",
  "xrtErrorGetString",

  // Functions profiled in xrt_ip.cpp
  "xrt::ip::write_register",
  "xrt::ip::read_register",

  // Functions profiled in xrt_kernel.cpp
  "xrt::run::run",
  "xrt::run::start",
  "xrt::run::wait",
  "xrt::run::state",
  "xrt::run::return_code",
  "xrt::run::get_ert_packet",
  "xrt::run::submit_wait",
  "xrt::run::submit_signal",
  "xrt::run::get_ctrl_scratchpad_bo",  
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

  // Functions profiled in xrt_xclbin.cpp
  "xrtXclbinAllocFilename",
  "xrtXclbinAllocRawData",
  "xrtXclbinFreeHandle",
  "xrtXclbinGetXSAName",
  "xrtXclbinGetUUID",
  "xrtXclbinGetNumKernels",
  "xrtXclbinGetNumKernelComputeUnits",
  "xrtXclbinGetData",
  "xrtXclbinUUID"
};

} // end namespace xdp::native

#endif
