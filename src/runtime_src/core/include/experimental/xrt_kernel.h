/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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

#ifndef _XRT_KERNEL_H_
#define _XRT_KERNEL_H_

#include "xrt.h"
#include "ert.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * typedef xrtDeviceHandle - opaque device handle
 *
 * Typedef alias from xrt.h
 */
typedef xclDeviceHandle xrtDeviceHandle;
typedef xclBufferHandle xrtBufferHandle;

/**
 * typedef xrtKernelHandle - opaque kernel handle
 *
 * A kernel handle is obtained by opening a kernel.  Clients
 * pass this kernel handle to APIs that operate on a kernel.
 */
typedef void * xrtKernelHandle;

/**
 * typedef xrtRunHandle - opaque handle to a specific kernel run
 *
 * A run handle is obtained by running a kernel.  Clients
 * use a run handle to check or wait for kernel completion.
 */
typedef void * xrtRunHandle;

/**
 * xrtPLKernelOpen() - Open a PL kernel and obtain its handle.
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
xrtKernelHandle
xrtPLKernelOpen(xrtDeviceHandle deviceHandle, const xuid_t xclbinId, const char *name);

/**
 * xrtPLKernelOpenExclusive() - Open a PL kernel and obtain its handle.
 *
 * Same as @xrtPLKernelOpen(), but opens compute units with exclusive
 * access.  Fails if any compute unit is already opened with either
 * exclusive or shared access.
 */
XCL_DRIVER_DLLESPEC
xrtKernelHandle
xrtPLKernelOpenExclusive(xrtDeviceHandle deviceHandle, const xuid_t xclbinId, const char *name);

/**
 * xrtKernelClose() - Close an opened kernel
 *
 * @kernelHandle: Handle to kernel previously opened with xrtKernelOpen
 * Return:        0 on success, -1 on error
 */
XCL_DRIVER_DLLESPEC
int
xrtKernelClose(xrtKernelHandle kernelHandle);

/**
 * xrtKernelRun() - Start a kernel execution
 *
 * @kernelHandle: Handle to the kernel to run
 * @args:         Kernel arguments
 * Return:        Run handle which must be closed with xrtRunClose()
 *
 * A run handle is specific to one execution of a kernel.  Once
 * execution completes, the run handle can be re-used to execute the
 * same kernel again.  When no longer needed, then run handle must be
 * closed with xrtRunClose().
 */
XCL_DRIVER_DLLESPEC
xrtRunHandle
xrtKernelRun(xrtKernelHandle kernelHandle, ...);

/**
 * xrtRunOpen() - Open a new run handle for a kernel without starting kernel
 *
 * @kernelHandle: Handle to the kernel to associate the run handle with
 * Return:        Run handle which must be closed with xrtRunClose()
 *
 * The handle can be used repeatedly to start an execution of the
 * associated kernel.  This API allows application to manage run
 * handles without maintaining corresponding kernel handle.
 */
XCL_DRIVER_DLLESPEC
xrtRunHandle
xrtRunOpen(xrtKernelHandle kernelHandle);

/**
 * xrtRunSetArg() - Set a specific kernel argument for this run
 *
 * @runHandle:  Handle to the run object to modify
 * @index:      Index of kernel argument to set
 * @arg:        The argument value to set.
 * Return:      0 on success, -1 on error
 *
 * Use this API to explicitly set specific kernel arguments prior
 * to starting kernel execution.  After setting all arguments, the
 * kernel execution can be start with xrtRunStart()
 */
XCL_DRIVER_DLLESPEC
int
xrtRunSetArg(xrtRunHandle runHandle, int index, ...);

/**
 * xrtRunUpdateArg() - Asynchronous update of kernel argument
 *
 * @runHandle:  Handle to the run object to modify
 * @index:      Index of kernel argument to update
 * @arg:        The argument value to set.
 * Return:      0 on success, -1 on error
 *
 * Use this API to asynchronously update a specific kernel
 * argument of an existing run.  
 *
 * This API is only supported on Edge.
 */  
XCL_DRIVER_DLLESPEC
int
xrtRunUpdateArg(xrtRunHandle rhdl, int index, ...);

/**
 * xrtRunStart() - Start existing run handle
 *
 * @runHandle:  Handle to the run object to start
 * Return:      0 on success, -1 on error
 *
 * Use this API when re-using a run handle for more than one execution
 * of the kernel associated with the run handle.
 */
XCL_DRIVER_DLLESPEC
int
xrtRunStart(xrtRunHandle runHandle);

/**
 * xrtRunWait() - Wait for a run to complete
 *
 * @runHandle:  Handle to the run object to start
 * Return:      Run command state for completed run, or
 *              ERT_CMD_STATE_ABORT on error
 *
 * Blocks current thread until job has completed
 */
XCL_DRIVER_DLLESPEC
ert_cmd_state
xrtRunWait(xrtRunHandle runHandle);

/**
 * xrtRunState() - Check the current state of a run
 *
 * @runHandle:  Handle to check
 * Return:      The underlying command execution state per ert.h
 */
XCL_DRIVER_DLLESPEC
ert_cmd_state
xrtRunState(xrtRunHandle runHandle);

/**
 * xrtRunSetCallback() - Set a callback function
 *
 * @runHandle:   Handle to set callback on
 * @state:       State to invoke callback on
 * @callback:    Callback function 
 * @userdata:    User data to pass to callback function
 *
 * Register a run callback function that is invoked when the
 * run changes underlying execution state to specified state.
 * Support states are: ERT_CMD_STATE_COMPLETED (to be extended)
 */
XCL_DRIVER_DLLESPEC
int
xrtRunSetCallback(xrtRunHandle runHandle, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtRunHandle, ert_cmd_state, void*),
                  void* data);

/**
 * xrtRunClose() - Close a run handle
 *
 * @runHandle:  Handle to close
 * Return:      0 on success, -1 on error
 */
XCL_DRIVER_DLLESPEC
int
xrtRunClose(xrtRunHandle runHandle);

#ifdef __cplusplus
}
#endif

#endif
