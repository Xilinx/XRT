/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef _XRT_PSKERNEL_H_
#define _XRT_PSKERNEL_H_

#include "ert.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_uuid.h"

#ifdef __cplusplus
# include "experimental/xrt_enqueue.h"
# include <chrono>
# include <cstdint>
# include <functional>
# include <memory>
# include <vector>
#endif

/**
 * typedef xrtPSKernelHandle - opaque kernel handle
 *
 * A kernel handle is obtained by opening a kernel.  Clients
 * pass this kernel handle to APIs that operate on a kernel.
 */
typedef void * xrtPSKernelHandle;

/**
 * typedef xrtPSRunHandle - opaque handle to a specific kernel run
 *
 * A run handle is obtained by running a kernel.  Clients
 * use a run handle to check or wait for kernel completion.
 */
typedef void * xrtPSRunHandle;  // NOLINT

#ifdef __cplusplus

namespace xrt {

class pskernel;
class event_impl;

/*!
 * @class psrun 
 *
 * @brief 
 * xrt::run represents one execution of a kernel
 *
 * @details
 * The run handle can be explicitly constructed from a kernel object
 * or implicitly constructed from starting a kernel execution.
 *
 * A run handle can be re-used to execute the same kernel again.
 */
class psrun_impl;
class psrun
{
 public:
  /**
   * psrun() - Construct empty run object
   *
   * Can be used as lvalue in assignment.
   */
  psrun() = default;

  /**
   * psrun() - Construct run object from a kernel object
   *
   * @param krnl: Kernel object representing the kernel to execute
   */
  XCL_DRIVER_DLLESPEC
  explicit
  psrun(const pskernel& krnl);

  /**
   * start() - Start one execution of a run.
   *
   * This function is asynchronous, run status must be expclicit checked
   * or ``wait()`` must be used to wait for the run to complete.
   */
  XCL_DRIVER_DLLESPEC
  void
  start();

  /**
   * stop() - Stop kernel run object at next safe iteration
   *
   * If the kernel run object has been started by specifying 
   * an iteration count or by specifying default iteration count, 
   * then this function can be used to stop the iteration early.  
   *
   * The function is synchronous and waits for the kernel
   * run object to complete.
   *
   * If the kernel is not iterating, then calling this funciton
   * is the same as calling ``wait()``.
   */
  XCL_DRIVER_DLLESPEC
  void
  stop();

  /**
   * wait() - Wait for a run to complete execution
   *
   * @param timeout  
   *  Timeout for wait (default block till run completes)
   * @return 
   *  Command state upon return of wait
   *
   * The default timeout of 0ms indicates blocking until run completes.
   *
   * The current thread will block until the run completes or timeout
   * expires. Completion does not guarantee success, the run status
   * should be checked by using ``state``.
   */
  XCL_DRIVER_DLLESPEC
  ert_cmd_state
  wait(const std::chrono::milliseconds& timeout = std::chrono::milliseconds{0}) const;

  /**
   * wait() - Wait for specified milliseconds for run to complete
   *
   * @param timeout_ms
   *  Timeout in milliseconds
   * @return
   *  Command state upon return of wait
   *
   * The default timeout of 0ms indicates blocking until run completes.
   *
   * The current thread will block until the run completes or timeout
   * expires. Completion does not guarantee success, the run status
   * should be checked by using ``state``.
   */
  ert_cmd_state
  wait(unsigned int timeout_ms) const
  {
    return wait(timeout_ms * std::chrono::milliseconds{1});
  }

  /**
   * state() - Check the current state of a run object
   *
   * @return 
   *  Current state of this run object
   *
   * The state values are defined in ``include/ert.h``
   */
  XCL_DRIVER_DLLESPEC
  ert_cmd_state
  state() const;

  /**
   * add_callback() - Add a callback function for run state
   *
   * @param state       State to invoke callback on
   * @param callback    Callback function 
   * @param data        User data to pass to callback function
   *
   * The function is called when the run object changes state to
   * argument state or any error state.  Only
   * ``ERT_CMD_STATE_COMPLETED`` is supported currently. 
   *
   * The function object's first parameter is a unique 'key'
   * for this xrt::run object implmentation on which the callback
   * was added. This 'key' can be used to identify an actual run
   * object that refers to the implementaion that is maybe shared
   * by multiple xrt::run objects.
   *
   * Any number of callbacks are supported.
   */
  XCL_DRIVER_DLLESPEC
  void
  add_callback(ert_cmd_state state,
               std::function<void(const void*, ert_cmd_state, void*)> callback,
               void* data);

  /// @cond
  /**
   * set_event() - Add event for enqueued operations
   *
   * @param event    
   *   Opaque implementation object
   *
   * This function is used when a run object is enqueued in an event
   * graph.  The event must be notified upon completion of the run.
   *
   * This is an experimental API using a WIP feature.
   */
  XCL_DRIVER_DLLESPEC
  void
  set_event(const std::shared_ptr<event_impl>& event) const;
  /// @endcond

  /**
   * operator bool() - Check if run handle is valid
   *
   * @return 
   *   True if run is associated with kernel object, false otherwise
   */
  explicit 
  operator bool() const
  {
    return handle != nullptr;
  }

  /**
   * operator < () - Weak ordering
   *
   * @param rhs
   *  Object to compare with
   * @return
   *  True if object is ordered less that compared with other
   */
  bool
  operator < (const xrt::psrun& rhs) const
  {
    return handle < rhs.handle;
  }

  /**
   * set_arg() - Set a specific kernel scalar argument for this run
   *
   * @param index
   *  Index of kernel argument to update
   * @param arg        
   *  The scalar argument value to set.
   * 
   * Use this API to explicit set or change a kernel argument prior
   * to starting kernel execution.  After setting arguments, the
   * kernel can be started using ``start()`` on the run object.
   *
   * See also ``operator()`` to set all arguments and start kernel.
   */
  template <typename ArgType>
  void
  set_arg(int index, ArgType&& arg)
  {
    set_arg_at_index(index, &arg, sizeof(arg));
  }

  /**
   * set_arg() - Set a specific kernel global argument for a run
   *
   * @param index
   *  Index of kernel argument to set
   * @param boh
   *  The global buffer argument value to set (lvalue).
   * 
   * Use this API to explicit set or change a kernel argument prior
   * to starting kernel execution.  After setting arguments, the
   * kernel can be started using ``start()`` on the run object.
   *
   * See also ``operator()`` to set all arguments and start kernel.
   */
  void
  set_arg(int index, xrt::bo& boh)
  {
    set_arg_at_index(index, boh);
  }

  /**
   * set_arg - xrt::bo variant for const lvalue
   */
  void
  set_arg(int index, const xrt::bo& boh)
  {
    set_arg_at_index(index, boh);
  }

  /**
   * set_arg - xrt::bo variant for rvalue
   */
  void
  set_arg(int index, xrt::bo&& boh)
  {
    set_arg_at_index(index, boh);
  }

  /**
   * operator() - Set all kernel arguments and start the run
   *
   * @param args
   *  Kernel arguments
   *
   * Use this API to explicitly set all kernel arguments and 
   * start kernel execution.
   */
  template<typename ...Args>
  void
  operator() (Args&&... args)
  {
    set_arg(0, std::forward<Args>(args)...);
    start();
  }

public:
  /// @cond
  const std::shared_ptr<psrun_impl>&
  get_handle() const
  {
    return handle;
  }

  // run() - Construct run object from a pimpl
  explicit
  psrun(std::shared_ptr<psrun_impl> impl)
    : handle(std::move(impl))
  {}

  // backdoor access to command packet
  XCL_DRIVER_DLLESPEC
  ert_packet*
  get_ert_packet() const;
  /// @endcond

private:
  std::shared_ptr<psrun_impl> handle;

  XCL_DRIVER_DLLESPEC
  void
  set_arg_at_index(int index, const void* value, size_t bytes);

  XCL_DRIVER_DLLESPEC
  void
  set_arg_at_index(int index, const xrt::bo&);

  template<typename ArgType, typename ...Args>
  void
  set_arg(int argno, ArgType&& arg, Args&&... args)
  {
    set_arg(argno, std::forward<ArgType>(arg));
    set_arg(++argno, std::forward<Args>(args)...);
  }
};
 

/*!
 * @class kernel
 *
 * A kernel object represents a set of instances matching a specified name.
 * The kernel is created by finding matching kernel instances in the 
 * currently loaded xclbin.
 *
 * Most interaction with kernel objects are through \ref xrt::run objects created 
 * from the kernel object to represent an execution of the kernel
 */
class pskernel_impl;
class pskernel
{
public:
  /**
   * cu_access_mode - compute unit access mode
   *
   * @var shared
   *  CUs can be shared between processes
   * @var exclusive
   *  CUs are owned exclusively by this process
   */
  enum class cu_access_mode : uint8_t { exclusive = 0, shared = 1, none = 2 };

  /**
   * pskernel() - Construct for empty kernel
   */
  pskernel() = default;

  /**
   * pskernel() - Constructor from a device and xclbin
   *
   * @param device
   *  Device on which the kernel should execute
   * @param xclbin_id
   *  UUID of the xclbin with the kernel
   * @param name
   *  Name of kernel to construct
   * @param mode
   *  Open the kernel instances with specified access (default shared)
   *
   * The kernel name must uniquely identify compatible kernel
   * instances (compute units).  Optionally specify which kernel
   * instance(s) to open using
   * "kernelname:{instancename1,instancename2,...}" syntax.  The
   * compute units are default opened with shared access, meaning that
   * other kernels and other process will have shared access to same
   * compute units.
   */
  XCL_DRIVER_DLLESPEC
    pskernel(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name,
         cu_access_mode mode = cu_access_mode::shared);

  /// @cond
  /// Deprecated construtor for exclusive access
  pskernel(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name, bool ex)
    : pskernel(device, xclbin_id, name, ex ? cu_access_mode::exclusive : cu_access_mode::shared)
  {}

  /**
   * Obsoleted construction from xclDeviceHandle
   */
  XCL_DRIVER_DLLESPEC
  pskernel(xclDeviceHandle dhdl, const xrt::uuid& xclbin_id, const std::string& name,
         cu_access_mode mode = cu_access_mode::shared);
  /// @endcond

  /**
   * operator() - Invoke the kernel function
   *
   * @param args
   *  Kernel arguments
   * @return 
   *  Run object representing this kernel function invocation
   */
  template<typename ...Args>
  psrun
  operator() (Args&&... args)
  {
    psrun r(*this);
    r(std::forward<Args>(args)...);
    return r;
  }

  /**
   * group_id() - Get the memory bank group id of an kernel argument
   *
   * @param argno
   *  The argument index
   * @return
   *  The memory group id to use when allocating buffers (see xrt::bo)
   *
   * The function throws if the group id is ambigious.
   */
  XCL_DRIVER_DLLESPEC
  int
  group_id(int argno) const;

  /**
   * offset() - Get the offset of kernel argument
   *
   * @param argno
   *  The argument index
   * @return
   *  The kernel register offset of the argument with specified index
   *
   * Use with ``read_register()`` and ``write_register()`` if manually
   * reading or writing kernel registers for explicit arguments. 
   */
  XCL_DRIVER_DLLESPEC
  uint32_t
  offset(int argno) const;

  /**
   * write() - Write to the address range of a kernel
   *
   * @param offset
   *  Offset in register space to write to
   * @param data     
   *  Data to write
   *
   * Throws std::out_or_range if offset is outside the
   * kernel address space
   *
   * The kernel must be associated with exactly one kernel instance 
   * (compute unit), which must be opened for exclusive access.
   */
  XCL_DRIVER_DLLESPEC
  void
  write_register(uint32_t offset, uint32_t data);

  /**
   * read() - Read data from kernel address range
   *
   * @param offset  
   *  Offset in register space to read from
   * @return 
   *  Value read from offset
   *
   * Throws std::out_or_range if offset is outside the
   * kernel address space
   *
   * The kernel must be associated with exactly one kernel instance 
   * (compute unit), which must be opened for exclusive access.
   */
  XCL_DRIVER_DLLESPEC
  uint32_t
  read_register(uint32_t offset) const;

public:
  /// @cond
  const std::shared_ptr<pskernel_impl>&
  get_handle() const
  {
    return handle;
  }
  /// @endcond

private:
  std::shared_ptr<pskernel_impl> handle;
};

/// @cond
// Specialization from xrt_enqueue.h for run objects, which
// are asynchronous waitable objects.
template <>
struct callable_traits<psrun>
{
  enum { is_async = true };
};
/// @endcond

} // namespace xrt

/// @cond
extern "C" {
#endif

/**
 * xrtPSKernelOpen() - Open a PS kernel and obtain its handle.
 *
 * @deviceHandle:  Handle to the device with the kernel
 * @xclbinId:      The uuid of the xclbin with the specified kernel.
 * @name:          Name of kernel to open.
 * Return:         Handle representing the opened kernel.
 *
 * The kernel name must uniquely identify compatible kernel instances
 * (compute units).  Optionally specify which kernel instance(s) to
 * open using "kernelname:{instancename1,instancename2,...}" syntax.
 * The compute units are opened with shared access, meaning that 
 * other kernels and other process will have shared access to same
 * compute units.  If exclusive access is needed then open the 
 * kernel using @xrtPLKernelOpenExclusve().
 *
 * An xclbin with the specified kernel must have been loaded prior
 * to calling this function. An XRT_NULL_HANDLE is returned on error
 * and errno is set accordingly.
 *
 * A kernel handle is thread safe and can be shared between threads.
 */
XCL_DRIVER_DLLESPEC
xrtPSKernelHandle
xrtPSKernelOpen(xrtDeviceHandle deviceHandle, const xuid_t xclbinId, const char *name);

/**
 * xrtPSKernelOpenExclusive() - Open a PL kernel and obtain its handle.
 *
 * @deviceHandle:  Handle to the device with the kernel
 * @xclbinId:      The uuid of the xclbin with the specified kernel.
 * @name:          Name of kernel to open.
 * Return:         Handle representing the opened kernel.
 *
 * Same as @xrtPSKernelOpen(), but opens compute units with exclusive
 * access.  Fails if any compute unit is already opened with either
 * exclusive or shared access.
 */
XCL_DRIVER_DLLESPEC
xrtPSKernelHandle
xrtPSKernelOpenExclusive(xrtDeviceHandle deviceHandle, const xuid_t xclbinId, const char *name);

/**
 * xrtPSKernelClose() - Close an opened kernel
 *
 * @kernelHandle: Handle to kernel previously opened with xrtKernelOpen
 * Return:        0 on success, -1 on error
 */
XCL_DRIVER_DLLESPEC
int
xrtPSKernelClose(xrtPSKernelHandle kernelHandle);

/**
 * xrtPSKernelRun() - Start a kernel execution
 *
 * @kernelHandle: Handle to the kernel to run
 * @...:          Kernel arguments
 * Return:        Run handle which must be closed with xrtPSRunClose()
 *
 * A run handle is specific to one execution of a kernel.  Once
 * execution completes, the run handle can be re-used to execute the
 * same kernel again.  When no longer needed, then run handle must be
 * closed with xrtRunClose().
 */
XCL_DRIVER_DLLESPEC
xrtPSRunHandle
xrtPSKernelRun(xrtPSKernelHandle kernelHandle, ...);

/**
 * xrtPSRunOpen() - Open a new run handle for a kernel without starting kernel
 *
 * @kernelHandle: Handle to the kernel to associate the run handle with
 * Return:        Run handle which must be closed with xrtRunClose()
 *
 * The handle can be used repeatedly to start an execution of the
 * associated kernel.  This API allows application to manage run
 * handles without maintaining corresponding kernel handle.
 */
XCL_DRIVER_DLLESPEC
xrtPSRunHandle
xrtPSRunOpen(xrtPSKernelHandle kernelHandle);

/**
 * xrtPSRunSetArg() - Set a specific kernel argument for this run
 *
 * @rhdl:       Handle to the run object to modify
 * @index:      Index of kernel argument to set
 * @...:        The argument value to set.
 * Return:      0 on success, -1 on error
 *
 * Use this API to explicitly set specific kernel arguments prior
 * to starting kernel execution.  After setting all arguments, the
 * kernel execution can be start with xrtRunStart()
 */
XCL_DRIVER_DLLESPEC
int
xrtPSRunSetArg(xrtPSRunHandle rhdl, int index, ...);

/**
 * xrtPSRunStart() - Start existing run handle
 *
 * @rhdl:       Handle to the run object to start
 * Return:      0 on success, -1 on error
 *
 * Use this API when re-using a run handle for more than one execution
 * of the kernel associated with the run handle.
 */
XCL_DRIVER_DLLESPEC
int
xrtPSRunStart(xrtPSRunHandle rhdl);

/**
 * xrtPSRunWait() - Wait for a run to complete
 *
 * @rhdl:       Handle to the run object to start
 * Return:      Run command state for completed run,
 *              or ERT_CMD_STATE_ABORT on error
 *
 * Blocks current thread until job has completed
 */
XCL_DRIVER_DLLESPEC
enum ert_cmd_state
xrtPSRunWait(xrtPSRunHandle rhdl);

/**
 * xrtPSRunWait() - Wait for a run to complete
 *
 * @rhdl:       Handle to the run object to start
 * @timeout_ms: Timeout in millisecond
 * Return:      Run command state for completed run, or
 *              current status if timeout.
 *
 * Blocks current thread until job has completed
 */
XCL_DRIVER_DLLESPEC
enum ert_cmd_state
xrtPSRunWaitFor(xrtPSRunHandle rhdl, unsigned int timeout_ms);

/**
 * xrtPSRunState() - Check the current state of a run
 *
 * @rhdl:       Handle to check
 * Return:      The underlying command execution state per ert.h
 */
XCL_DRIVER_DLLESPEC
enum ert_cmd_state
xrtPSRunState(xrtPSRunHandle rhdl);

/**
 * xrtPSRunSetCallback() - Set a callback function
 *
 * @rhdl:        Handle to set callback on
 * @state:       State to invoke callback on
 * @callback:    Callback function 
 * @data:        User data to pass to callback function
 *
 * Register a run callback function that is invoked when the
 * run changes underlying execution state to specified state.
 * Support states are: ERT_CMD_STATE_COMPLETED (to be extended)
 */
XCL_DRIVER_DLLESPEC
int
xrtPSRunSetCallback(xrtPSRunHandle rhdl, enum ert_cmd_state state,
                  void (* callback)(xrtPSRunHandle, enum ert_cmd_state, void*),
                  void* data);

/**
 * xrtPSRunClose() - Close a run handle
 *
 * @rhdl:  Handle to close
 * Return:      0 on success, -1 on error
 */
XCL_DRIVER_DLLESPEC
int
xrtPSRunClose(xrtPSRunHandle rhdl);

/// @endcond

#ifdef __cplusplus
}
#endif

#endif
