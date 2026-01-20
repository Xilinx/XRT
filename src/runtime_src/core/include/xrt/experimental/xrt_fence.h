// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_FENCE_H_
#define XRT_FENCE_H_

#include "xrt/detail/config.h"
#include "xrt/xrt_device.h"

// Until better place for pid_type
#include "xrt/xrt_bo.h"

#ifdef __cplusplus
# include <cstdint>
# include <chrono>
# include <condition_variable>
#endif

#ifdef __cplusplus
// Opaque handle to internal use
namespace xrt_core {
class fence_handle;
}

namespace xrt {

/*!
 * @class fence
 *
 * @brief
 * Fence object for synchronization of operations
 *
 * @details
 * A fence object is used to synchronize operations between
 * run objects.
 *
 * A fence object should be signaled by one run object and waited on
 * by other run objects.  The fence represents the expected next state
 * of a run object, it is enqueued as a wait for one or more run
 * objects, and it is signaled upon completion of the run on which is
 * was enqueued.
 *
 * The fence object has state that represents the next value (fence
 * id) of an enqueued operation and is has state that represents the
 * corresponding value to wait for. Both the next and the expected
 * value is incremented when the fence is signaled or waited upon, and
 * therefore a fence should be copied if more than one operation is
 * dependent on it.  A fence should never be signaled by more than one
 * run object.
 *
 * A fence object can be exported for use by another process. This
 * allows setting up a pipeline of operations between processes.
 */
class fence_impl;  
class fence : public detail::pimpl<fence_impl>
{
public:
#ifdef _WIN32
  using export_handle = uint64_t;
#else
  using export_handle = int;
#endif

public:
  /**
   * @enum access_mode - buffer object accessibility
   *
   * @var local
   *   Access is local to process and device on which it is allocated
   * @var shared
   *   Access is shared between devices within process
   * @var process
   *   Access is shared between processes and devices
   * @var hybrid
   *   Access is shared between drivers (cross-adapater)
   */
  enum class access_mode : uint8_t { local, shared, process, hybrid };

  /**
   * fence() - Default constructor
   *
   * Constructs an empty fence object that converts to false in
   * boolean comparisons.
   */
  fence() = default;

  /**
   * fence() - Constructor for fence object with specific access
   *
   * @param device
   *  The device on which to allocate this fence
   * @param access
   *  Specific access mode for the buffer (see `enum access_mode`)
   *
   * The fence object should be signaled by one run object and waited
   * on by other run objects.  The fence represents the expected next
   * state of a run object, it is enqueued as a wait for one or
   * more run objects, and it is signaled upon completion of the run
   * on which is was enqueued.
   *
   * The fence object has state that represents the next value (fence
   * id) of an enqueued operation and is has state that represents the
   * corresponding value to wait for. Both the next and the expected
   * value is incremented when the fence is signaled or waited upon,
   * and therefore a fence should be copied if more than one operation
   * is dependent on it.  A fence should never be signaled by more
   * than one run object.
   */
  XRT_API_EXPORT
  fence(const xrt::device& device, access_mode access);

  /**
   * fence() - Constructor to import an exported fence
   *
   * @param pid
   *  Process id of exporting process
   * @param ehdl
   *  Exported fence handle, implementation specific type
   *
   * The exported fence handle is obtained from exporting process by
   * calling `export_fence()` on the fence to be exported.
   */
  XRT_API_EXPORT
  fence(const xrt::device& device, pid_type pid, export_handle ehdl);

  /**
   * fence() - Copy constructor
   *
   * Creates a new fence object that is a copy of the other fence.
   * There is no shared state between the two fence objects.
   *
   * The copy constructor is used when a fence is to be waited on by
   * more than one consumer.  Since the fence has state that increments
   * at each wait submission, the fence should be copied if more than
   * one consumer is to wait on it.
   */
  XRT_API_EXPORT
  fence(const fence& other);

  /**
   * fence() - Move constructor
   *
   * Move constructor.  The other fence object is left in an
   * unspecified state.
   */
  XRT_API_EXPORT
  fence(fence&& other) noexcept;

  /**
   * Move assignment operator
   *
   * The other fence object is left in an unspecified state.
   */
  XRT_API_EXPORT
  fence&
  operator=(fence&& other) noexcept = default;
    
  ///@cond
  // Constructor from internal implementation
  XRT_API_EXPORT
  fence(std::unique_ptr<xrt_core::fence_handle> handle);
  ///@endcond

  /**
   * export_fence() - Export fence object to another process
   *
   * @return
   *  Exported fence handle, implementation specific type.  This
   *  exported handle can be imported by another process
   */
  XRT_API_EXPORT
  export_handle
  export_fence();

  /**
   * signal() - Signal the fence with specified value.
   *
   * @param value Value to signal the fence with.
   *
   * This function is provided for host side signaling unrelated to
   * the execution of a command on a hardware queue.  For example, a
   * fence submitted for wait on a hardware queue may need to be
   * signaled by the host to indicate that some prerequisite
   * operation is ready.
   *
   * After calling this function, the fence current value will be
   * minimum the specified `value`. The next value of the fence is set
   * to one more than the signaled value.
   *
   * Note, that the next value of a fence is the value that will be
   * implicitly signaled or waited on if the fence is submitted to as
   * a sync wait object to a hardware queue through an xrt::run
   * object.
   *
   * It is undefined behavior to signal a fence if the fence is
   * currently submitted for signaling to a hardware queue through
   * an xrt::run object.
   *
   * The function returns immediately without signaling if the
   * specified value is less than the current fence value.
   */
  XRT_API_EXPORT
  void
  signal(uint64_t value);

  /**
   * signal() - Signal the fence at its next value
   *
   * This function is provided for host side signaling unrelated to
   * the execution of a command on a hardware queue.  For example, a
   * fence submitted for wait on a hardware queue may need to be
   * signaled by the host to indicate that some prerequisite
   * operation is ready.
   *
   * After calling this function, the fence current value will be
   * updated to the next value and the next value will be incremented.
   *
   * Note, that the next value of a fence is the value that will be
   * implicitly signaled or waited on if the fence is submitted for
   * signaling to a hardware queue through an xrt::run object.
   *
   * It is undefined behavior to signal a fence if the fence is
   * currently submitted for signaling to a hardware queue through
   * an xrt::run object.
   */
  XRT_API_EXPORT
  void
  signal();

  /**
   * wait() - Wait for fence to be signaled at specified value
   *
   * Wait for fence to be signaled at specified value. This is
   * CPU blocking operation.
   *
   * @param value Value to wait for
   * @param timeout Timeout for wait.  A value of 0, implies block
   *  until completes.
   * @return std::cv_status::no_timeout when wait
   *  completes succesfully.  std::cv_status::timeout when wait times
   *  out.
   *
   * This function is provided for host side waiting unrelated to the
   * execution of a command on a hardware queue.  For example, a fence
   * submitted for signaling on a hardware queue may need to be waited
   * on by the host to indicate that it can proceed with some
   * operation.
   */
  XRT_API_EXPORT
  std::cv_status
  wait(uint64_t value, const std::chrono::milliseconds& timeout);

  /**
   * wait() - Wait for fence to be signaled
   *
   * Wait for fence to be signaled at its current value. This is
   * CPU blocking operation.
   *
   * @paramm timeout Timeout for wait.  A value of 0, implies block
   *  until completes.
   * @return std::cv_status::no_timeout when wait completes
   *  succesfully.  std::cv_status::timeout when wait times out.
   *
   * This function is provided for host side waiting unrelated to the
   * execution of a command on a hardware queue.  For example, a fence
   * submitted for signaling on a hardware queue may need to be waited
   * on by the host to indicate that it can proceed with some
   * operation.
   */
  XRT_API_EXPORT
  std::cv_status
  wait(const std::chrono::milliseconds& timeout);

  /**
   * get_access_mode() - Return the mode of access for the fence
   *
   * See access mode.  Only process level fence objects can be
   * exported.
   */
  XRT_API_EXPORT
  access_mode
  get_access_mode() const;

  /**
   * get_next_state() - Return the next state of the fence
   *
   * The next state is the value that the fence will be set to when it
   * is signaled and it is the value that will be waited on when the
   * fence is submitted for wait.  The next state is incremented each
   * time the fence is signaled or waited on.
   */
  XRT_API_EXPORT
  uint64_t
  get_next_state() const;
};

} // namespace xrt

#else
# error xrt::fence is only implemented for C++
#endif // __cplusplus

#endif
