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
#ifndef _XMAAPP_KERNEL_H_
#define _XMAAPP_KERNEL_H_

#include "app/xmabuffers.h"
#include "app/xmaparam.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DOC:
 *  The kernel interface provides a generic method for controlling a kernel
 *  that does not need to send and receive video frame data.  While it is
 *  possible to use the kernel plugin class for video, the class is intended
 *  for cases where only control information is required or the data is not
 *  readily classified as typical video frame data.
 *
 *  In most cases specific to video accelerators, the class of session
 *  typically fits into one of the followeing categories:
 *
 *  1. encoder
 *  2. decoder
 *  3. filter (with 1 input and 1 output)
 *  4. ABR scaler (with 1 input and multiple output)
 *
 *  Each of the classes above expect frame data as input/output or both.  As a
 *  result, these APIs provide a convenient way to send and/or receive frame
 *  data.  In cases where frame data is not needed or more control over the
 *  type of data sent/recevied is needed, the kernel class of plugin may be a
 *  better fit.  In this case, data is transferred between the application
 *  and the plugin via one or more XmaParameter.  The XmaParameter allows
 *  for a number intrinsic types as well as user-defined data types.  The XMA
 *  does not interpret the XmaParameter.  Instead, these parameters are passed
 *  by the XMA from the application to the plugin.  As a result, it is up to
 *  the application and plugin to decide the meaning of the XmaParameter.
 *
 */

/**
 * enum XmaKernelType - Description of kernel represented by XmaKernel
*/
typedef enum XmaKernelType
{
    XMA_KERNEL_TYPE = 1, /**< 1 */
} XmaKernelType;

/**
 * struct XmaKernelProperties - XmaKernels represent unspecified or custom kernels that may not necessarily
 * fit an existing video kernel type.  As such, they may take custom parameters
 * as properties.  You should consult the documentation for the kernel plugin
 * to get a list of XmaParameter parameters needed to specify how the kernel
 * should be initalized.
*/
typedef struct XmaKernelProperties
{
    /** requested kernel type */
    XmaKernelType   hwkernel_type;
    /** requested vendor */
    char            hwvendor_string[MAX_VENDOR_NAME];
    /** array of kernel-specific custom initialization parameters */
    XmaParameter    *params;
    /** count of custom parameters for port */
    uint32_t        param_cnt;
    int32_t         dev_index;
    int32_t         cu_index;
    int32_t         ddr_bank_index;//Used for allocating device buffers. Used only if valid index is provide (>= 0); value of -1 imples that XMA should select automatically and then XMA will set it with bank index used automatically
    int32_t         channel_id;
    char            *plugin_lib;
    int32_t         reserved[4];
} XmaKernelProperties;


/* Forward declaration */
typedef struct XmaKernelSession XmaKernelSession;

/**
 *  xma_kernel_session_create() - This function creates a kernel session and must be called prior to
 *  invoking other kernel session functions.  A session reserves hardware
 *  resources until session destroy function is called.
 *
 *  @props:  Pointer to a XmaKernelProperties structure that
 * contains the key configuration properties needed for
 * finding available hardware resource.
 *
 *  RETURN:       Not NULL on success
 * 
 * NULL on failure
 *
 *  Note: session create & destroy are thread safe APIs
*/
XmaKernelSession*
xma_kernel_session_create(XmaKernelProperties *props);

/**
 *  xma_kernel_session_destroy() - This function destroys a kernel session that was previously created
 *  with the xma_kernel_session_create function.
 *
 *  @session:  Pointer to XmaKernelSession created with
 *                  xma_kernel_session_create
 *
 *  RETURN:        XMA_SUCCESS on success
 *          
 * XMA_ERROR on failure.
 *
 *  Note: session create & destroy are thread safe APIs
*/
int32_t
xma_kernel_session_destroy(XmaKernelSession *session);

/**
 *  xma_kernel_session_write() - This function invokes plugin->write fucntion 
 * assigned to this session which handles sending data to the hardware kernel.  
 *  The meaning of the data is managed by the caller and low-level
 *  XMA plugin. This means that the data provided could contain
 *  information about how kernel registers are programmed, how device
 *  DDR memory is set, or some combination of both.
 *
 *  @session:    Pointer to session created by xm_enc_sesssion_create
 *  @param:      Pointer to an XmaParameter list
 *  @param_cnt:  Number of parameters provided in the list
 *
 *  RETURN: XMA_SUCCESS on success.
 *          
 * XMA_ERROR on error.
*/
int32_t
xma_kernel_session_write(XmaKernelSession  *session,
                         XmaParameter      *param,
                         int32_t            param_cnt);

/**
 *  xma_kernel_session_read() - This function invokes plugin->read
 * assigned to this session which handles obtaining output data from the hardware kernel.  
 *  The meaning of the data is managed by the caller and low-level
 *  XMA plugin.
 *
 *  @session:   Pointer to session created by xm_enc_sesssion_create
 *  @param:     Pointer to an XmaParameter
 *  @param_cnt: Pointer number of parameters in the list
 *
 *  RETURN:        XMA_SUCCESS on success.
 *          
 * XMA_ERROR on error.
*/
int32_t
xma_kernel_session_read(XmaKernelSession  *session,
                        XmaParameter      *param,
                        int32_t           *param_cnt);

#ifdef __cplusplus
}
#endif

#endif
