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
#ifndef _XMA_LIMITS_H
#define _XMA_LIMITS_H

#define MAX_SCALER_OUTPUTS       8
#define MAX_FILTER_OUTPUTS      MAX_SCALER_OUTPUTS
#define MAX_DDR_MAP             16
#define MAX_XILINX_DEVICES      16
#define MAX_XILINX_KERNELS      16
#define MAX_KERNEL_CONFIGS      60
#define MAX_KERNEL_CHANS        64
#define MAX_KERNEL_FREQS         2
#define MAX_IMAGE_CONFIGS       16
#define MAX_FUNCTION_NAME       32
#define MAX_PLUGIN_NAME         32
#define MAX_VENDOR_NAME         32
#define MAX_KERNEL_NAME         64
#define MAX_DSA_NAME            64
#define XMA_MAX_PLANES           3
#define MAX_PLUGINS             16
#define MAX_CONNECTION_ENTRIES  64
#endif
