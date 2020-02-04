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
#ifndef _XMAPLG_SCALER_H_
#define _XMAPLG_SCALER_H_


#include "xma.h"
#include "plg/xmasess.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * typedef XmaScalerSession - An instance of a scaler kernel allocated to a client application
*/
typedef struct XmaScalerSession XmaScalerSession;

/**
 * @struct XmaScalerPlugin - Scaler plugin interface
*/
typedef struct XmaScalerPlugin
{
    XmaScalerType   hwscaler_type; /**< specific scaler type of this instance */
    const char     *hwvendor_string; /**< vendor of kernel controlled by plugin*/
    XmaFormatType   input_format; /**< id of fourcc input format */
    XmaFormatType   output_format; /**< id of fourcc output format */
    int32_t         bits_per_pixel; /**< bits per pixel of input primary plane */
    size_t          plugin_data_size; /**< plugin session private data size */
     /** callback to initalize kernel and kernel buffers*/
    int32_t         (*init)(XmaScalerSession *session);
    /** callback to process input frame from client */
    int32_t         (*send_frame)(XmaScalerSession  *session,
                                  XmaFrame         *frame);
    /** callback to send output data to client */
    int32_t         (*recv_frame_list)(XmaScalerSession *session,
                                      XmaFrame          **frame_list);
    /** callback to perform cleanup when client terminates session */
    int32_t         (*close)(XmaScalerSession *sc_session);

    /** Optional callback called when app calls xma_scal_session_create()
      * Implement this callback if your kernel supports channels and is
      * multi-process safe
    */
    xma_plg_alloc_chan_mp alloc_chan_mp;

    /** Optional callback called when app calls xma_scal_session_create()
      * Implement this callback if your kernel supports channels and is
      * NOT multi-process safe (but it IS thread-safe)
    */
    xma_plg_alloc_chan alloc_chan;

} XmaScalerPlugin;

/**
 * struct XmaScalerSession - An instance of a scaler kernel allocated to a client application
*/
typedef struct XmaScalerSession
{
    XmaSession            base; /**< base session class */
    XmaScalerProperties   props; /**< client requested scaler properties */
    XmaScalerPlugin      *scaler_plugin; /**< pointer to plugin interface */
    int32_t               conn_recv_handle; /**< handle to upstream kernel */
    int32_t               conn_send_handles[MAX_SCALER_OUTPUTS]; /**< handle to downstream kernels*/
    uint64_t              out_dev_addrs[MAX_SCALER_OUTPUTS]; /**< paddrs to write scaled outputs */
    bool                  zerocopy_dests[MAX_SCALER_OUTPUTS]; /**< map of downstream connections supporting zerocopy */
    int8_t                current_pipe; /**< current_pipe */
    int8_t                first_frame; /**< first_frame */

} XmaScalerSession;

/**
 * to_xma_scaler() - Unpack an XmaSession to the XmaScalerSession subclass
 *
 * @s: XmaSession instance
 *
 * Note: call is_xma_scaler() prior to this call to ensure this
 * reference can be unpacked into an XmaScalerSession safely.
*/
static inline XmaScalerSession *to_xma_scaler(XmaSession *s)
{
    return (XmaScalerSession *)s;
}


#ifdef __cplusplus
}
#endif

#endif
