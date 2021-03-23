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
#ifndef _XMA_HW_H_
#define _XMA_HW_H_

#ifdef __cplusplus
extern "C" {
#endif

//typedef void * XmaKernelRes;
typedef struct XmaHwKernel XmaHwKernel;

typedef struct XmaHwSession
{
    uint32_t        dev_index;
    int32_t         bank_index;//default bank to use
    void            *private_do_not_use;

    uint32_t         reserved[4];
} XmaHwSession;

enum XmaCmdState {
  XMA_CMD_STATE_QUEUED = 1, //Submitted to XMA -> XRT
  XMA_CMD_STATE_COMPLETED = 2, //Cmd has finished
  XMA_CMD_STATE_ERROR = 3, //XMA or XRT error during submission of cmd
  XMA_CMD_STATE_ABORT = 4, //XRT aborted the cmd; CU may or may not have received the cmd
  XMA_CMD_STATE_TIMEOUT = 5, //XMA or XRT timeout waiting for cmd to finish
  XMA_CMD_STATE_PSK_ERROR = 6, //PS Kernel cmd completed but with error return code
  XMA_CMD_STATE_PSK_CRASHED = 7, //PS kernel has crashed
  XMA_CMD_STATE_MAX = 8 // Always the last one
};


#ifdef __cplusplus
}
#endif

#endif
