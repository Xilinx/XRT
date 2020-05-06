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
    int32_t         dev_index;
    int32_t         cu_index;
    char            *cu_name;
    int32_t         ddr_bank_index;//Used for allocating device buffers. Used only if valid index is provide (>= 0); value of -1 imples that XMA should select automatically and then XMA will set it with bank index used automatically
    int32_t         channel_id;
    char            *plugin_lib;
    bool            ooo_execution;//Out of order execution of cu cmds
    int32_t         reserved[4];
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
 *  Note: session create & destroy are thread safe APIs
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
 *  Note: session create & destroy are thread safe APIs
*/
int32_t
xma_scaler_session_destroy(XmaScalerSession *session);

/**
 *  xma_scaler_session_send_frame() - This function invokes plugin->send_frame fucntion 
 * assigned to this session which handles sending data to the hardware scaler.  
 * 
 * This function sends a frame to the hardware scaler.  If a frame
 *  buffer is not available, the plugin function should block.
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
 *  xma_scaler_session_recv_frame_list() - This function invokes plugin->recv_frame_list 
 * assigned to this session which handles obtaining list of output frames from the hardware encoder.  
 * This function is called after
 *  calling the function xma_scaler_session_send_frame.  If a data buffer is
 *  not ready to be returned, the plugin function should blocks.
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
