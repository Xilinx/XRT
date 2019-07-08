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
#ifndef _XMAPLG_KERNEL_H_
#define _XMAPLG_KERNEL_H_

#include "xma.h"
#include "plg/xmasess.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * typedef XmaKernelPlugin - XmaKernel plugin interface
*/
typedef struct XmaKernelSession XmaKernelSession;

/**
 * struct XmaKernelPlugin - XmaKernel plugin interface
*/
typedef struct XmaKernelPlugin
{
    XmaKernelType   hwkernel_type; /**< specific kernel function of instance */
    const char     *hwvendor_string; /**< vendor of kernel */
    size_t          plugin_data_size; /**< session-specific private data */
    /** init callback used to perpare kernel and allocate device buffers */
    int32_t         (*init)(XmaKernelSession  *session);
    /** write callback used for general purpose write/send data operations */
    int32_t         (*write)(XmaKernelSession *session,
                             XmaParameter     *param,
                             int32_t           param_cnt);
    /** read callback used for general purpose read/recv data operations */
    int32_t         (*read)(XmaKernelSession   *session,
                            XmaParameter       *param,
                            int32_t            *param_cnt);
    /** close callback used to preform cleanup when application terminates session*/
    int32_t         (*close)(XmaKernelSession *session);

    /** Callback invoked at start to check compatibility with XMA version */
    int32_t         (*xma_version)(int32_t *main_version, int32_t *sub_version);

    /** Reserved */
    uint32_t        reserved[4];

} XmaKernelPlugin;

/**
 * struct XmaKernelSession - An instance of an XmaKernel
*/
typedef struct XmaKernelSession
{
    XmaSession            base; /**< base class of XmaKernelSession */
    XmaKernelProperties   kernel_props; /**< application supplied properites */
    XmaKernelPlugin      *kernel_plugin; /**< pointer to plugin driver */
} XmaKernelSession;

/**
 * to_xma_kernel() - Unpack XmaSession to XmaKernelSession subclass pointer
 *
 * @s: XmaSession parent instance
 *
 * RETURN:  pointer to XmaKernelSession container
 *
 * Note: caller must have checked if XmaSession represents a member
 * of XmaKernelSession by calling is_xma_kernel() prior to ensure
 * this unpacking is safe
*/
static inline XmaKernelSession *to_xma_kernel(XmaSession *s)
{
    return (XmaKernelSession *)s;
}

#ifdef __cplusplus
}
#endif

#endif
