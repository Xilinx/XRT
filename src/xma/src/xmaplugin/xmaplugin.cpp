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
#include "xmaplugin.h"
#include "xrt.h"
#include "ert.h"
#include "lib/xmahw_lib.h"
#include "lib/xmares.h"

#include <cstdio>
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
using namespace std;

#define XMAPLUGIN_MOD "xmapluginlib"

XmaBufferHandle
xma_plg_buffer_alloc(XmaHwSession s_handle, size_t size)
{
    XmaBufferHandle handle;
    xclDeviceHandle dev_handle = s_handle.dev_handle;
    uint32_t ddr_bank = s_handle.kernel_info->ddr_bank;

#if 0
    printf("xma_plg_buffer_alloc dev_handle = %p\n", dev_handle);
    printf("xma_plg_buffer_alloc size = %lu\n", size);
    printf("xma_plg_buffer_alloc ddr_bank = %u\n", ddr_bank);
#endif
    handle = xclAllocBO(dev_handle, size, 0, ddr_bank);
#if 0
    printf("xma_plg_buffer_alloc handle = %d\n", handle);
#endif
  
    if (handle < 0) {
        printf("xclAllocBO failed. handle=%d\n", handle);
    }
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
    uint32_t *src_array = (uint32_t*)src;
    size_t   cur_max = offset + size; 
    uint32_t  entries = size / sizeof(uint32_t);
    uint32_t  start = offset / sizeof(uint32_t);

    //Kernel regmap 4KB in xmahw.h; execBO size is 4096 = 4KB in xmahw_hal.cpp; But ERT uses some space for ert pkt so allow max of 4032 Bytes for regmap
    if (cur_max > MAX_KERNEL_REGMAP_SIZE) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Max kernel regmap size is 4032 Bytes\n");
        return XMA_ERROR;
    }
    for (uint32_t i = 0, tmp_idx = start; i < entries; i++, tmp_idx++) {
        s_handle.kernel_info->reg_map[tmp_idx] = src_array[i];
    }

    return 0;
}

void xma_plg_kernel_lock(XmaHwSession s_handle)
{
    /* Only acquire the lock if we don't already own it */
    if (s_handle.kernel_info->have_lock)
        return;

    xma_res_kernel_lock(s_handle.kernel_info->lock);
    s_handle.kernel_info->have_lock = true;
}

void xma_plg_kernel_unlock(XmaHwSession s_handle)
{
    if (s_handle.kernel_info->have_lock)
    {
        xma_res_kernel_unlock(s_handle.kernel_info->lock);
        s_handle.kernel_info->have_lock = false;
    }
}

int32_t xma_plg_execbo_avail_get(XmaHwSession s_handle)
{
    int32_t i;
    int32_t rc = -1;
    bool    found = false;

    for (i = 0; i < MAX_EXECBO_POOL_SIZE; i++)
    {
        ert_start_kernel_cmd *cu_cmd = 
            (ert_start_kernel_cmd*)s_handle.kernel_info->kernel_execbo_data[i];
        if (s_handle.kernel_info->kernel_execbo_inuse[i])
        {
            switch(cu_cmd->state)
            {
                case ERT_CMD_STATE_NEW:
                case ERT_CMD_STATE_QUEUED:
                case ERT_CMD_STATE_RUNNING:
                    continue;
                break;
                case ERT_CMD_STATE_COMPLETED:
                    found = true;
                    // Update count of completed work items
                    s_handle.kernel_info->kernel_complete_count++;
                break;
                case ERT_CMD_STATE_ERROR:
                case ERT_CMD_STATE_ABORT:
                    xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD,
                               "Could not find free execBO cmd buffer\n");
                break;
            }
        }
        else
            found = true;

        if (found)
            break;
    }
    if (found)
    {
        s_handle.kernel_info->kernel_execbo_inuse[i] = true;
        rc = i;
    }

    return rc;
}

int32_t
xma_plg_schedule_work_item(XmaHwSession s_handle)
{
    uint8_t *src = (uint8_t*)s_handle.kernel_info->reg_map;
    //size_t  size = s_handle.kernel_info->max_offset;
    size_t  size = MAX_KERNEL_REGMAP_SIZE;//Max regmap in xmahw.h is 4KB; execBO size is 4096; Supported max regmap size is 4032 Bytes only
    int32_t bo_idx;
    int32_t rc = XMA_SUCCESS;
    
    // Find an available execBO buffer
    bo_idx = xma_plg_execbo_avail_get(s_handle);
    if (bo_idx == -1)
        rc = XMA_ERROR;
    else
    {
        // Setup ert_start_kernel_cmd 
        ert_start_kernel_cmd *cu_cmd = 
            (ert_start_kernel_cmd*)s_handle.kernel_info->kernel_execbo_data[bo_idx];
        cu_cmd->state = ERT_CMD_STATE_NEW;
        cu_cmd->opcode = ERT_START_CU;

        // Copy reg_map into execBO buffer 
        memcpy(cu_cmd->data, src, size);

        // Set count to size in 32-bit words + 1 
        cu_cmd->count = (size >> 2) + 1;
     
        if (xclExecBuf(s_handle.dev_handle, 
                       s_handle.kernel_info->kernel_execbo_handle[bo_idx]) != 0)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD,
                       "Failed to submit kernel start with xclExecBuf\n");
            rc = XMA_ERROR;
        }
    }
         
    return rc;
}

int32_t xma_plg_is_work_item_done(XmaHwSession s_handle, int32_t timeout_ms)
{
    int32_t current_count = s_handle.kernel_info->kernel_complete_count;
    int32_t count = 0;

    // Keep track of the number of kernel completions
    while (count == 0)
    {
        // Look for inuse commands that have completed and increment the count
        for (int32_t i = 0; i < MAX_EXECBO_POOL_SIZE; i++)
        {
            if (s_handle.kernel_info->kernel_execbo_inuse[i])
            {
                ert_start_kernel_cmd *cu_cmd = 
                    (ert_start_kernel_cmd*)s_handle.kernel_info->kernel_execbo_data[i];
                if (cu_cmd->state == ERT_CMD_STATE_COMPLETED)
                {
                    // Increment completed kernel count and make BO buffer available
                    count++;
                    s_handle.kernel_info->kernel_execbo_inuse[i] = false;
                } 
            }
        }

        if (count)
            break;

        // Wait for a notification
        if (xclExecWait(s_handle.dev_handle, timeout_ms) <= 0)
            break;
    }
    current_count += count;
    if (current_count)
    {
        current_count--;
        s_handle.kernel_info->kernel_complete_count = current_count;
        return XMA_SUCCESS;
    }
    else
    {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD,
                    "Could not find completed work item\n");
        return XMA_ERROR;
    }
}
    
int32_t
xma_plg_register_write(XmaHwSession  s_handle,
                       void         *src,
                       size_t        size,
                       size_t        offset)
{
    xclDeviceHandle dev_handle = s_handle.dev_handle;
    uint64_t        dev_offset = s_handle.kernel_info->base_address;
    //printf("xma_plg_register_write dev_handle=%p,base_addr=0x%lx,src=%p,size=%lu,offset=0x%lx\n", dev_handle, dev_offset, src, size, offset);
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
    uint64_t        dev_offset = s_handle.kernel_info->base_address;
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
