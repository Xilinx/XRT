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
#ifndef _XMAAPP_SCALER_H_
#define _XMAAPP_SCALER_H_


#include "app/xmabuffers.h"
#include "lib/xmalimits.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC:
 *  The Xilinx media scaler API is comprised of two distinct interfaces:
 *  one interface for an external framework such as FFmpeg or a proprietary
 *  multi-media framework and the plugin interface used by Xilinx
 *  accelerator developers.  This section illustrates both interfaces
 *  starting with the external framework view and moving on to the plugin
 *  developers view.
 *
 *  The external interface to the Xilinx video scaler is comprised of the
 *  following functions:
 *
 *  1. xma_scaler_session_create()
 *  2. xma_scaler_session_destroy()
 *  3. xma_scaler_session_send_frame()
 *  4. xma_scaler_session_recv_frame_list()
 *
 *  A media framework (such as FFmpeg) is responsible for creating a scaler
 *  session.  The scaler session contains state information used by the
 *  scaler plugin to manage the hardware associated with a Xilinx accelerator
 *  device.  Prior to creating a scaler session the media framework is
 *  responsible for initializing the XMA using the function
 *  @ref xma_initialize().  The initialize function should be called by the
 *  media framework early in the framework initialization to ensure that all
 *  resources have been configured.  Ideally, the @ref xma_initialize()
 *  function should be called from the main() function of the media framework
 *  in order to guarantee it is only called once.
 *
 *
 *  ::
 * 
 *       #include <xma.h>
 *      
 *       int main(int argc, char *argv[])
 *       {
 *           int rc;
 *      
 *           // Other media framework initialization
 *           ...
 *      
 *           rc = xma_initialize();
 *           if (rc != 0)
 *           {
 *               // Log message indicating XMA initialization failed
 *               printf("ERROR: Could not initialize XMA rc=%d\n\n", rc);
 *               return rc;
 *           }
 *      
 *           // Other media framework processing
 *           ...
 *      
 *           return 0;
 *       }
 *
 *  Assuming XMA initialization completes successfully, each scaler
 *  plugin must be initialized, provided frames to scale, requested to
 *  receive available scaled data and finally closed when the video stream
 *  ends.
 *
 *  The code snippet below demonstrates the creation of an XMA scaler
 *  session:
 *
 *
 *  ::
 * 
 *       // Code snippet for creating an scaler session
 *       ...
 *       #include <xma.h>
 *       ...
 *       // Setup scaler properties
 *       XmaScalerProperties props;
 *       props.hwencoder_type = XMA_POLYPHASE_SCALER_TYPE;
 *       strcpy(props.hwvendor_string, "Xilinx");
 *       props.num_outputs = 2;
 *       xma_scaler_default_filter_coeff_set(&props.filter_coefficients);
 *       props.input.format = XMA_YUV420_FMT_TYPE;
 *       props.input.bit_per_pixel = 8;
 *       props.input.width = 1920;
 *       props.input.height = 1080;
 *       props.input.stride = 1920;
 *       props.input.filter_idx = 0;
 *       props.output[0].format = XMA_YUV420_FMT_TYPE;
 *       props.output[0].bit_per_pixel = 8;
 *       props.output[0].width = 1280;
 *       props.output[0].height = 720;
 *       props.output[0].stride = 1280;
 *       props.output[0].filter_idx = 0;
 *       props.output[1].format = XMA_YUV420_FMT_TYPE;
 *       props.output[1].bit_per_pixel = 8;
 *       props.output[1].width = 640;
 *       props.output[1].height = 480;
 *       props.output[1].stride = 640;
 *       props.output[1].filter_idx = 0;
 *      
 *       // Create a scaler session based on the requested properties
 *       XmaScalerSession *session;
 *       session = xma_scaler_session_create(&props);
 *       if (!session)
 *       {
 *           // Log message indicating session could not be created
 *           // return from function
 *       }
 *       // Save returned session for subsequent calls.  In FFmpeg, the returned
 *       // session could be saved in the private_data of the AVCodecContext
 *
 *  The code snippet that follows demonstrates how to send a frame
 *  to the scaler session and receive any available scaled frames:
 *
 *  ::
 * 
 *       // Code snippet for sending a frame to the encoder and checking
 *       // if scaled frames are available.
 *      
 *       // Other non-XMA related includes
 *       ...
 *       #include <xma.h>
 *      
 *       // For this example it is assumed that session is a pointer to
 *       // a previously created scaler session and an XmaFrame has been
 *       // created using the @ref xma_frame_from_buffers_clone() function.
 *      
 *       // In presence of a scaler pipeline, XMA_SEND_MORE_DATA return code  
 *       // is sent by xma_scaler_session_send_frame() until the pipeline is filled.  
 *       // Subsequently  before closing scaler, NULL frames are sent to 
 *       // xma_scaler_session_send_frame()  until all the frames are flushed from 
 *       // pipeline with XMA_FLUSH_AGAIN.  
 *       // frame->data[0].buffer = NULL;
 *       // frame->data[1].buffer = NULL;
 *       // frame->data[2].buffer = NULL; 
 *       // And a final XMA_EOS return code ends the scaler processing. 
 *      
 *       int32_t send_rc;
 *       int32_t recv_rc;
 *       send_rc = xma_scaler_session_send_frame(session, frame);
 *       if (send_rc == XMA_EOS)
 *       {
 *           // destroy session and cleanup
 *           return 0;
 *       }
 *       
 *      
 *       // Get the scaled frame list if it is available. It is assumed that
 *       // the caller will provide a list of pointers to XmaFrame structures
 *       // that are large enough to hold the scaled output for each selected
 *       // resolution.
 *       // For XMA_SEND_MORE_DATA return code from xma_scaler_session_send_frame()
 *       // xma_scaler_session_recv_frame_list() is skipped as the output will not be available yet.
 *       // It will get the scaler outputs with a XMA_SUCCESS or a XMA_FLUSH_AGAIN return code.  
 *       
 *       XmaFrame *frame_list[2];
 *      
 *       XmaFrameProperties fprops;
 *       fprops.format = XMA_YUV420_FMT_TYPE;
 *       fprops.width = 1280;
 *       fprops.height = 720;
 *       fprops.bits_per_pixel = 8;
 *      
 *       frame_list[0] = xma_frame_alloc(&fprops);
 *       fprops.width = 640;
 *       fprops.height = 480;
 *       frame_list[1] = xma_frame_alloc(&fprops);
 *       if ((send_rc==XMA_SUCCESS)||(send_rc==XMA_FLUSH_AGAIN))
 *       {
 *            recv_rc = xma_scaler_session_recv_frame_list(session, frame_list);
 *            if (recv_rc != XMA_SUCCESS)
 *            {
 *               // No data to return at this time
 *               // Tell framework there is no available data
 *               return (-1);
 *            }
 *       }
 *       // Provide scaled frames to framework
 *       ...
 *       return rc;
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
 *       // This example assumes that the session is a pointer to a previously
 *       // created XmaScalerSession
 *       int32_t rc;
 *       rc = xma_scaler_session_destroy(session);
 *       if (rc != 0)
 *       {
 *           // TODO: Log message that the destroy function failed
 *           return rc;
 *       }
 *       return rc;
 */

/**
 * enum XmaScalerType - Specific type of scaler to request
*/
typedef enum XmaScalerType
{
    XMA_BICUBIC_SCALER_TYPE = 1, /**< 1 */
    XMA_BILINEAR_SCALER_TYPE, /**< 2 */
    XMA_POLYPHASE_SCALER_TYPE /**< 3 */
} XmaScalerType;

/**
 * struct XmaScalerInOutProperties - Properties which shall be used to specify configuration 
 * of kernel input or output as applicable
*/
typedef struct XmaScalerInOutProperties
{
    XmaFormatType   format; /**< ID specifying fourcc format */
    int32_t         bits_per_pixel; /**< bits per pixel for primary plane */
    int32_t         width; /**< width of primary plane */
    int32_t         height; /**< height of primary plane */
    /** framerate data structure specifying frame rate per second */
    XmaFraction     framerate;
    int32_t         stride; /**< stride of primary plane */
    int32_t         filter_idx; /**< tbd */
    int32_t         coeffLoad;       /**< 0-AutoGen 1-Default 2-FromFile */
    char            coeffFile[1024]; /**< Coeff file name when coeffLoad is set to 2 */
} XmaScalerInOutProperties;

/**
 * struct XmaScalerFilterProperties - Filter coefficients to be used by kernel
 */
typedef struct XmaScalerFilterProperties
{
    int16_t         h_coeff0[64][12]; /**< horizontal coefficients 1 */
    int16_t         h_coeff1[64][12]; /**< horizontal coefficients 2 */
    int16_t         h_coeff2[64][12]; /**< horizontal coefficients 3 */
    int16_t         h_coeff3[64][12]; /**< horizontal coefficients 4 */
    int16_t         v_coeff0[64][12]; /**< vertical coefficients 1 */
    int16_t         v_coeff1[64][12]; /**< vertical coefficients 2 */
    int16_t         v_coeff2[64][12]; /**< vertical coefficients 3 */
    int16_t         v_coeff3[64][12]; /**< vertical coefficients 4 */
} XmaScalerFilterProperties;

/* Forward declaration */
typedef struct XmaSession XmaSession;
typedef struct XmaScalerSession XmaScalerSession;

/**
 * struct XmaScalerProperties - Properties structure used to request filter type and 
 * vendor as well as the manner in which it should be inialized by the plugin
*/
typedef struct XmaScalerProperties
{
    /** specific filter function requested */
    XmaScalerType             hwscaler_type;
    /** downstream kernel receiving data from this filter */
    XmaSession                *destination;
    /** maximum number of scaled outputs */
    uint32_t                  max_dest_cnt;
    /** specific vendor filter originated from */
    char                      hwvendor_string[MAX_VENDOR_NAME];
    /** number of actual scaled outputs */
    int32_t                   num_outputs;
    /** application-specified filter coefficients */
    XmaScalerFilterProperties filter_coefficients;
    /** input properties */
    XmaScalerInOutProperties  input;
    /** output properties array */
    XmaScalerInOutProperties  output[MAX_SCALER_OUTPUTS];
    /** array of kernel-specific custom initialization parameters */
    XmaParameter    *params;
    /** count of custom parameters for port */
    uint32_t        param_cnt;
} XmaScalerProperties;


/**
 *  xma_scaler_default_filter_coeff_set() - This helper function sets the default horizontal and vertical
 *  filter coefficients for a polyphase filter bank.
 *
 *  @props: Pointer to a XmaScalerFilterProperties structure that
 * will contain the filter coefficients.
 *
*/
void xma_scaler_default_filter_coeff_set(XmaScalerFilterProperties *props);

/**
 *  xma_scaler_session_create() - This function creates a scaler session and must be called prior to
 *  scaling a frame.  A session reserves hardware resources for the
 *  duration of a video stream. The number of sessions allowed depends on
 *  a number of factors that include: resolution, frame rate, bit depth,
 *  and the capabilities of the hardware accelerator.
 *
 *  @props Pointer to a XmaScalerProperties structure that
 * contains the key configuration properties needed for
 * finding available hardware resource.
 *
 *  RETURN:      Not NULL on success
 *        
 * NULL on failure
 *
 *  Note: Cannot be presumed to be thread safe.
*/
XmaScalerSession*
xma_scaler_session_create(XmaScalerProperties *props);

/**
 *  xma_scaler_session_destroy() - This function destroys an scaler session that was previously created
 *  with the xma_scaler_session_create() function.
 *
 *  @session:  Pointer to XmaScalerSession created with
 * xma_scaler_session_create
 *
 *  RETURN: XMA_SUCCESS on success
 *          
 * XMA_ERROR on failure.
 *
 *  Note: Cannot be presumed to be thread safe.
*/
int32_t
xma_scaler_session_destroy(XmaScalerSession *session);

/**
 *  xma_scaler_session_send_frame() - This function sends a frame to the hardware scaler.  If a frame
 *  buffer is not available and this interface will block.
 *
 *  @session:  Pointer to session created by xma_scaler_sesssion_create
 *  @frame:    Pointer to a frame to be scaled.  If the scaler is
 *  buffering input, then an XmaFrame with a NULL data buffer
 *  pointer to the first data buffer must be sent to flush the filter and
 *  to indicate that no more data will be sent:
 *  XmaFrame.data[0].buffer = NULL
 *  The application must then check for XMA_FLUSH_AGAIN for each such call
 *  when flushing the last few frames.  When XMA_EOS is returned, no new
 *  data may be collected from the scaler.
 *
 *  RETURN:        XMA_SUCCESS on success and the scaler is ready to
 * produce output
 *          
 * XMA_SEND_MORE_DATA if the scaler is buffering input frames
 *          
 * XMA_FLUSH_AGAIN when flushing scaler with a null frame
 *          
 * XMA_EOS when the scaler has been flushed of all residual frames
 *          
 * XMA_ERROR on error
*/
int32_t
xma_scaler_session_send_frame(XmaScalerSession *session,
                              XmaFrame         *frame);

/**
 *  xma_scaler_session_recv_frame_list() - This function populates a list of XmaFrame buffers with the scaled
 *  data returned from the hardware accelerator. This function is called after
 *  calling the function xma_scaler_session_send_frame.  If a data buffer is
 *  not ready to be returned, this function blocks.
 *
 *  @session:    Pointer to session created by xma_scaler_sesssion_create
 *  @frame_list: Pointer to a list of XmaFrame structures
 *
 *  RETURN:        XMA_SUCCESS on success
 *          
 * XMA_ERROR on error
*/
int32_t
xma_scaler_session_recv_frame_list(XmaScalerSession *session,
                                  XmaFrame          **frame_list);

#ifdef __cplusplus
}
#endif

#endif
