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
#include <stdio.h>
#include "xclhal2.h"
#include "xmaplugin.h"

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
        xma_plg_register_read(s_handle, &value, sizeof(value), i*4);
        printf("0x%08X\t\t0x%08X\n", i*4, value);
    }
}
