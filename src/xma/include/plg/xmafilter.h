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

/**
 * @ingroup xma_plg_intf
 * @file plg/xmafilter.h
 * XMA plugin interface for video filer kernels
 */

#include "xma.h"
#include "plg/xmasess.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup xmaplugin
 * @addtogroup xmaplgfilter xmafilter.h
 * @{
*/

/**
 * @typedef XmaFilterPlugin
 * Plugin interface for XmaFilter type kernels
 *
 * @typedef XmaFilterSession
 * Data structure representing a session instance for a filter kernel
*/

/* Forward declaration */
typedef struct XmaFilterSession XmaFilterSession;

/**
 * @struct XmaFilterPlugin
 * Plugin interface for XmaFilter type kernels
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
    /** Optional callback called when app calls xma_filter_session_create() */
    int32_t         (*alloc_chan)(XmaSession *pending_sess,
                                  XmaSession **curr_sess,
                                  uint32_t sess_cnt);
} XmaFilterPlugin;

/**
 * @struct XmaFilterSession
 * Data structure representing a session instance for a filter kernel
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
 * @brief Unpack an abstract XmaSession into an filter session
 *
 * Use to case a session object to an filter session.
 *
 * @param s Address of XmaSession member of enclosing XmaEncoderSession
 *  instance.
 *
 * @return Pointer to XmaEncoderSession
 *
 * @note Should call is_xma_filter() on pointer first to ensure this
 * converstion is safe.
*/
static inline XmaFilterSession *to_xma_filter(XmaSession *s)
{
    return (XmaFilterSession *)s;
}
/** @} */
#ifdef __cplusplus
}
#endif

#endif
