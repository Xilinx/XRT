// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc
// Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.
//
// Xilinx Runtime (XRT) Experimental APIs

#ifndef _XRT_COMMON_KERNEL_INT_H_
#define _XRT_COMMON_KERNEL_INT_H_

// This file defines implementation extensions to the XRT Kernel APIs.
#include "core/include/xrt/experimental/xrt_kernel.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include "core/common/config.h"
#include "core/common/xclbin_parser.h"
#include "core/common/shim/buffer_handle.h"

#include <bitset>
#include <cstdint>
#include <string>
#include <vector>

namespace xrt_core::xdp {
struct xrt_kernel_data;
}

namespace xrt_core::kernel_int {

// Provide access to kdma command based BO copy Used by xrt::bo::copy.
// Arguably this should implemented by by shim->copy_bo, but must wait
// until execbuf()/execwait() can handle multithreading with one device .
void
copy_bo_with_kdma(const std::shared_ptr<xrt_core::device>& core_device,
                  size_t sz,
                  buffer_handle* dst_bo, size_t dst_offset,
                  buffer_handle* src_bo, size_t src_offset);

XRT_CORE_COMMON_EXPORT
std::vector<const xclbin::kernel_argument*>
get_args(const xrt::kernel&);

XRT_CORE_COMMON_EXPORT
const xclbin::kernel_argument*
get_arg_info(const xrt::run& run, size_t argidx);

XRT_CORE_COMMON_EXPORT
std::vector<uint32_t>
get_arg_value(const xrt::run&, size_t argidx);

XRT_CORE_COMMON_EXPORT
xrt_core::xclbin::kernel_argument::argtype
arg_type_at_index(const xrt::kernel& kernel, size_t idx);

XRT_CORE_COMMON_EXPORT
void
set_arg_at_index(const xrt::run& run, size_t idx, const void* value, size_t bytes);

XRT_CORE_COMMON_EXPORT
xrt::run
clone(const xrt::run& run);

// This API is provide to allow implementations such as OpenCL
// to dictate what kernel CUs to use.  For example sub-device
// may restrict CUs.
XRT_CORE_COMMON_EXPORT
void
set_cus(xrt::run& run, const std::bitset<128>& mask);

XRT_CORE_COMMON_EXPORT
const std::bitset<128>&
get_cumask(const xrt::run& run);

inline size_t
get_num_cus(const xrt::run& run)
{
  return get_cumask(run).count();
}

XRT_CORE_COMMON_EXPORT
xrt::xclbin::ip::control_type
get_control_protocol(const xrt::run& run);

XRT_CORE_COMMON_EXPORT
void
pop_callback(const xrt::run& run);

XRT_CORE_COMMON_EXPORT
size_t
get_regmap_size(const xrt::kernel& kernel);

// get_instance_name() - Get the kernel instance name
// Return the name used to create the kernel before it is stripped to
// remove instance identifier part.  In xclbin flow, this name is not
// necessarily an instance name, but in ELF flow it uniquely
// identifies a kernel instance.
std::string
get_instance_name(const xrt::kernel&);

// get_hwctx() - Get hwctx in which this kernel is created
xrt::hw_context
get_hwctx(const xrt::kernel& kernel);

// get_ctrlcode() - Get kernel ctrlcode
// For kernels created from legacy xrt::module
xrt::elf
get_ctrlcode(const xrt::kernel&);

// get_hwctx() - Get hwctx in which the run kernel is created
XRT_CORE_COMMON_EXPORT
xrt::hw_context
get_hwctx(const xrt::run&);

// get_kernel() - Get xrt::kernel fron which run is created
xrt::kernel
get_kernel(const xrt::run&);

// get_kernel_from_impl() - wrap impl in an xrt::kernel
// Returns xrt::kernel created from shared kernel_impl.
xrt::kernel
get_kernel_from_impl(const xrt::kernel_impl* kernel_impl);

// get_run_from_impl() - wrap impl in an xrt::run
// Returns xrt::run created from shared run_impl.
xrt::run
get_run_from_impl(const xrt::run_impl*);

// Fill XDP kernel data from a run_impl pointer for profiling hooks
XRT_CORE_COMMON_EXPORT
void
get_xdp_kernel_data(const xrt::run_impl* run_impl, xrt_core::xdp::xrt_kernel_data* data);

// Set dtrace control file on a run_impl handle
// This is used by XDP profiling to set the CT file without requiring xrt::run
XRT_CORE_COMMON_EXPORT
void
set_dtrace_control_file(xrt::run_impl* run_impl, const std::string& path);

} // xrt_core::kernel_int

#endif
