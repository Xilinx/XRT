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
#ifndef _XMAAPP_ENCODER_H_
#define _XMAAPP_ENCODER_H_


#include "app/xmabuffers.h"
#include "app/xmaparam.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  DOC:
 *  The Xilinx media encoder API is comprised of two distinct interfaces:
 *  one interface for an external framework such as FFmpeg or a proprietary
 *  multi-media framework and the plugin interface used by Xilinx
 *  accelerator developers.  This section illustrates both interfaces
 *  starting with the external framework view and moving on to the plugin
 *  developers view.
 *
 *  The external interface to the Xilinx video encoder is comprised of the
 *  following functions:
 *
 *  1. xma_enc_session_create()
 *  2. xma_enc_session_destroy()
 *  3. xma_enc_session_send_frame()
 *  4. xma_enc_session_recv_data()
 *
 *  A media framework (such as FFmpeg) is responsible for creating an encoder
 *  session.  The encoder session contains state information used by the
 *  encoder plugin to manage the hardware associated with a Xilinx accelerator
 *  device.  Prior to creating an encoder session the media framework is
 *  responsible for initializing the XMA using the function
 *  xma_initialize().  The initialize function should be called by the
 *  media framework early in the framework initialization to ensure that all
 *  resources have been configured.  Ideally, the  xma_initialize()
 *  function should be called from the main() function of the media framework
 *  in order to guarantee it is only called once.
 *
 *
 *  ::
 * 
 *      #include <xma.h>
 *
 *      int main(int argc, char *argv[])
 *      {
 *          int rc;
 *          char *yaml_filepath = argv[1];
 *
 *          // Other media framework initialization
 *          ...
 *
 *          rc = xma_initialize(yaml_filepath);
 *          if (rc != 0)
 *          {
 *              // Log message indicating XMA initialization failed
 *              printf("ERROR: Could not initialize XMA rc=%d\n\n", rc);
 *              return rc;
 *          }
 *
 *          // Other media framework processing
 *          ...
 *
 *          return 0;
 *      }
 *  
 *
 *  Assuming XMA initialization completes successfully, each encoder
 *  plugin must be initialized, provided frames to encode, requested to
 *  receive available encoded data and finally closed when the video stream
 *  ends.
 *
 *  The code snippet below demonstrates the creation of an XMA encoder
 *  session:
 *
 *
 *  ::
 * 
 *       // Code snippet for creating an encoder session
 *       ...
 *       #include <xma.h>
 *       ...
 *       // Setup encoder properties
 *       XmaEncoderProperties enc_props;
 *       enc_props.hwencoder_type = XMA_VP9_ENCODER_TYPE;
 *       strcpy(enc_props.hwvendor_string, "Xilinx");
 *       enc_props.format = XMA_YUV420_FMT_TYPE;
 *       enc_props.bits_per_pixel = 8;
 *       enc_props.width = 1920;
 *       enc_props.height = 1080;
 *       enc_props.framerate = 60;
 *       enc_props.bitrate = 4000000;
 *       enc_props.qp = -1;
 *       enc_props.gop_size = 30;
 *       enc_props.idr_interval = 30;
 *       enc_props.lookahead_depth = 4;
 *      
 *       // Create an encoder session based on the requested properties
 *       XmaEncoderSession *enc_session;
 *       enc_session = xma_enc_session_create(&enc_props);
 *       if (!enc_session)
 *       {
 *           // Log message indicating session could not be created
 *           // return from function
 *       }
 *       // Save returned session for subsequent calls.  In FFmpeg, the returned
 *       // session could be saved in the private_data of the AVCodecContext
 *  
 *
 *  The code snippet that follows demonstrates how to send a frame
 *  to the encoder session and receive any available encoded data:
 *
 *  ::
 * 
 *       // Code snippet for sending a frame to the encoder and checking
 *       // if encoded data is available.
 *      
 *       // Other non-XMA related includes
 *       ...
 *       #include <xma.h>
 *      
 *       // For this example it is assumed that enc_session is a pointer to
 *       // a previously created encoder session and an XmaFrame has been
 *       // created using the  xma_frame_from_buffers_clone() function.
 *       int32_t rc;
 *       rc = xma_enc_session_send_frame(enc_session, frame);
 *       if (rc != 0)
 *       {
 *           // Log error indicating frame could not be accepted
 *           return rc;
 *       }
 *      
 *       // Get the encoded data if it is available.  This example assumes
 *       // that an XmaDataBuffer has been created by cloning the data buffer
 *       // provided by the media framework using  xma_data_from_buffer_clone()
 *      
 *       int32_t data_size;
 *       rc = xma_enc_session_recv_data(enc_session, data_buffer, &data_size);
 *       if (rc != 0)
 *       {
 *           // No data to return at this time
 *           // Tell framework there is no available data
 *           return rc;
 *       }
 *       // Provide encoded data and data size to framework
 *       ...
 *       return rc;
 *  
 *
 *  This last code snippet demonstrates the interface for destroying the
 *  session when the stream is closed.  This allows all allocated resources
 *  to be freed and made available to other processes.
 *
 *  ::
 * 
 *       // Code snippet for destroying a session once a stream has ended
 *      
 *       // Other non-XMA related includes
 *       ...
 *       #include <xma.h>
 *      
 *       // This example assumes that the enc_session is a pointer to a previously
 *       // created XmaEncoderSession
 *       int32_t rc;
 *       rc = xma_enc_session_destroy(enc_session);
 *       if (rc != 0)
 *       {
 *           // TODO: Log message that the destroy function failed
 *           return rc;
 *       }
 *       return rc;
 *       
 */

/**
 * enum XmaEncoderType - Value specifies the precise type of encoder kernel requested
*/
typedef enum XmaEncoderType
{
    XMA_H264_ENCODER_TYPE = 1, /**< 1*/
    XMA_HEVC_ENCODER_TYPE, /**< 2 */
    XMA_VP9_ENCODER_TYPE, /**< 3 */
    XMA_AV1_ENCODER_TYPE, /**< 4 */
    XMA_COPY_ENCODER_TYPE, /**< 5 */
    XMA_MULTI_ENCODER_TYPE, /**< 6 */
} XmaEncoderType;

/**
 * struct XmaEncoderProperties - This properties structure must be created 
 * and initalized prior to calling xma_enc_session_create().  It is vital for 
 * ensuring the proper kernel is selected and that the plugin is provided the 
 * parameters necessary to initalize the kernel for video processing.
 *
*/
typedef struct XmaEncoderProperties
{
    /** hwencoder_type integer value indicating precise kernel function requested */
    XmaEncoderType  hwencoder_type;
    /* XmaConnProps    connection_props; */
    /** hwvendor_string requested vendor from which kernel originated */
    char            hwvendor_string[MAX_VENDOR_NAME];
    /** format input video format to kernel */
    XmaFormatType   format;
    /** bits per pixel for primary plane of input video */
    int32_t         bits_per_pixel;
    /** width width in pixels of incoming video stream/data */
    int32_t         width;
    /** height height in pixels of incoming video stream/data */
    int32_t         height;
    /** timebase */
    XmaFraction     timebase;
    /** framerate data structure specifying frame rate per second */
    XmaFraction     framerate;
    /** fixed bitrate requested for output (qp should not be specified if used) */
    int32_t         bitrate;
    /** qp fixed quantization value (0-51 with 51 indicated worst quality) */
    int32_t         qp;
    /** gop_size group-of-pictures size in frames */
    int32_t         gop_size;
    /** idr_interval frames between idr frame insertion */
    int32_t         idr_interval;
    /** lookahead_depth maximum number of input frames to request before */
    int32_t         lookahead_depth;
    /** qp_offset_I */
    int32_t         qp_offset_I;
    /** qp_offset_B0 */
    int32_t         qp_offset_B0;
    /** qp_offset_B1 */
    int32_t         qp_offset_B1;
    /** qp_offset_B2 */
    int32_t         qp_offset_B2;
    /** temp_aq_gain */
    int32_t         temp_aq_gain;
    /** spat_aq_gain */
    int32_t         spat_aq_gain;
    /** aq_mode  */
    int32_t         aq_mode;
    int32_t         minQP;
    /** force property values to be accepted by encoder plugin */
    int32_t         force_param;
    /** array of kernel-specific custom initialization parameters */
    XmaParameter    *params;
    /** count of custom parameters for port */
    uint32_t        param_cnt;
    int32_t         dev_index;
    int32_t         cu_index;
    int32_t         ddr_bank_index;//Used for allocating device buffers. Used only if valid index is provide (>= 0); value of -1 imples that XMA should select automatically and then XMA will set it with bank index used automatically
    int32_t         channel_id;
    char            *plugin_lib;//Lib with full path
    int32_t         reserved[4];
} XmaEncoderProperties;

/* Forward declaration */
typedef struct XmaEncoderSession XmaEncoderSession;

/**
 *  xma_enc_session_create() - This function creates an encoder session and must be 
 *  called prior to encoding a frame.  A session reserves hardware resources for the
 *  duration of a video stream. The number of sessions allowed depends on
 *  a number of factors that include: resolution, frame rate, bit depth,
 *  and the capabilities of the hardware accelerator.
 *
 *  @enc_props: Pointer to a XmaEncoderProperties structure that
 * contains the key configuration properties needed for
 * finding available hardware resource.
 *
 *  RETURN:          Not NULL on success
 * 
 * NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaEncoderSession*
xma_enc_session_create(XmaEncoderProperties *enc_props);

/**
 *  xma_enc_session_destroy() - This function destroys an encoder session that was previously created
 *  with the xm_enc_session_create function.
 *
 *  @session:  Pointer to XmaEncoderSession created with
 *                  xma_enc_session_create
 *
 *  RETURN: XMA_SUCCESS on success
 *  
 * XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t
xma_enc_session_destroy(XmaEncoderSession *session);

/**
 *  xma_enc_session_send_frame() - This function sends a frame to the hardware encodeding.  
 *  If a frame buffer is not available and the blocking flag is set to true, this
 *  function will block.  If a frame buffer is not available and the
 *  blocking flag is set to false, this function will return XMA_SEND_MORE_DATA.
 *
 *  @session:  Pointer to session created by xm_enc_sesssion_create
 *  @frame:    Pointer to a frame to be encoded.  If the encoder
 * has buffered input, the input will need to be flushed.  To do so,
 * an XmaFrame with a null pointer to the first data buffer will need
 * to be sent until XMA_EOS is received (XmaFrame.data[0].buffer = NULL).
 *
 *  RETURN: XMA_SUCCESS on success; indicates that sufficient data has
 * been received to begin producing output
 * 
 * XMA_SEND_MORE_DATA if additional frames are needed before
 * output can be received
 * 
 * XMA_ERROR on error
*/
int32_t
xma_enc_session_send_frame(XmaEncoderSession *session,
                           XmaFrame          *frame);

/**
 *  xma_enc_session_recv_data() - This function returns a data buffer along with the length of the buffer
 *  if one is available.  This function is called after calling the
 *  function xm_enc_session_send_frame.  If a data buffer is not ready to be
 *  returned, this function returns -1 and sets the length of the data
 *  buffer to 0.  In addition, the data buffer pointer is set to NULL.  If a
 *  data buffer is ready, the data_size will be set with a non-zero value and
 *  a pointer to the data buffer will be set to a non-NULL value.
 *
 *  @session:   Pointer to session created by xm_enc_sesssion_create
 *  @data:      Pointer to a data buffer containing encoded data
 *  @data_size: Pointer to hold the size of the data buffer returned
 *
 *  RETURN:        XMA_SUCCESS on success.
 *          
 * XMA_EOS when all data has been flushed from the encoder
 *          
 * XMA_ERROR on error.
*/
int32_t
xma_enc_session_recv_data(XmaEncoderSession *session,
                          XmaDataBuffer     *data,
                          int32_t           *data_size);

#ifdef __cplusplus
}
#endif

#endif
