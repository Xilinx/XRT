// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_HW_CONTEXT_H_
#define XRT_HW_CONTEXT_H_

#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"

#include "xrt/xrt_device.h"
#include "xrt/xrt_uuid.h"

#ifdef __cplusplus

namespace xrt {

/**
 * class hw_context -- manage hw resources
 *
 * A hardware context associates an xclbin with hardware
 * resources.  Prior to creating a context, the xclbin
 * must be registered with the device (`xrt::device::register_xclbin`)
 *
 */
class hw_context_impl;
class hw_context : public detail::pimpl<hw_context_impl>
{
public:
  enum class priority {};
public:
  /**
   * hw_context() - Constructor for empty context
   */
  hw_context() = default;

  /**
   * hw_context() - Constructor
   */
  XRT_API_EXPORT
  hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, priority qos);

  hw_context(const xrt::device& device, const xrt::uuid& xclbin_id)
    : hw_context{device, xclbin_id, static_cast<priority>(0)}
  {}

  XRT_API_EXPORT
  xrt::device
  get_device() const;

  XRT_API_EXPORT
  xrt::uuid
  get_xclbin_uuid() const;

  XRT_API_EXPORT
  xrt::xclbin
  get_xclbin() const;
};

} // namespace xrt

#else
# error xrt_hwcontext is only implemented for C++
#endif // __cplusplus

#endif
