/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
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
#ifndef _XMA_PLUGIN_H_
#define _XMA_PLUGIN_H_

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include <stddef.h>
#include "xma.h"
#include "plg/xmasess.h"
#include "plg/xmadecoder.h"
#include "plg/xmaencoder.h"
#include "plg/xmascaler.h"
#include "plg/xmafilter.h"
#include "plg/xmakernel.h"
#include "plg/xmaadmin.h"

/**
 * DOC: XMA Plugin Interface
 * The interface used by XMA kernel plugin developers
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  xma_plg_buffer_alloc)() - Allocate device memory
 *  This function allocates memory on the FPGA DDR and
 *  provides a BufferObject to the memory that can be used for
 *  copying data from the host to device memory or from
 *  the device to the host. 
 *  BufferObject contains paddr, device index, ddr bank, etc.  
 *  paddr (the physical address) is necessary for setting
 *  the AXI register map with physical pointers so that the
 *  kernel knows where key input and output buffers are located.
 *  This function knows which DDR bank is associated with this
 *  session and therefore automatically selects the correct
 *  DDR bank.
 *
 *  @s_handle: The session handle associated with this plugin instance.
 *  @size:     Size in bytes of the device buffer to be allocated.
 *  @device_only_buffer: Allocate device only buffer without any host space
 *  @return_code:  XMA_SUCESS or XMA_ERROR.
 *
 *  RETURN:    BufferObject on success;
 *
 */
XmaBufferObj xma_plg_buffer_alloc(XmaSession s_handle, size_t size, bool device_only_buffer, int32_t* return_code);

/**
 *  xma_plg_buffer_alloc_arg_num() - Allocate device memory
 *  This function allocates memory on the FPGA DDR bank 
 *  connected to the supplied kernel argument number and
 *  provides a BufferObject to the memory that can be used for
 *  copying data from the host to device memory or from
 *  the device to the host. 
 *  BufferObject contains paddr, device index, ddr bank, etc.  
 *  paddr (the physical address) is necessary for setting
 *  the AXI register map with physical pointers so that the
 *  kernel knows where key input and output buffers are located.
 *  This function knows which DDR bank is associated with this
 *  session and therefore automatically selects the correct
 *  DDR bank.
 *
 *  @s_handle: The session handle associated with this plugin instance.
 *  @size:     Size in bytes of the device buffer to be allocated.
 *  @device_only_buffer: Allocate device only buffer without any host space
 *  @arg_num: kernel argumnet num. Buffer is allocated on DDR bank connected to this kernel argument
 *  @return_code:  XMA_SUCESS or XMA_ERROR.
 *
 *  RETURN:    BufferObject on success;
 *
 */
XmaBufferObj xma_plg_buffer_alloc_arg_num(XmaSession s_handle, size_t size, bool device_only_buffer, int32_t arg_num, int32_t* return_code);


/**
 *  xma_plg_buffer_free() - Free a device buffer
 *  This function frees a previous allocated buffer that was obtained
 *  using the @ref xma_plg_buffer_alloc() function.
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @b_obj:  The BufferObject returned from
 *                   @ref xma_plg_buffer_alloc()
 *
 */
void xma_plg_buffer_free(XmaSession s_handle, XmaBufferObj b_obj);

/**
 *  xma_plg_buffer_write() - Write data from host to device buffer
 *  This function copies data from host memory to device memory.
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @b_obj:  The BufferObject returned from
 *                   @ref xma_plg_buffer_alloc()
 *  @size:      Size of data to copy
 *  @offset:    Offset from the beginning of the allocated device memory
 *
 *  RETURN:     XMA_SUCCESS on success
 * XMA_ERROR on failure
 *
 */
int32_t xma_plg_buffer_write(XmaSession     s_handle,
                             XmaBufferObj  b_obj,
                             size_t           size,
                             size_t           offset);

/**
 *  xma_plg_buffer_read() - Read data from device memory and copy to host memory
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @b_obj:  The BufferObject returned from
 *                   @ref xma_plg_buffer_alloc()
 *  @size:      Size of data to copy
 *  @offset:    Offset from the beginning of the allocated device memory
 *
 *  RETURN:     XMA_SUCCESS on success
 * XMA_ERROR on failure
 *
 */
int32_t xma_plg_buffer_read(XmaSession     s_handle,
                            XmaBufferObj  b_obj,
                            size_t           size,
                            size_t           offset);

/**
 *  xma_plg_channel_id() - Query channel_id assigned to this plugin session
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *
 *  RETURN:     Assigned channel_id on success
 * XMA_ERROR on failure
 *
 */
int32_t xma_plg_channel_id(XmaSession     s_handle);

/**
 * xma_plg_schedule_work_item() - This function schedules a request to the XRT
 * scheduler for execution of a kernel based on the saved state of the kernel registers
 * supplied by the xma_plg_register_prep_write() function call.  The prep_write() keeps a
 * shadow register map so that the schedule_work_item() can gather all registers
 * and push a new work item onto the scheduler queue.  Work items are processed
 * in FIFO order.  After calling schedule_work_item() one or more times, the caller
 * can invoke xma_plg_is_work_item_done() to wait for one item of work to complete.
 * Register map must be locked with xma_plg_kernel_lock_regmap used before this call
 *
 * @s_handle: The session handle associated with this plugin instance
 *
 * RETURN:     XMA_SUCCESS on success
 *
 * XMA_ERROR on failure
 *
 */
int32_t xma_plg_schedule_work_item(XmaSession s_handle);

/**
 * xma_plg_is_work_item_done() - This function checks if at least one work item
 * previously submitted via xma_plg_schedule_work_item() has completed.  If the
 * supplied timeout expires before a work item has completed, this function
 * returns an error.
 *
 * @s_handle:      The session handle associated with this plugin instance
 * @timeout_in_ms: A timeout value in milliseconds
 *
 * RETURN:         XMA_SUCCESS on success
 *
 * XMA_ERROR on timeout
 *
 */
int32_t xma_plg_is_work_item_done(XmaSession s_handle, int32_t timeout_in_ms);

/**
 * xma_plg_kernel_lock_regmap() - This function acquires register map lock
 * so that prep_write & schedule_work_items can be called
 *
 * @s_handle:      The session handle associated with this plugin instance
 *
 * RETURN:         XMA_SUCCESS or XMA_ERROR
 *
 */
int32_t xma_plg_kernel_lock_regmap(XmaSession s_handle);

/**
 * xma_plg_kernel_unlock_regmap() - This function releases register map lock
 * so that other plugins can share same CU
 *
 * @s_handle:      The session handle associated with this plugin instance
 *
 * RETURN:         XMA_SUCCESS or XMA_ERROR
 *
 */
int32_t xma_plg_kernel_unlock_regmap(XmaSession s_handle);

/**
 *  xma_plg_register_prep_write() - This function writes the data provided and sets
 * the specified AXI_Lite register(s) exposed by a kernel. The base offset of 0
 * is the beginning of the kernels AXI_Lite memory map as this function adds the required
 * offsets internally for the kernel and PCIe.
 * Register map must be locked with xma_plg_kernel_lock_regmap used before this call
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @dst:       Destination data pointer
 *  @size:      Size of data to copy
 *  @offset:    Offset from the beginning of the kernel AXI_Lite register
 *                   register map
 *
 *  RETURN:          >=0 number of bytes written
 *
 * <0 on failure
 *
 */
int32_t xma_plg_register_prep_write(XmaSession     s_handle,
                                    void            *dst,
                                    size_t           size,
                                    size_t           offset);

/**
 * xma_plg_schedule_work_item_with_args() - This function schedules a request to the XRT
 * scheduler for execution of a kernel based on the supplied kernel register map
 * Work items are processed
 * in FIFO order. For dataflow kernels with channels work items for a given channel are processed in FIFO order. 
 * After calling xma_plg_schedule_work_item_with_args() one or more times, the caller
 * can invoke xma_plg_is_work_item_done() to wait for one item of work to complete.
 * pointer to regamp contains aruments to be supplied to kernel
 * regmap must start from offset 0 of register map of a kernel
 *
 * Note: register map lock is not required before this call. 
 * So xma_plg_kernel_lock_regmap is not required before this call
 * 
 * @s_handle: The session handle associated with this plugin instance
 *  @regmap:    pointer to register map to use for kernel arguments. regmap must start from offset 0 of register map of a kernel
 *  @regmap_size:   Size of above regmap (in bytes) to copy
 *
 * RETURN:     XMA_SUCCESS on success
 *
 * XMA_ERROR on failure
 *
 */
int32_t xma_plg_schedule_work_item_with_args(XmaSession s_handle,
                                 void            *regmap,
                                 int32_t          regmap_size);



#ifdef __cplusplus
}
#endif

#endif
