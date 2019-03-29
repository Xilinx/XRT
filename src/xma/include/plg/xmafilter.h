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
#ifndef _XMAPLG_FILTER_H_
#define _XMAPLG_FILTER_H_

#include "xma.h"
#include "plg/xmasess.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct XmaFilterSession XmaFilterSession;

/**
 * struct XmaFilterPlugin - Plugin interface for XmaFilter type kernels
*/
typedef struct XmaFilterPlugin
{
    XmaFilterType   hwfilter_type; /**< specific kernel function of instance */
    const char     *hwvendor_string; /**< vendor of kernel */
    size_t          plugin_data_size; /**< session-specific private data */
    /** init callback used to perpare kernel and allocate device buffers */
    int32_t         (*init)(XmaFilterSession *session);
    /** Callback called when application calls xma_filter_send_frame() */
    int32_t         (*send_frame)(XmaFilterSession  *session,
                                  XmaFrame         *frame);
    /** Callback called when application calls xma_filter_recv_data() */
    int32_t         (*recv_frame)(XmaFilterSession *session,
                                  XmaFrame          *frame);
    /** Callback called when application calls xma_filter_session_destroy() */
    int32_t         (*close)(XmaFilterSession *session);

    /** Optional callback called when app calls xma_filter_session_create()
      * Implement this callback if your kernel supports channels and is
      * multi-process safe
    */
    xma_plg_alloc_chan_mp alloc_chan_mp;

    /** Optional callback called when app calls xma_filter_session_create()
      * Implement this callback if your kernel supports channels and is
      * NOT multi-process safe (but it IS thread-safe)
    */
    xma_plg_alloc_chan alloc_chan;
} XmaFilterPlugin;

/**
 * struct XmaFilterSession - Data structure representing a session instance for a filter kernel
*/
typedef struct XmaFilterSession
{
    XmaSession            base; /**< base class */
    XmaFilterProperties   props; /**< properties specified by app */
    XmaFilterPlugin      *filter_plugin; /**< link to XMA filter plugin */
    int32_t               conn_recv_handle; /**< upstream kernel */
    int32_t               conn_send_handle; /**< downstream kernel */
    uint64_t              out_dev_addr; /**< paddr of device output buffer */
    bool                  zerocopy_dest; /**< flag indicating destination supports zerocopy */
} XmaFilterSession;

/**
 * to_xma_filter() - Use to case a session object to an filter session.
 *
 * @s: Address of XmaSession member of enclosing XmaEncoderSession
 *  instance.
 *
 * RETURN: Pointer to XmaEncoderSession
 *
 * Note: Should call is_xma_filter() on pointer first to ensure this
 * converstion is safe.
*/
static inline XmaFilterSession *to_xma_filter(XmaSession *s)
{
    return (XmaFilterSession *)s;
}

#ifdef __cplusplus
}
#endif

#endif
