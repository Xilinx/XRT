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
#include "lib/xmahw.h"
#include "lib/xmahw_hal.h"
#include "plg/xmasess.h"
#include "plg/xmadecoder.h"
#include "plg/xmaencoder.h"
#include "plg/xmascaler.h"
#include "plg/xmafilter.h"
#include "plg/xmakernel.h"

/**
 * DOC: XMA Plugin Interface
 * The interface used by XMA kernel plugin developers
*/

/**
 * @ingroup xma_plg_intf xmaplugin
 * @file xmaplugin.h
 * Primary header file providing access to complete XMA plugin interface
 */

/**
 * @ingroup xma_plg_intf
 * @addtogroup xmaplugin xmaplugin.h
 * @{
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t  XmaBufferHandle;

/**
 *  xma_plg_buffer_alloc)() - Allocate device memory
 *
 *  This function allocates memory on the FPGA DDR and
 *  provides a handle to the memory that can be used for
 *  copying data from the host to device memory or from
 *  the device to the host.  In addition, the handle
 *  can be passed to the function @ref xma_plg_get_paddr()
 *  in order to obtain the physical address of the buffer.
 *  Obtaining the physical address is necessary for setting
 *  the AXI register map with physical pointers so that the
 *  kernel knows where key input and output buffers are located.
 *
 *  This function knows which DDR bank is associated with this
 *  session and therefore automatically selects the correct
 *  DDR bank.
 *
 *  @s_handle: The session handle associated with this plugin instance.
 *  @size:     Size in bytes of the device buffer to be allocated.
 *
 *  RETURN:    Non-zero buffer handle on success
 *
 */
XmaBufferHandle xma_plg_buffer_alloc(XmaHwSession s_handle, size_t size);

/**
 *  xma_plg_buffer_free() - Free a device buffer
 *
 *  This function frees a previous allocated buffer that was obtained
 *  using the @ref xma_plg_buffer_alloc() function.
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @b_handle:  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *
 */
void xma_plg_buffer_free(XmaHwSession s_handle, XmaBufferHandle b_handle);

/**
 *  xma_plg_buffer_free() - Get a physical address for a buffer handle
 *
 *  This function returns the physical address of DDR memory on the FPGA
 *  used by a specific session
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @b_handle:  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *
 *  RETURN:          Physical address of DDR on the FPGA
 *
 */
uint64_t xma_plg_get_paddr(XmaHwSession s_handle, XmaBufferHandle b_handle);

/**
 *  xma_plg_get_paddr() - Write data from host to device buffer
 *
 *  This function copies data from host to memory to device memory.
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @b_handle:  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *  @src:       Source data pointer
 *  @size:      Size of data to copy
 *  @offset:    Offset from the beginning of the allocated device memory
 *
 *  RETURN:     XMA_SUCCESS on success
 * XMA_ERROR on failure
 *
 */
int32_t xma_plg_buffer_write(XmaHwSession     s_handle,
                             XmaBufferHandle  b_handle,
                             const void      *src,
                             size_t           size,
                             size_t           offset);

/**
 *  xma_plg_buffer_read() - Read data from device memory and copy to host memory
 *
 *  This function copies data from device memory and stores the result in
 *  the requested host memory
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @b_handle:  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *  @dst:       Destination data pointer
 *  @size:      Size of data to copy
 *  @offset:    Offset from the beginning of the allocated device memory
 *
 *  RETURN:     XMA_SUCCESS on success
 * XMA_ERROR on failure
 *
 */
int32_t xma_plg_buffer_read(XmaHwSession     s_handle,
                            XmaBufferHandle  b_handle,
                            void            *dst,
                            size_t           size,
                            size_t           offset);

/**
 *  xma_plg_register_write() - Write kernel register(s)
 *
 *  This function writes the data provided and sets the specified AXI_Lite
 *  register(s) exposed by a kernel. The base offset of 0 is the beginning
 *  of the kernels AXI_Lite memory map as this function adds the required
 *  offsets internally for the kernel and PCIe.
 *
 *  Note: Do not use this API. Instead use below mentioned xma_plg_ebo_kernel* APIs
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @dst:       Destination data pointer
 *  @size:      Size of data to copy
 *  @offset:    Offset from the beginning of the kernel AXI_Lite register
 *                   register map
 *
 * RETURN:      >=0 number of bytes written
 * <0 on failure
 *
 */
int32_t xma_plg_register_write(XmaHwSession     s_handle,
                               void            *dst,
                               size_t           size,
                               size_t           offset) __attribute__ ((deprecated));

/**
 *  xma_plg_ebo_kernel_start() - execBO based kernel APIs
 *  xma_plg_register_write should NOT be used.
 *  Please use below APIs instead
 *  
 */
int32_t xma_plg_ebo_kernel_start(XmaHwSession  s_handle, uint32_t* args, uint32_t args_size);

/**
 *  xma_plg_ebo_kernel_done() - execBO based kernel APIs. Wait for all pending kernel commands to finish
 *  xma_plg_register_write should NOT be used.
 *  Please use below APIs instead
 *
 */
int32_t xma_plg_ebo_kernel_done(XmaHwSession  s_handle);//Wait for all pending kernel commands to finish



/**
 *  xma_plg_register_read() - Read kernel registers
 *
 *  This function reads the register(s) exposed by the kernel. The base offset
 *  of 0 is the beginning of the kernels AXI_Lite memory map as this function
 *  adds the required offsets internally for the kernel and PCIe.
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @dst:       Destination data pointer
 *  @size:      Size of data to copy
 *  @offset:    Offset from the beginning of the kernel's AXI_Lite memory
 *                   map
 *
 *  RETURN:     >=0 number of bytes read
 * <0 on failure
 *
 */
int32_t xma_plg_register_read(XmaHwSession     s_handle,
                              void            *dst,
                              size_t           size,
                              size_t           offset) __attribute__ ((deprecated));

/**
 *  xma_plg_register_dump() - Dump kernel registers
 *
 *  This function dumps the registers for a kernel up to the number of words
 *  specified and prints them with the offset and value.
 *
 *  @s_handle:  The session handle associated with this plugin instance
 *  @num_words: Number of 32-bit words to dump
 *
 */
void xma_plg_register_dump(XmaHwSession     s_handle,
                           int32_t          num_words);

#ifdef __cplusplus
}
#endif

#endif
