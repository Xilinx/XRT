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
#ifndef _XMAAPP_FILTER_H_
#define _XMAAPP_FILTER_H_

#include "app/xmabuffers.h"
#include "app/xmaparam.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * enum XmaFilterType - Identifier specifying precise type of video filter during session creation
*/
typedef enum XmaFilterType
{
    XMA_2D_FILTER_TYPE = 1, /**< 1 */
} XmaFilterType;

/**
 * struct XmaFilterPortProperties - Properties necessary for specifying how an input or output port
 * should be configured by the plugin.
*/
typedef struct XmaFilterPortProperties
{
    /* core filter port properties */
    XmaFormatType   format; /**< video format entering/leaving port */
    int32_t         bits_per_pixel; /**< bits per pixel of video format */
    int32_t         width; /**< width in pixels of data */
    int32_t         height; /**< height in pixels of data */
    /** framerate data structure specifying frame rate per second */
    XmaFraction     framerate;
    int32_t         stride; /**< stride of video data row */
    /* user-defineable properties */
    XmaParameter    *params; /**< array of custom parameters for port */
    uint32_t        param_cnt; /**< count of custom parameters for port */
} XmaFilterPortProperties;

/* Forward declaration */
typedef struct XmaSession XmaSession;
typedef struct XmaFilterSession XmaFilterSession;

/**
 * struct XmaFilterProperties - Properties necessary for specifying which filter kernel to select and how
 * it should be configured by the plugin.
*/
typedef struct XmaFilterProperties
{
    /* core filter properties */
    /** Specifying type of filter to reserve; @see XmaFilterType */
    XmaFilterType            hwfilter_type;
    /** kernel receiving data from this filter */
    XmaSession              *destination;
    /** Vendor request for kernel session */
    char                     hwvendor_string[MAX_VENDOR_NAME];
    /** input data port instance */
    XmaFilterPortProperties  input;
    /** output data port instance */
    XmaFilterPortProperties  output;
    /* user-defineable properties */
    /** array of custom parameters for port */
    XmaParameter             *params;
    /** count of custom parameters for port */
    uint32_t                 param_cnt;
    int32_t         dev_index;
    int32_t         cu_index;
    char            *cu_name;
    int32_t         ddr_bank_index;//Used for allocating device buffers. Used only if valid index is provide (>= 0); value of -1 imples that XMA should select automatically and then XMA will set it with bank index used automatically
    int32_t         channel_id;
    char            *plugin_lib;
    bool            ooo_execution;//Out of order execution of cu cmds
    int32_t         reserved[4];
} XmaFilterProperties;

/**
 *  xma_filter_session_create() - This function creates a filter session and must be called prior to
 *  filtering a frame.  A session reserves hardware resources for the
 *  duration of a video stream. The number of sessions allowed depends on
 *  a number of factors that include: resolution, frame rate, bit depth,
 *  and the capabilities of the hardware accelerator.
 *
 *  @props: Pointer to a XmaFilterProperties structure that
 * contains the key configuration properties needed for
 * finding available hardware resource.
 *
 *  RETURN:      Not NULL on success
 * 
 * NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaFilterSession*
xma_filter_session_create(XmaFilterProperties *props);

/**
 *  xma_filter_session_destroy() - This function destroys an filter session that was previously created
 *  with the @ref xma_filter_session_create function.
 *
 *  @session:  Pointer to XmaFilterSession created with xma_filter_session_create
 *
 * RETURN:        XMA_SUCCESS on success
 *          
 * XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t
xma_filter_session_destroy(XmaFilterSession *session);

/**
 *  xma_filter_session_send_frame() - This function invokes plugin->send_frame fucntion 
 * assigned to this session which handles sending frames to the hardware decoder.  
 *
 *  @session:  Pointer to session created by xma_filter_session_create
 *  @frame:    Pointer to a frame to be filtered. If the filter is
 *  buffering input, then an XmaFrame with a NULL data buffer
 *  pointer to the first data buffer must be sent to flush the filter and
 *  to indicate that no more data will be sent:
 *  XmaFrame.data[0].buffer = NULL
 *  The application must then check for XMA_FLUSH_AGAIN for each such call
 *  when flushing the last few frames.  When XMA_EOS is returned, no new
 *  data may be collected from the filter.
 *
 *  RETURN:        XMA_SUCCESS on success and the filter is ready to
 * produce output
 *          
 * XMA_SEND_MORE_DATA if the filter is buffering input frames
 *          
 * XMA_FLUSH_AGAIN when flushing filter with a null frame
 *          
 * XMA_EOS when the filter has been flushed of all residual frames
 *          
 * XMA_ERROR on error
*/
int32_t
xma_filter_session_send_frame(XmaFilterSession *session,
                              XmaFrame         *frame);

/**
 *  xma_filter_session_recv_frame() - This function invokes plugin->recv_frame 
 * assigned to this session which handles obtaining output frame with filtered data from the hardware filter.  
 * This function is called after
 *  calling the function xma_filter_session_send_frame.  If a data buffer is
 *  not ready to be returned, the plugin function should block.
 *
 *  @session:    Pointer to session created by xma_filter_session_create
 *  @frame_list: Pointer to a list of XmaFrame structures
 *
 *  RETURN:        XMA_SUCCESS on success.
 *          
 * XMA_ERROR on error.
*/
int32_t
xma_filter_session_recv_frame(XmaFilterSession *session,
                              XmaFrame         *frame);


#ifdef __cplusplus
}
#endif

#endif
