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
#ifndef _XMAPLG_DECODER_H_
#define _XMAPLG_DECODER_H_

#include "xma.h"
#include "plg/xmasess.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * XmaDecoderSession - Session object serving as handle to a kernel allocated to an application
*/
typedef struct XmaDecoderSession XmaDecoderSession;

/**
 * struct XmaDecoderPlugin - A decoder plugin instance
*/
typedef struct XmaDecoderPlugin
{
    /** Specific type of decoder (e.g. h.264) */
    XmaDecoderType  hwdecoder_type;
    /** Vendor responsible for creating kernel */
    const char     *hwvendor_string;
    /** Size of private session-specific data */
    size_t          plugin_data_size;
    /** Init callback called during session creation */
    int32_t         (*init)(XmaDecoderSession *dec_session);
    /** Send callback invoked when application sends data to plugin */
    int32_t         (*send_data)(XmaDecoderSession  *dec_session,
                                 XmaDataBuffer     *data,
								 int32_t           *data_used);
    /** Callback to retrieve frame properties of decoded data */
    int32_t         (*get_properties)(XmaDecoderSession *dec_session,
                                      XmaFrameProperties *fprops);
    /** Receive callback invoked when application requests data from plugin */
    int32_t         (*recv_frame)(XmaDecoderSession *dec_session,
                                  XmaFrame           *frame);
    /** Callback invoked to clean up device buffers when app has terminated session */
    int32_t         (*close)(XmaDecoderSession *session);

    /** Callback invoked at start to check compatibility with XMA version */
    int32_t         (*xma_version)(int32_t *main_version, int32_t *sub_version);

    /** Reserved */
    uint32_t        reserved[4];
} XmaDecoderPlugin;

/**
 * struct XmaDecoderSession - Session object representing a kernel or kernel channel allocated to app
*/
typedef struct XmaDecoderSession
{
    XmaSession            base; /**< base session class */
    XmaDecoderProperties  decoder_props; /**< session decoder properties */
    XmaDecoderPlugin     *decoder_plugin; /**< pointer to plugin instance */
    int32_t               conn_recv_handle; /**< connection handle to encoder */
} XmaDecoderSession;

/**
 * to_xma_decoder() - Return XmaDecoderSession subclass from XmaSession parent
 *
 * Note: Caller should first ensure that this pointer is actually a parent
 * of an XmaDecoderSession by calling is_xma_decoder() prior to making
 * this cast.
*/
static inline XmaDecoderSession *to_xma_decoder(XmaSession *s)
{
    return (XmaDecoderSession *)s;
}

#ifdef __cplusplus
}
#endif

#endif
