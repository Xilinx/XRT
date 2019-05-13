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
#ifndef _XMA_HW_HAL_H_
#define _XMA_HW_HAL_H_

typedef struct XmaHwHALKernel
{
    char            name[MAX_KERNEL_NAME];
    uint64_t        base_address;
    uint32_t        ddr_bank;
}XmaHwHALKernel;

typedef struct XmaHwHAL
{
    void           *dev_handle;                  // HAL device handle
    XmaHwHALKernel  kernels[MAX_XILINX_KERNELS];
    uint32_t       dev_index;
} XmaHwHAL;

#endif
