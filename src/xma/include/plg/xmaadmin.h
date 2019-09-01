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
#ifndef _XMAPLG_ADMIN_H_
#define _XMAPLG_ADMIN_H_

#include "xma.h"
#include "plg/xmasess.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * typedef XmaKernelPlugin - XmaKernel plugin interface
*/
typedef struct XmaAdminSession XmaAdminSession;

/**
 * struct XmaAdminPlugin - XmaAdmin plugin interface
*/
typedef struct XmaAdminPlugin
{
    XmaAdminType   hwkernel_type; /**< specific kernel function of instance */
    const char     *hwvendor_string; /**< vendor of kernel */
    size_t          plugin_data_size; /**< session-specific plugin private data */
    /** init callback used to perpare kernel and allocate device buffers */
    int32_t         (*init)(XmaAdminSession  *session);
    /** write callback used for general purpose write/send data operations */
    int32_t         (*write)(XmaAdminSession *session,
                             XmaParameter     *param,
                             int32_t           param_cnt);
    /** read callback used for general purpose read/recv data operations */
    int32_t         (*read)(XmaAdminSession   *session,
                            XmaParameter       *param,
                            int32_t            *param_cnt);
    /** close callback used to preform cleanup when application terminates session*/
    int32_t         (*close)(XmaAdminSession *session);

    /** Callback invoked at start to check compatibility with XMA version */
    int32_t         (*xma_version)(int32_t *main_version, int32_t *sub_version);

    /** Reserved */
    uint32_t        reserved[4];

} XmaAdminPlugin;

/**
 * struct XmaKernelSession - An instance of an XmaKernel
*/
typedef struct XmaAdminSession
{
    XmaSession            base; /**< base class of XmaKernelSession */
    XmaAdminProperties   admin_props; /**< application supplied properites */
    XmaAdminPlugin      *admin_plugin; /**< pointer to plugin driver */
    void                 *private_session_data; //Managed by host video application
    int32_t              private_session_data_size; //Managed by host video application
    /** Reserved */
    uint32_t        reserved[4];
} XmaAdminSession;

/**
 * to_xma_admin() - Unpack XmaSession to XmaAdminSession subclass pointer
 *
 * @s: XmaSession parent instance
 *
 * RETURN:  pointer to XmaAdminSession container
 *
 * Note: caller must have checked if XmaSession represents a member
 * of XmaAdminSession by calling is_xma_admin() prior to ensure
 * this unpacking is safe
*/
static inline XmaAdminSession *to_xma_admin(XmaSession *s)
{
    return (XmaAdminSession *)s;
}

#ifdef __cplusplus
}
#endif

#endif
