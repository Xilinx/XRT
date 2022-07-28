// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_HW_CONTEXT_H_
#define XRT_HW_CONTEXT_H_

#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"

#include "xrt/xrt_device.h"
#include "xrt/xrt_uuid.h"

#include "xcl_hwctx.h"

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
  /**
   * @enum priority - tbd
   *
   * @var exclusive
   *  Create a context for exclusive access to shareable resources.
   *  Legacy compute unit access control.
   * @var shared
   *  Create a context for shared access to shareable resources
   *  Legacy compute unit access control.
   */
  enum class qos : xcl_qos_type {
    exclusive = XCL_QOS_EXCLUSIVE,  // legacy
    shared = XCL_QOS_SHARED,        // legacy
    reserved = 0
  };

public:
  /**
   * hw_context() - Constructor for empty context
   */
  hw_context() = default;

  /**
   * hw_context() - Constructor
   *
   * @param device
   *  Device where context is created
   * @param xclbin_id
   *  UUID of xclbin that should be assigned to HW resources
   * @qos
   *  Quality of service request that should be fulfilled by the context
   */
  XRT_API_EXPORT
  hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, qos qos);

  ///@cond
  // Undocumented construction w/o specifying qos
  // Subject to change in default qos value
  hw_context(const xrt::device& device, const xrt::uuid& xclbin_id)
    : hw_context{device, xclbin_id, static_cast<qos>(0)}
  {}
  /// @endcond

  /**
   * get_device() - Device from which context was created
   */
  XRT_API_EXPORT
  xrt::device
  get_device() const;

  /**
   * get_xclbin_uuid() - UUID of xclbin from which context was created
   */
  XRT_API_EXPORT
  xrt::uuid
  get_xclbin_uuid() const;

  /**
   * get_xclbin() - Retrieve underlying xclbin matching the UUID
   */
  XRT_API_EXPORT
  xrt::xclbin
  get_xclbin() const;

  /**
   * get_qos() - Get the QOS value of the context
   */
  XRT_API_EXPORT
  qos
  get_qos() const;

public:
  /// @cond
  // Undocumented internal access to low level context handle
  // Subject to change without warning
  XRT_API_EXPORT
  explicit operator xcl_hwctx_handle () const;
  /// @endcond
};

} // namespace xrt

#else
# error xrt_hwcontext is only implemented for C++
#endif // __cplusplus

#endif
