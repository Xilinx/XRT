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
#include "lib/xmalimits.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @ingroup xma_app_intf
 * @file app/xmafilter.h
 * Application interface for creating and controlling video filter kernels
*/

/**
 * @ingroup xma
 * @addtogroup xmafilter xmafilter.h
 * Application interface for creating and controlling video filter kernels
 * @{
*/

/**
 * @typedef XmaFilterType
 * Identifier specifying precise type of video filter during session creation
 *
 * @typedef XmaFilterPortProperties
 * Properties necessary for specifying how an input or output port
 * should be configured by the plugin.
 *
 * @typedef XmaFilterSession
 * Opaque pointer to a filter kernel instance. Used to specify the filter
 * instance for all filter application interface APIs
 *
 * @typedef XmaFilterProperties
 * Properties necessary for specifying which filter kernel to select and how
 * it should be configured by the plugin.
*/

/**
 * @enum XmaFilterType
 * Identifier specifying precise type of video filter during session creation
*/
typedef enum XmaFilterType
{
    XMA_2D_FILTER_TYPE = 1, /**< 1 */
} XmaFilterType;

/**
 * @struct XmaFilterPortProperties
 * Properties necessary for specifying how an input or output port
 * should be configured by the plugin.
*/
typedef struct XmaFilterPortProperties
{
    /* core filter port properties */
    XmaFormatType   format; /**< video format entering/leaving port */
    int32_t         bits_per_pixel; /**< bits per pixel of video format */
    int32_t         width; /**< width in pixels of data */
    int32_t         height; /**< height in pixels of data */
    int32_t         stride; /**< stride of video data row */
    /* user-defineable properties */
    XmaParameter    *params; /**< array of custom parameters for port */
    uint32_t        param_cnt; /**< count of custom parameters for port */
} XmaFilterPortProperties;

/* Forward declaration */
typedef struct XmaSession XmaSession;
typedef struct XmaFilterSession XmaFilterSession;

/**
 * @struct XmaFilterProperties
 * Properties necessary for specifying which filter kernel to select and how
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
} XmaFilterProperties;

/**
 *  @brief Create a filter session
 *
 *  This function creates a filter session and must be called prior to
 *  filtering a frame.  A session reserves hardware resources for the
 *  duration of a video stream. The number of sessions allowed depends on
 *  a number of factors that include: resolution, frame rate, bit depth,
 *  and the capabilities of the hardware accelerator.
 *
 *  @param props Pointer to a XmaFilterProperties structure that
 *               contains the key configuration properties needed for
 *               finding available hardware resource.
 *
 *  @return      Not NULL on success
 *  @return      NULL on failure
*/
XmaFilterSession*
xma_filter_session_create(XmaFilterProperties *props);

/**
 *  @brief Destroy a filter session
 *
 *  This function destroys an filter session that was previously created
 *  with the @ref xma_filter_session_create function.
 *
 *  @param session  Pointer to XmaFilterSession created with
                    xma_filter_session_create
 *
 *  @return        XMA_SUCCESS on success
 *  @return        XMA_ERROR on failure.
*/
int32_t
xma_filter_session_destroy(XmaFilterSession *session);

/**
 *  @brief Send a frame for filtering to the hardware accelerator
 *
 *  This function sends a frame to the hardware filter.  If a frame
 *  buffer is not available and this interface will block.
 *
 *  @param session  Pointer to session created by xma_filter_session_create
 *  @param frame    Pointer to a frame to be filtered
 *
 *  @return        XMA_SUCCESS on success.
 *  @return        XMA_SEND_MORE_DATA if more data is needed before output can
 *                  be generated
 *  @return        XMA_ERROR on error.
*/
int32_t
xma_filter_session_send_frame(XmaFilterSession *session,
                              XmaFrame         *frame);

/**
 *  @brief Receive one or more frames from the hardware accelerator
 *
 *  This function populates a list of XmaFrame buffers with the filtered
 *  data returned from the hardware accelerator. This function is called after
 *  calling the function xma_filter_session_send_frame.  If a data buffer is
 *  not ready to be returned, this function blocks.
 *
 *  @param session    Pointer to session created by xma_filter_session_create
 *  @param frame_list Pointer to a list of XmaFrame structures
 *
 *  @return        XMA_SUCCESS on success.
 *  @return        XMA_ERROR on error.
*/
int32_t
xma_filter_session_recv_frame(XmaFilterSession *session,
                              XmaFrame         *frame);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
