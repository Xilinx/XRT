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
//#include "lib/xmares.h"

#include <cstdio>
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
using namespace std;

#define XMAPLUGIN_MOD "xmapluginlib"

//Sarab: TODO .. Assign buffr object fields.. bank etc..
//Add new API for allocating device only buffer object.. which will not have host mappped buffer.. This could be used for zero copy plugins..
//NULL data pointer in buffer obj implies it is device only buffer..
//Remove read/write before SyncBO.. As plugin should manage that using host mapped data pointer..

XmaBufferObj
xma_plg_buffer_alloc(XmaSession s_handle, size_t size, bool device_only_buffer, int32_t* return_code)
{
    XmaBufferObj b_obj;
    XmaBufferObj b_obj_error;
    b_obj_error.data = NULL;
    b_obj_error.size = 0;
    b_obj_error.bank_index = -1;
    b_obj_error.dev_index = -1;
    b_obj_error.device_only_buffer = false;
    b_obj_error.private_do_not_touch = NULL;
    b_obj.data = NULL;
    b_obj.device_only_buffer = false;
    b_obj.private_do_not_touch = NULL;

    xclDeviceHandle dev_handle = s_handle.hw_session.dev_handle;
    uint32_t ddr_bank = s_handle.hw_session.kernel_info->ddr_bank;
    b_obj.bank_index = s_handle.hw_session.kernel_info->ddr_bank;
    b_obj.size = size;
    b_obj.dev_index = s_handle.hw_session.dev_index;

    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_buffer_alloc failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_alloc failed. XMASession is corrupted.\n");
        if (return_code) *return_code = XMA_ERROR;
        return b_obj_error;
    }

    /*
    #define XRT_BO_FLAGS_MEMIDX_MASK        (0xFFFFFFUL)
    #define XCL_BO_FLAGS_CACHEABLE          (1 << 24)
    #define XCL_BO_FLAGS_SVM                (1 << 27)
    #define XCL_BO_FLAGS_DEV_ONLY           (1 << 28)
    #define XCL_BO_FLAGS_HOST_ONLY          (1 << 29)
    #define XCL_BO_FLAGS_P2P                (1 << 30)
    #define XCL_BO_FLAGS_EXECBUF            (1 << 31)
    */
    uint64_t b_obj_handle = 0;
    if (device_only_buffer) {
        b_obj_handle = xclAllocBO(dev_handle, size, 0, XCL_BO_FLAGS_DEV_ONLY | ddr_bank);
        b_obj.device_only_buffer = true;
    } else {
        b_obj_handle = xclAllocBO(dev_handle, size, 0, ddr_bank);
    }
    /*BO handlk is uint64_t
    if (b_obj_handle < 0) {
        std::cout << "ERROR: xma_plg_buffer_alloc failed. handle=0x" << std::hex << b_obj_handle << std::endl;
        //printf("xclAllocBO failed. handle=0x%ullx\n", b_obj_handle);
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xclAllocBO failed.\n");
        if (return_code) *return_code = XMA_ERROR;
        return b_obj_error;
    }
    */
    b_obj.paddr = xclGetDeviceAddr(dev_handle, b_obj_handle);
    if (!device_only_buffer) {
        b_obj.data = (uint8_t*) xclMapBO(dev_handle, b_obj_handle, true);
    }
    XmaBufferObjPrivate* tmp1 = new XmaBufferObjPrivate;
    b_obj.private_do_not_touch = (void*) tmp1;
    tmp1->dummy = (void*)(((uint64_t)tmp1) | signature);
    tmp1->size = size;
    tmp1->paddr = b_obj.paddr;
    tmp1->bank_index = b_obj.bank_index;
    tmp1->dev_index = b_obj.dev_index;
    tmp1->boHandle = b_obj_handle;
    tmp1->device_only_buffer = b_obj.device_only_buffer;
    tmp1->dev_handle = dev_handle;

    if (return_code) *return_code = XMA_SUCCESS;
    return b_obj;
}

int32_t xma_check_device_buffer(XmaBufferObj *b_obj) {
    if (b_obj == NULL) {
        std::cout << "ERROR: xma_device_buffer_free failed. XMABufferObj failed allocation" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_device_buffer_free failed. XMABufferObj failed allocation\n");
        return XMA_ERROR;
    }

    XmaBufferObjPrivate* b_obj_priv = (XmaBufferObjPrivate*) b_obj->private_do_not_touch;
    if (b_obj_priv == NULL) {
        std::cout << "ERROR: xma_device_buffer_free failed. XMABufferObj failed allocation" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_device_buffer_free failed. XMABufferObj failed allocation\n");
        return XMA_ERROR;
    }
    if (b_obj_priv->dev_index < 0 || b_obj_priv->bank_index < 0 || b_obj_priv->size <= 0) {
        std::cout << "ERROR: xma_device_buffer_free failed. XMABufferObj failed allocation" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_device_buffer_free failed. XMABufferObj failed allocation\n");
        return XMA_ERROR;
    }
    if (b_obj_priv->dummy != (void*)(((uint64_t)b_obj_priv) | signature)) {
        std::cout << "ERROR: xma_device_buffer_free failed. XMABufferObj is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_device_buffer_free failed. XMABufferObj is corrupted.\n");
        return XMA_ERROR;
    }
    if (b_obj_priv->dev_handle == NULL) {
        std::cout << "ERROR: xma_device_buffer_free failed. XMABufferObj is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_device_buffer_free failed. XMABufferObj is corrupted.\n");
        return XMA_ERROR;
    }
    return XMA_SUCCESS;
}

void
xma_plg_buffer_free(XmaSession s_handle, XmaBufferObj b_obj)
{
    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_buffer_free failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_free failed. XMASession is corrupted.\n");
        return;
    }
    if (xma_check_device_buffer(&b_obj) != XMA_SUCCESS) {
        return;
    }
    XmaBufferObjPrivate* b_obj_priv = (XmaBufferObjPrivate*) b_obj.private_do_not_touch;
    //xclDeviceHandle dev_handle = s_handle.hw_session.dev_handle;
    xclFreeBO(b_obj_priv->dev_handle, b_obj_priv->boHandle);
    b_obj_priv->dummy = NULL;
    b_obj_priv->size = -1;
    b_obj_priv->bank_index = -1;
    b_obj_priv->dev_index = -1;
    free(b_obj_priv);
}

/*Sarab: padd API not required with buffer Object
uint64_t
xma_plg_get_paddr(XmaHwSession s_handle, XmaBufferObj b_obj)
{
    uint64_t paddr;
    xclDeviceHandle dev_handle = s_handle.hw_session.dev_handle;
    paddr = xclGetDeviceAddr(dev_handle, b_obj);
#if 0
    printf("xma_plg_get_paddr b_obj = %d, paddr = %lx\n", b_obj, paddr);
#endif
    return paddr;
}
*/

int32_t
xma_plg_buffer_write(XmaSession s_handle,
                     XmaBufferObj  b_obj,
                     size_t           size,
                     size_t           offset)
{
    int32_t rc;
    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_buffer_write failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_write failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (xma_check_device_buffer(&b_obj) != XMA_SUCCESS) {
        return XMA_ERROR;
    }
    XmaBufferObjPrivate* b_obj_priv = (XmaBufferObjPrivate*) b_obj.private_do_not_touch;
    if (b_obj_priv->device_only_buffer) {
        xma_logmsg(XMA_WARNING_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_write skipped as it is device only buffer.\n");
        return XMA_SUCCESS;
    }
    if (size + offset > b_obj_priv->size) {
        std::cout << "ERROR: xma_plg_buffer_write failed. Can not write past end of buffer" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_write failed. Can not write past end of buffer.\n");
        return XMA_ERROR;
    }
    //xclDeviceHandle dev_handle = s_handle.hw_session.dev_handle;

    //printf("xma_plg_buffer_write b_obj=%d,src=%p,size=%lu,offset=%lx\n", b_obj, src, size, offset);
    rc = xclSyncBO(b_obj_priv->dev_handle, b_obj_priv->boHandle, XCL_BO_SYNC_BO_TO_DEVICE, size, offset);
    if (rc != 0) {
        std::cout << "ERROR: xma_plg_buffer_write xclSyncBO failed " << std::dec << rc << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xclSyncBO failed %d\n", rc);
    }

    return XMA_SUCCESS;
}

int32_t
xma_plg_buffer_read(XmaSession s_handle,
                    XmaBufferObj  b_obj,
                    size_t           size,
                    size_t           offset)
{
    int32_t rc;
    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_buffer_read failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_read failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (xma_check_device_buffer(&b_obj) != XMA_SUCCESS) {
        return XMA_ERROR;
    }
    XmaBufferObjPrivate* b_obj_priv = (XmaBufferObjPrivate*) b_obj.private_do_not_touch;
    if (b_obj_priv->device_only_buffer) {
        xma_logmsg(XMA_WARNING_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_read skipped as it is device only buffer.\n");
        return XMA_SUCCESS;
    }
    if (size + offset > b_obj_priv->size) {
        std::cout << "ERROR: xma_plg_buffer_read failed. Can not read past end of buffer" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_buffer_read failed. Can not read past end of buffer.\n");
        return XMA_ERROR;
    }

    //xclDeviceHandle dev_handle = s_handle.hw_session.dev_handle;

    //printf("xma_plg_buffer_read b_obj=%d,dst=%p,size=%lu,offset=%lx\n",
    //       b_obj, dst, size, offset);
    rc = xclSyncBO(b_obj_priv->dev_handle, b_obj_priv->boHandle, XCL_BO_SYNC_BO_FROM_DEVICE,
                   size, offset);
    if (rc != 0)
    {
        std::cout << "ERROR: xma_plg_buffer_read xclSyncBO failed " << std::dec << rc << std::endl;
    }

    return XMA_SUCCESS;
}

int32_t
xma_plg_register_prep_write(XmaSession  s_handle,
                       void         *src,
                       size_t        size,
                       size_t        offset)
{
    uint32_t *src_array = (uint32_t*)src;
    int32_t   cur_max = offset + size; 
    uint32_t  entries = size / sizeof(uint32_t);
    uint32_t  start = offset / sizeof(uint32_t);

    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_register_prep_write failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_register_prep_write failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    //Kernel regmap 4KB in xmahw.h; execBO size is 4096 = 4KB in xmahw_hal.cpp; But ERT uses some space for ert pkt so allow max of 4032 Bytes for regmap
    if (cur_max > MAX_KERNEL_REGMAP_SIZE) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Max kernel regmap size is 4032 Bytes\n");
        return XMA_ERROR;
    }
    if (*(s_handle.hw_session.kernel_info->reg_map_locked)) {
        if (s_handle.session_id != s_handle.hw_session.kernel_info->locked_by_session_id || s_handle.session_type != s_handle.hw_session.kernel_info->locked_by_session_type) {
            xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "regamp is locked by another session\n");
            return XMA_ERROR;
        }
    } else {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Must lock kernel regamp before writing to it and submitting work item\n");
        return XMA_ERROR;
    }
    if (s_handle.hw_session.kernel_info->regmap_max < cur_max) {
        s_handle.hw_session.kernel_info->regmap_max = cur_max;
    }
    for (uint32_t i = 0, tmp_idx = start; i < entries; i++, tmp_idx++) {
        s_handle.hw_session.kernel_info->reg_map[tmp_idx] = src_array[i];
    }

    return XMA_SUCCESS;
}

int32_t xma_plg_kernel_lock_regmap(XmaSession s_handle)
{
    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_kernel_lock_regmap failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_kernel_lock_regmap failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    /* Only acquire the lock if we don't already own it */
    if (*(s_handle.hw_session.kernel_info->reg_map_locked)) {
        if (s_handle.session_id == s_handle.hw_session.kernel_info->locked_by_session_id && s_handle.session_type == s_handle.hw_session.kernel_info->locked_by_session_type) {
            return XMA_SUCCESS;
        } else {
            return XMA_ERROR;
        }
    }
    bool expected = false;
    bool desired = true;
    while (!(*(s_handle.hw_session.kernel_info->reg_map_locked)).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //reg map lock acquired

    s_handle.hw_session.kernel_info->locked_by_session_id = s_handle.session_id;
    s_handle.hw_session.kernel_info->locked_by_session_type = s_handle.session_type;

    return XMA_SUCCESS;
}

int32_t xma_plg_kernel_unlock_regmap(XmaSession s_handle)
{
    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_kernel_unlock_regmap failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_kernel_unlock_regmap failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    /* Only unlock if this session owns it */
    if (*(s_handle.hw_session.kernel_info->reg_map_locked)) {
        if (s_handle.session_id == s_handle.hw_session.kernel_info->locked_by_session_id && s_handle.session_type == s_handle.hw_session.kernel_info->locked_by_session_type) {
            *(s_handle.hw_session.kernel_info->reg_map_locked) = false;
            s_handle.hw_session.kernel_info->locked_by_session_id = -1;

            return XMA_SUCCESS;
        } else {
            return XMA_ERROR;
        }
    }
    return XMA_SUCCESS;
}

int32_t xma_plg_execbo_avail_get(XmaSession s_handle)
{
    //std::cout << "Sarab: Debug - " << __func__ << "; " << __LINE__ << std::endl;
    XmaHwKernel* kernel_tmp1 = s_handle.hw_session.kernel_info;
    XmaHwDevice *dev_tmp1 = (XmaHwDevice*)kernel_tmp1->private_do_not_use;
    if (dev_tmp1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Session XMA private pointer is NULL\n");
        return -1;
    }
    int32_t num_execbo = dev_tmp1->num_execbo_allocated;
    if (num_execbo <= 0) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Session XMA private: No execbo allocated\n");
        return -1;
    }
    int32_t i;
    int32_t rc = -1;
    bool    found = false;
    bool expected = false;
    bool desired = true;
    while (!(*(dev_tmp1->execbo_locked)).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //kernel completion lock acquired

    for (i = 0; i < num_execbo; i++) {
        ert_start_kernel_cmd *cu_cmd = 
            (ert_start_kernel_cmd*)dev_tmp1->kernel_execbo_data[i];
        if (dev_tmp1->kernel_execbo_inuse[i]) {
            if (dev_tmp1->kernel_execbo_cu_index[i] == kernel_tmp1->cu_index) {
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
                        kernel_tmp1->kernel_complete_count++;

                    break;
                    case ERT_CMD_STATE_ERROR:
                    case ERT_CMD_STATE_ABORT:
                        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD,
                                "Could not find free execBO cmd buffer\n");
                    break;
                }
            }
        }
        else
            found = true;

        if (found) {
            dev_tmp1->kernel_execbo_inuse[i] = true;
            dev_tmp1->kernel_execbo_cu_index[i] = kernel_tmp1->cu_index;
            rc = i;
            break;
        }
    }
    //std::cout << "Sarab: Debug - " << __func__ << "; " << __LINE__ << std::endl;
    //Release completion lock
    *(dev_tmp1->execbo_locked) = false;

    return rc;
}

int32_t
xma_plg_schedule_work_item(XmaSession s_handle)
{
    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_schedule_work_item failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_schedule_work_item failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    XmaHwKernel* kernel_tmp1 = s_handle.hw_session.kernel_info;
    XmaHwDevice *dev_tmp1 = (XmaHwDevice*)kernel_tmp1->private_do_not_use;
    if (dev_tmp1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Session XMA private pointer is NULL\n");
        return XMA_ERROR;
    }
    uint8_t *src = (uint8_t*)kernel_tmp1->reg_map;
    
    //size_t  size = MAX_KERNEL_REGMAP_SIZE;//Max regmap in xmahw.h is 4KB; execBO size is 4096; Supported max regmap size is 4032 Bytes only
    size_t  size = s_handle.hw_session.kernel_info->regmap_max;
    if (size < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Use xma_plg_register_prep_write to prepare regmap for CU before scheduling work item \n");
        return XMA_ERROR;
    }

    int32_t bo_idx;
    int32_t rc = XMA_SUCCESS;
    
    if (*(kernel_tmp1->reg_map_locked)) {
        if (s_handle.session_id != kernel_tmp1->locked_by_session_id || s_handle.session_type != kernel_tmp1->locked_by_session_type) {
            xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "regamp is locked by another session\n");
            return XMA_ERROR;
        }
    } else {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Must lock kernel regamp before writing to it and submitting work item\n");
        return XMA_ERROR;
    }

    // Find an available execBO buffer
    bo_idx = xma_plg_execbo_avail_get(s_handle);
    if (bo_idx == -1) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Unable to find free execbo to use\n");
        return XMA_ERROR;
    }
    // Setup ert_start_kernel_cmd 
    ert_start_kernel_cmd *cu_cmd = 
        (ert_start_kernel_cmd*)dev_tmp1->kernel_execbo_data[bo_idx];
    cu_cmd->state = ERT_CMD_STATE_NEW;
    if (kernel_tmp1->soft_kernel) {
        cu_cmd->opcode = ERT_SK_START;
    } else {
        cu_cmd->opcode = ERT_START_CU;
    }
    cu_cmd->extra_cu_masks = 3;//XMA now supports 128 CUs

    cu_cmd->cu_mask = kernel_tmp1->cu_mask0;

    cu_cmd->data[0] = kernel_tmp1->cu_mask1;
    cu_cmd->data[1] = kernel_tmp1->cu_mask2;
    cu_cmd->data[2] = kernel_tmp1->cu_mask3;
    // Copy reg_map into execBO buffer 
    memcpy(&cu_cmd->data[3], src, size);
    if (kernel_tmp1->regmap_max >= 1024) {
        xma_logmsg(XMA_DEBUG_LOG, XMAPLUGIN_MOD, "Dev# %d; Kernel: %s; Regmap size used is: %d\n", dev_tmp1->dev_index, kernel_tmp1->name, kernel_tmp1->regmap_max);
    }

    // Set count to size in 32-bit words + 4; Three extra_cu_mask are present
    cu_cmd->count = (size >> 2) + 4;
    
    if (xclExecBuf(s_handle.hw_session.dev_handle, 
                    dev_tmp1->kernel_execbo_handle[bo_idx]) != 0)
    {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD,
                    "Failed to submit kernel start with xclExecBuf\n");
        rc = XMA_ERROR;
    }
    return rc;
}

int32_t xma_plg_is_work_item_done(XmaSession s_handle, int32_t timeout_ms)
{
    if (s_handle.session_signature != (void*)(((uint64_t)s_handle.hw_session.kernel_info) | ((uint64_t)s_handle.hw_session.dev_handle))) {
        std::cout << "ERROR: xma_plg_schedule_work_item failed. XMASession is corrupted" << std::endl;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "xma_plg_schedule_work_item failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    XmaHwKernel* kernel_tmp1 = s_handle.hw_session.kernel_info;
    XmaHwDevice *dev_tmp1 = (XmaHwDevice*)kernel_tmp1->private_do_not_use;
    if (dev_tmp1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Session XMA private pointer is NULL\n");
        return XMA_ERROR;
    }
    int32_t num_execbo = dev_tmp1->num_execbo_allocated;
    if (num_execbo <= 0) {
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD, "Session XMA private: No execbo allocated\n");
        return XMA_ERROR;
    }

    int32_t count = 0;
    int32_t give_up = 0;
    // Keep track of the number of kernel completions
    while (count == 0)
    {
        bool expected = false;
        bool desired = true;
        while (!(*(dev_tmp1->execbo_locked)).compare_exchange_weak(expected, desired)) {
            expected = false;
        }
        //kernel completion lock acquired

        // Look for inuse commands that have completed and increment the count
        for (int32_t i = 0; i < num_execbo; i++)
        {
            if (dev_tmp1->kernel_execbo_inuse[i] && (dev_tmp1->kernel_execbo_cu_index[i] == kernel_tmp1->cu_index))
            {
                ert_start_kernel_cmd *cu_cmd = 
                    (ert_start_kernel_cmd*)dev_tmp1->kernel_execbo_data[i];
                if (cu_cmd->state == ERT_CMD_STATE_COMPLETED)
                {
                    // Increment completed kernel count and make BO buffer available
                    count++;
                    dev_tmp1->kernel_execbo_inuse[i] = false;
                } 
            }
        }
        //Release completion lock
        *(dev_tmp1->execbo_locked) = false;

        if (count)
            break;

        // Wait for a notification
        give_up++;
        if (xclExecWait(s_handle.hw_session.dev_handle, timeout_ms) <= 0 && give_up >= 3)
            break;
    }

    bool expected = false;
    bool desired = true;
    while (!(*(dev_tmp1->execbo_locked)).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //kernel completion lock acquired

    kernel_tmp1->kernel_complete_count += count;
    if (kernel_tmp1->kernel_complete_count)
    {
        kernel_tmp1->kernel_complete_count--;
        
        //Release completion lock
        *(dev_tmp1->execbo_locked) = false;
        return XMA_SUCCESS;
    }
    else
    {
        //Release completion lock
        *(dev_tmp1->execbo_locked) = false;
        xma_logmsg(XMA_ERROR_LOG, XMAPLUGIN_MOD,
                    "Could not find completed work item\n");
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}
