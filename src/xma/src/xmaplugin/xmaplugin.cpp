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

#include <iostream>
#include <memory.h>
#include <thread>
#include <chrono>
#include "ert.h"
using namespace std;

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
    cout << "ERROR: Replace xma_plg_register_write api with:" << endl;
    cout << "  - xma_plg_ebo_kernel_start and" << endl;
    cout << "  - xma_plg_ebo_kernel_done" << endl;
    return -1;
}

/**
 *  @brief execBO based kernel APIs
 *  xma_plg_register_write should NOT be used.
 *  Please use below APIs instead
 *  xma_plg_ebo_kernel_start
 *  xma_plg_ebo_kernel_done: Wait for all pending kernel commands to finish
 *  
 */
int32_t xma_plg_ebo_kernel_start(XmaHwSession  s_handle, uint32_t* args, uint32_t args_size) {
  int loop_idx = 0;
  bool start_done = false;
  if (args_size > 2048) {
    cout << "ERROR: arg_size for kernel is too large" << endl;
    exit(1);
  }
  /*
  cout << "Kernel to start: " << string((char*)s_handle.kernel_info.name) << endl;
  cout << "Kernel instance: " << dec << s_handle.kernel_info.instance << endl;
  cout << "Kernel ddr bank: " << dec << s_handle.kernel_info.ddr_bank << endl;
  cout << "Kernel base_addr: 0x" << hex << s_handle.kernel_info.base_address << endl;
  */
  while (loop_idx < 10 && !start_done) {
    for (int i = 0; i < MAX_EXECBO_POOL_SIZE; i++) {
      //cout << "ExecBO pool idx: " << dec << i << endl;
      //cout << "ExecBO handle: 0x" << hex << s_handle.kernel_info.kernel_execbo_handle[i] << endl;
      //cout << "ExecBO data: 0x" << hex << (void*)s_handle.kernel_info.kernel_execbo_data[i] << endl;
      ert_start_kernel_cmd* cu_cmd = (ert_start_kernel_cmd*) s_handle.kernel_info.kernel_execbo_data[i];
      if (s_handle.kernel_info.kernel_execbo_inuse[i]) {
        //cout << "Check if previous kernel cmds have finished" << endl;
        if (cu_cmd->state >= 4) {
          if (cu_cmd->state != 4) {
            cout << "ERROR: CU DONE failed" << endl;
            exit(1);
          }
          s_handle.kernel_info.kernel_execbo_inuse[i] = false;
          cu_cmd->state = ERT_CMD_STATE_NEW;
          cu_cmd->opcode = ERT_START_CU;
          memset((void*)cu_cmd->data, 0, (cu_cmd->count - 1) * 4);
        }
      } else if (!start_done) {
        //cout << "Will start kernel now" << endl;
        memcpy((void*)cu_cmd->data, (char*) args, args_size);
        cu_cmd->count = 1 + (args_size >> 2);//cu_args_size is in bytes => convert to 32 bit words
        s_handle.kernel_info.kernel_execbo_inuse[i] = true;
        if (xclExecBuf(s_handle.dev_handle, s_handle.kernel_info.kernel_execbo_handle[i]) != 0) {
          cout << "ERROR: Failed to submit Kernel start" << endl;
          exit(1);
        }
        start_done = true;
      }
    }
    if (!start_done) {
      loop_idx++;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  if (!start_done) {
    cout << "ERROR: Kernel may be hung. Failed to submit Kernel start" << endl;
    exit(1);
  }
  return 0;
}


//Wait for all pending kernel commands to finish
int32_t xma_plg_ebo_kernel_done(XmaHwSession  s_handle) {
  //cout << "kernel_done start" << endl;
  int loop_idx = 0;
  bool all_idle = false;
  while (loop_idx <= MAX_EXECBO_POOL_SIZE && !all_idle) {
    all_idle = true;
    for (int i = 0; i < MAX_EXECBO_POOL_SIZE; i++) {
      if (s_handle.kernel_info.kernel_execbo_inuse[i]) {
        all_idle = false;
        ert_start_kernel_cmd* cu_cmd = (ert_start_kernel_cmd*) s_handle.kernel_info.kernel_execbo_data[i];
        if (cu_cmd->state >= 4) {
          if (cu_cmd->state != 4) {
            cout << "ERROR: CU DONE failed" << endl;
            exit(1);
          }
          s_handle.kernel_info.kernel_execbo_inuse[i] = false;
          cu_cmd->state = ERT_CMD_STATE_NEW;
          cu_cmd->opcode = ERT_START_CU;
        } else {
          //cout << "Testing: ..." << endl;
          if (xclExecWait(s_handle.dev_handle, 7000) < 0) {//With zero keep waiting. > 0 go and check status..
            cout << "ERROR: Failed to wait for Kernel done" << endl;
            exit(1);
          }
        }
      }
    }
  }
  if (!all_idle) {
    cout << "ERROR: With XMA multiple threads not allowed to share a kernel." << endl;
    exit(1);
  }
  return 0;
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
