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
#ifndef _XMDECODER_H_
#define _XMDECODER_H_

#include "app/xmabuffers.h"
#include "app/xmaparam.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * DOC:
 *  The Xilinx media decoder API is comprised of two distinct interfaces:
 *  one interface for an external framework such as FFmpeg or a proprietary
 *  multi-media framework and the plugin interface used by Xilinx
 *  accelerator developers.  This section illustrates both interfaces
 *  starting with the external framework view and moving on to the plugin
 *  developers view.
 *
 *  The external interface to the Xilinx video decoder is comprised of the
 *  following functions:
 *
 *  1. xma_dec_session_create()
 *  2. xma_dec_session_destroy()
 *  3. xma_dec_session_send_data()
 *  4. xma_dec_session_get_properties()
 *  5. xma_dec_session_recv_frame()
 *
 *  A media framework (such as FFmpeg) is responsible for creating a decoder
 *  session.  The decoder session contains state information used by the
 *  decoder plugin to manage the hardware associated with a Xilinx accelerator
 *  device.  Prior to creating an decoder session the media framework is
 *  responsible for initializing the XMA using the function
 *  xma_initialize().  The initialize function should be called by the
 *  media framework early in the framework initialization to ensure that all
 *  resources have been configured.  Ideally, the xma_initialize()
 *  function should be called from the main() function of the media framework
 *  in order to guarantee it is only called once.
 *
 */

/**
 * enum XmaDecoderType - A decoder from this list forms part of a request for 
 * a specific decoder when creating a decoder session via xma_dec_session_create.
 */
typedef enum XmaDecoderType
{
    XMA_H264_DECODER_TYPE = 1, /**< 1 */
    XMA_HEVC_DECODER_TYPE, /**< 2 */
    XMA_VP9_DECODER_TYPE, /**< 3 */
    XMA_AV1_DECODER_TYPE, /**< 4 */
    XMA_JPG_DECODER_TYPE, 
    XMA_MULTI_DECODER_TYPE
} XmaDecoderType;

/**
 * struct XmaDecoderProperties - Properities used to specify which decoder is requested 
 * and how the decoder should be initalized by the plugin driver
 */
typedef struct XmaDecoderProperties
{
    /** Specific type of decoder requested. See #XmaDecoderType*/
    XmaDecoderType  hwdecoder_type;
    /* XmaConnProps    connection_props; */
    /** Vendor string used to identify specific decoder requested */
    char            hwvendor_string[MAX_VENDOR_NAME];
    /** todo */
    int32_t         intraOnly;
    /** array of kernel-specific custom initialization parameters */
    XmaParameter    *params;
    /** count of custom parameters for port */
    uint32_t        param_cnt;
    /** bits per pixel for primary plane of input video */
    int32_t         bits_per_pixel;
    /** width width in pixels of incoming video stream/data */
    int32_t         width;
    /** height height in pixels of incoming video stream/data */
    int32_t         height;
    /** framerate data structure specifying frame rate per second */
    XmaFraction     framerate;

    /** For JPG Decoders: chroma_width & chroma_height specify subsampling of chroma: YUV444, YUV422, YUV411, YUV420, etc  */
    int32_t         chroma_width;
    int32_t         chroma_height;
    int32_t         num_of_UV;//Num of UV components; yuv400 has zero UV

    int32_t         dev_index;
    int32_t         cu_index;
    char            *cu_name;
    int32_t         ddr_bank_index;//Used for allocating device buffers. Used only if valid index is provide (>= 0); value of -1 imples that XMA should select automatically and then XMA will set it with bank index used automatically
    int32_t         channel_id;
    char            *plugin_lib;
    int32_t         reserved[4];
} XmaDecoderProperties;

/* Forward declaration */
typedef struct XmaDecoderSession XmaDecoderSession;

/**
 *  xma_dec_session_create() - This function creates a decoder session and must be called prior to
 *  decoding data.  A session reserves hardware resources for the
 *  duration of a video stream. The number of sessions allowed depends on
 *  a number of factors that include: resolution, frame rate, bit depth,
 *  and the capabilities of the hardware accelerator.
 *
 *  @dec_props: Pointer to a XmaDecoderProperties structure that
 * contains the key configuration properties, device & cu index, etc
 *
 *  RETURN: Not NULL on success
 * 
 * NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaDecoderSession*
xma_dec_session_create(XmaDecoderProperties *dec_props);

/**
 *  xma_dec_session_destroy() - This function destroys a decoder session that 
 * was previously created with the xma_dec_session_create function.
 *
 *  @session:  Pointer to XmaDecoderSession created with
 * xma_dec_session_create
 *
 *  RETURN: XMA_SUCCESS on success
 * 
 * XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t
xma_dec_session_destroy(XmaDecoderSession *session);

/**
 *  xma_dec_session_send_data() - This function invokes plugin->send_data fucntion 
 * assigned to this session which handles sending data to the hardware decoder.  
 *
 * Plugin and media framework (like FFMPEG) may handle events like 
 * errors, no empty lookup buffer, no output data, end of input stream, etc using 
 * XMA_FLUSH_AGAIN, XMA_TRY_AGAIN, XMA_SEND_MORE_DATA, XMA_EOS,
 * XMA_ERROR, etc return codes
 * 
 *  @session:   Pointer to session created by xma_dec_sesssion_create
 *  @data:      Pointer to a data buffer to be decoded
 *  @data_used: Pointer to an integer to receive the amount of data used
 *
 *  RETURN:        XMA_SUCCESS on success.
 *  
 * XMA_ERROR on error.
*/

int32_t
xma_dec_session_send_data(XmaDecoderSession *session,
                          XmaDataBuffer     *data,
						  int32_t           *data_used);

/**
 *  xma_dec_session_get_properties() - This function invokes plugin->get_properties 
 * assigned to this session which returns properties of the hardware decoder.  
 *
 *  @dec_session:  Pointer to session created by xma_dec_sesssion_create
 *  @fprops:   Pointer to a frame properties structure to be filled in
 *
 *  RETURN: XMA_SUCCESS on success.
 *  
 * XMA_ERROR on error.
*/
int32_t
xma_dec_session_get_properties(XmaDecoderSession  *dec_session,
                               XmaFrameProperties *fprops);

/**
 *  xma_dec_session_recv_frame() - This function invokes plugin->recv_frame 
 * assigned to this session which handles obtaining output frame from the hardware decoder.  
 * This function returns a frame if one is available.  
 * 
 * This function is called after calling the function xma_dec_session_send_data.  
 * If a frame is not ready to be returned, plugin function should return -1.  In addition, the
 *  frame pointer should be set to NULL.  If a frame is ready, a pointer to the frame
 *  will be set to a non-NULL value.
 *
 *  @session:     Pointer to session created by xma_dec_sesssion_create
 *  @frame:       Pointer to a frame containing decoded data
 *
 *  RETURN:        XMA_SUCCESS on success.
 *        
 * XMA_ERROR on error.
*/
int32_t
xma_dec_session_recv_frame(XmaDecoderSession *session,
                           XmaFrame          *frame);

#ifdef __cplusplus
}
#endif

#endif
