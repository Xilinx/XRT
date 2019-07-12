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
#ifndef _XMAPLG_ENCODER_H_
#define _XMAPLG_ENCODER_H_


#include "xma.h"
#include "plg/xmasess.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct XmaEncoderSession XmaEncoderSession;

/**
 * struct XmaEncoderPlugin - Primary XMA encoder interface to be implemented to provide
 * application control of a given kernel.
 * The plugin code must statically allocate this structure
 * and provide values for all member except alloc_chan which is
 * an optional callback. This structure and its callbacks are
 * the primary link between the XMA application interface and
 * the hardware kernel.
 * Additional notes on some members:
 * XmaEncoderPlugin::alloc_chan()
 * If kernel supports multiple channels, then this callback must be
 * implemented. If not, it will be treated as a single-channel kernel
 * XmaEncoderPlugin::get_dev_input_paddr()
 * Plugin should return free buffer that upstream video kernel can fill
 * with video frame data to be encoded.
*/
typedef struct XmaEncoderPlugin
{
    /** specific encoder type */
    XmaEncoderType  hwencoder_type;
    /** Specific encoder vendor */
    const char     *hwvendor_string;
    /** input video format fourcc index */
    XmaFormatType   format;
    /** bits per pixel for primary plane of input format */
    int32_t         bits_per_pixel;
    /** size of allocated kernel-wide private data */
    size_t          kernel_data_size;
    /** size of allocated private plugin data.*/
    size_t          plugin_data_size;
    /** Initalization callback.  Called during session_create() */
    int32_t         (*init)(XmaEncoderSession *enc_session);
    /** Callback called when application calls xma_enc_send_frame() */
    int32_t         (*send_frame)(XmaEncoderSession *enc_session,
                                  XmaFrame          *frame);
    /** Callback called when application calls xma_enc_recv_data() */
    int32_t         (*recv_data)(XmaEncoderSession  *enc_session,
                                 XmaDataBuffer      *data,
                                 int32_t            *data_size);
    /** Callback called when application calls xma_enc_session_destroy() */
    int32_t         (*close)(XmaEncoderSession *session);

    /** Callback invoked at start to check compatibility with XMA version */
    int32_t         (*xma_version)(int32_t *main_version, int32_t *sub_version);

    /** Reserved */
    uint32_t        reserved[4];

} XmaEncoderPlugin;


/**
 * struct XmaEncoderSession - Represents an instance of an encoder kernel
 */
typedef struct XmaEncoderSession
{
    XmaSession            base; /**< base class */
    XmaEncoderProperties  encoder_props; /**< properties specified by app */
    XmaEncoderPlugin     *encoder_plugin; /**< link to XMA encoder plugin */
    /** index into connection table (requires zerocopy: enable in cfg)  */
    //int32_t               conn_recv_handle;
    /** Reserved */
    uint32_t        reserved[4];
} XmaEncoderSession;

/**
 * to_xma_encoder() - Use to case a session object to an encoder session.
 *
 * @s: Address of XmaSession member of enclosing XmaEncoderSession
 *  instance.
 *
 * RETURN: Pointer to XmaEncoderSession
 *
 * Note: Should call is_xma_encoder() on pointer first to ensure this
 * converstion is safe.
*/
static inline XmaEncoderSession *to_xma_encoder(XmaSession *s)
{
    return (XmaEncoderSession *)s;
}

#ifdef __cplusplus
}
#endif

#endif
