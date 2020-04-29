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

#include "app/xmalimits.h"

#ifndef _XMA_LIMITS_APP_H
#define _XMA_LIMITS_APP_H

#define MAX_DDR_MAP             64
#define MAX_XILINX_DEVICES      16
#define MAX_XILINX_KERNELS      128
#define MAX_XILINX_SOFT_KERNELS      128
//#define LIKELY_KERNEL_CONFIGS      16
//#define MAX_KERNEL_CHANS        64
#define MAX_KERNEL_FREQS         2
//#define MAX_IMAGE_CONFIGS       16
//#define MAX_FUNCTION_NAME       256
#define MAX_KERNEL_NAME         256
//#define MAX_DSA_NAME            256
//#define MAX_PLUGINS             32//32 encoders+32 decoders+...

#define XMA_LIB_MAIN_VER        2020
#define XMA_LIB_SUB_VER         1

#define XMA_NUM_EXECBO_DEFAULT  4//KDS fixed for wait_count check
#define XMA_NUM_EXECBO_MODE2    1
#define XMA_NUM_EXECBO_MODE3    8
#define XMA_NUM_EXECBO_MODE4    64

#define XMA_CPU_MODE1           1  //Low cpu load + high performance 
#define XMA_CPU_MODE2           2  //High cpu load + high performance
#define XMA_CPU_MODE3           3  //Same as legacy
#define XMA_CPU_MODE4           4  //Low cpu load

#define INVALID_M1             -1
#define STATS_WINDOW            4096.0f
#define STATS_WINDOW_1          4095

#endif
