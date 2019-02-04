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
#include <string.h>
#include <stdio.h>
#include "xclhal2.h"
#include "xmaplugin.h"

#define XMA_ADDR_AP_CTRL 0x00   /* AXI-Lite control signals */
#define XMA_ADDR_AP_GIER 0x04   /* Global Interrupt enable register */
#define XMA_ADDR_AP_IIER 0x08   /* IP Interrupt enable register */
#define XMA_ADDR_AP_IISR 0x0C   /* IP Interrupt status register */ 

#define XMA_AP_CTRL_START 0x01  /* AXI-Lite control start bit */
#define XMA_AP_CTRL_DONE  0x02  /* AXI-Lite control done bit */
#define XMA_AP_CTRL_IDLE  0x04  /* AXI-Lite control idle bit */
#define XMA_AP_CTRL_READY 0x08  /* AXI-Lite control ready bit */

XmaBufferHandle
xma_plg_buffer_alloc(XmaHwSession s_handle, size_t size)
{
    XmaBufferHandle handle;
    xclDeviceHandle dev_handle = s_handle.dev_handle;
    uint32_t ddr_bank = s_handle.ddr_bank;

#if 0
    printf("xma_plg_buffer_alloc dev_handle = %p\n", dev_handle);
    printf("xma_plg_buffer_alloc size = %lu\n", size);
    printf("xma_plg_buffer_alloc ddr_bank = %u\n", ddr_bank);
#endif
    handle = xclAllocBO(dev_handle, size, XCL_BO_DEVICE_RAM, ddr_bank);
#if 0
    printf("xma_plg_buffer_alloc handle = %d\n", handle);
#endif
    return handle;
}

void
xma_plg_buffer_free(XmaHwSession s_handle, XmaBufferHandle b_handle)
{
#if 0
    printf("xma_plg_buffer_free called\n");
#endif
    xclDeviceHandle dev_handle = s_handle.dev_handle;
    xclFreeBO(dev_handle, b_handle);
}

uint64_t
xma_plg_get_paddr(XmaHwSession s_handle, XmaBufferHandle b_handle)
{
    uint64_t paddr;
    xclDeviceHandle dev_handle = s_handle.dev_handle;
    paddr = xclGetDeviceAddr(dev_handle, b_handle);
#if 0
    printf("xma_plg_get_paddr b_handle = %d, paddr = %lx\n", b_handle, paddr);
#endif
    return paddr;
}

int32_t
xma_plg_buffer_write(XmaHwSession s_handle,
                     XmaBufferHandle  b_handle,
                     const void      *src,
                     size_t           size,
                     size_t           offset)
{
    int32_t rc;

    //printf("xma_plg_buffer_write called\n");

    xclDeviceHandle dev_handle = s_handle.dev_handle;

    //printf("xma_plg_buffer_write b_handle=%d,src=%p,size=%lu,offset=%lx\n", b_handle, src, size, offset);

    rc = xclWriteBO(dev_handle, b_handle, src, size, offset);
    if (rc != 0)
        printf("xclWriteBO failed %d\n", rc);

    rc = xclSyncBO(dev_handle, b_handle, XCL_BO_SYNC_BO_TO_DEVICE, size, offset);
    if (rc != 0)
        printf("xclSyncBO failed %d\n", rc);

    return rc;
}

int32_t
xma_plg_buffer_read(XmaHwSession s_handle,
                    XmaBufferHandle  b_handle,
                    void            *dst,
                    size_t           size,
                    size_t           offset)
{
    int32_t rc;

    //printf("xma_plg_buffer_read called\n");

    xclDeviceHandle dev_handle = s_handle.dev_handle;

    //printf("xma_plg_buffer_read b_handle=%d,dst=%p,size=%lu,offset=%lx\n",
    //       b_handle, dst, size, offset);

    rc = xclSyncBO(dev_handle, b_handle, XCL_BO_SYNC_BO_FROM_DEVICE,
                   size, offset);
    if (rc != 0)
    {
        printf("xclSyncBO failed %d\n", rc);
        return rc;
    }

    rc = xclReadBO(dev_handle, b_handle, dst, size, offset);
    if (rc != 0)
        printf("xclReadBO failed %d\n", rc);


    return rc;
}

int32_t
xma_plg_register_prep_write(XmaHwSession  s_handle,
                       void         *src,
                       size_t        size,
                       size_t        offset)
{
    uint8_t *dst = (uint8_t*)s_handle.reg_map;
    size_t   cur_min = offset; 
    size_t   cur_max = offset + size; 

    dst += offset;
    memcpy(dst, src, size);

    if (cur_max > s_handle.max_offset)
        s_handle.max_offset = cur_max;

    if (s_handle.min_offset == 0)
        s_handle.min_offset = cur_min;
        
    if (cur_min < s_handle.min_offset)
        s_handle.min_offset = cur_min;

    return 0;
}

void xma_plg_kernel_lock(XmaHwSession s_handle)
{
    /* Only acquire the lock if we don't already own it */
    if (s_handle.have_lock)
        return;

    pthread_mutex_lock(s_handle.lock);
    s_handle.have_lock = true;
}

void xma_plg_kernel_unlock(XmaHwSession s_handle)
{
    if (s_handle.have_lock)
    {
        pthread_mutex_unlock(s_handle.lock);
        s_handle.have_lock = false;
    }
}

void xma_plg_kernel_wait_on_finish(XmaHwSession s_handle)
{
    uint32_t val = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    while((val & XMA_AP_CTRL_IDLE) != XMA_AP_CTRL_IDLE)
    {
        xma_plg_register_read(s_handle, &val, sizeof(val), XMA_ADDR_AP_CTRL);
    }
#pragma GCC diagnostic pop
}

void xma_plg_kernel_start(XmaHwSession s_handle)
{
    
    uint32_t val = XMA_AP_CTRL_START;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    xma_plg_register_write(s_handle, &val, sizeof(val), XMA_ADDR_AP_CTRL);
#pragma GCC diagnostic pop
}

int32_t
xma_plg_kernel_exec(XmaHwSession s_handle, bool wait_on_kernel_finish)
{
    void   *src = s_handle.reg_map;
    size_t  offset = s_handle.min_offset;
    size_t  size = s_handle.max_offset;
    
    /* The lock is only acquired if the session does not already own it */
    xma_plg_kernel_lock(s_handle);

    /* Must wait for the kernel to be idle before configuring it */
    xma_plg_kernel_wait_on_finish(s_handle);

    /* Write the kernel registers */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    xma_plg_register_write(s_handle, src, size, offset);
#pragma GCC diagnostic pop

    /* Start the kernel */
    xma_plg_kernel_start(s_handle);

    /* Only wait for the kernel to complete if the caller explicitly requests */
    if (wait_on_kernel_finish)
    {
        /* Block, waiting for the IDLE status */
        xma_plg_kernel_wait_on_finish(s_handle);

        /* Can only unlock if we've finished using the kernel */
        xma_plg_kernel_unlock(s_handle);
    }

    return 0;
}
    
int32_t
xma_plg_register_write(XmaHwSession  s_handle,
                       void         *src,
                       size_t        size,
                       size_t        offset)
{
    xclDeviceHandle dev_handle = s_handle.dev_handle;
    uint64_t        dev_offset = s_handle.base_address;
    //printf("xma_plg_register_write dev_handle=%p,src=%p,size=%lu,offset=%lx\n", dev_handle, src, size, offset);
    return xclWrite(dev_handle, XCL_ADDR_KERNEL_CTRL, dev_offset + offset,
                    src, size);
}

int32_t
xma_plg_register_read(XmaHwSession  s_handle,
                      void         *dst,
                      size_t        size,
                      size_t        offset)
{
    xclDeviceHandle dev_handle = s_handle.dev_handle;
    uint64_t        dev_offset = s_handle.base_address;
#if 0
    printf("xma_plg_register_read dev_handle=%p,dst=%p,size=%lu,offset=%lx\n",
            dev_handle, dst, size, offset);
#endif
    return xclRead(dev_handle, XCL_ADDR_KERNEL_CTRL, dev_offset + offset,
                   dst, size);
}

void
xma_plg_register_dump(XmaHwSession s_handle,
                      int32_t      num_words)
{
    printf("Printing Register Map Dump\n");
    for (int32_t i = 0; i < num_words; i++)
    {
        uint32_t value;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        xma_plg_register_read(s_handle, &value, sizeof(value), i*4);
#pragma GCC diagnostic pop
        printf("0x%08X\t\t0x%08X\n", i*4, value);
    }
}
