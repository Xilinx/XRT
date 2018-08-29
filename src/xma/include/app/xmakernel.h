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

/**
 * @ingroup xma_app_intf
 * @file app/xmakernel.h
 * XMA application interface to general purpose kernels
 */

#include "app/xmabuffers.h"
#include "app/xmaparam.h"
#include "lib/xmalimits.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @ingroup xma
 *  @addtogroup xmakernel xmakernel.h
 *  @{
 *  @section xmakernel_intro Xilinx Media Accelerator Any API
 *  The kernel interface provides a generic method for controlling a kernel
 *  that does not need to send and receive video frame data.  While it is
 *  possible to use the kernel plugin class for video, the class is intended
 *  for cases where only control information is required or the data is not
 *  readily classified as typical video frame data.
 *
 *  @subsection External Interface for XMA Kernel Interface
 *
 *  In most cases specific to video accelerators, the class of session
 *  typically fits into one of the followeing categories:
 *
 *  @li encoder
 *  @li decoder
 *  @li filter (with 1 input and 1 output)
 *  @li ABR scaler (with 1 input and multiple output)
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
 *  @code
 *  #include <xma.h>
 *
 *  int main(int argc, char *argv[])
 *  {
 *      int rc;
 *      char *yaml_filepath = argv[1];
 *
 *      // Other media framework initialization
 *      ...
 *
 *      rc = xma_initialize(yaml_filepath);
 *      if (rc != 0)
 *      {
 *          // Log message indicating XMA initialization failed
 *          printf("ERROR: Could not initialize XMA rc=%d\n\n", rc);
 *          return rc;
 *      }
 *
 *      // Other media framework processing
 *      ...
 *
 *      return 0;
 *  }
 *  @endcode
 *
 *  Assuming XMA initialization completes successfully, the kernel plugin
 *  must be initialized and optionally provided configuration data.  Once
 *  session initialization successfully completes, the application will
 *  send write requests, read requests or both during normal processing.
 *  Prior to ending the application, the session should be destroyed to
 *  gracefully clean-up any allocated resources.
 *
 *  The code snippet below demonstrates the creation of an XMA kernel
 *  session:
 *
 *  @code
 *  // Code snippet for creating a kernel session
 *  ...
 *  #include <xma.h>
 *  ...
 *  XmaKernelProperties props;
 *  XmaParameter        params[2];
 *  int32_t             channel_num = 2;
 *  uint64_t            data = 0x123456789UL;
 *
 *  // Setup kernel properties
 *  props.hwkernel_type = XMA_KERNEL_TYPE;
 *  strcpy(props.hwvendor_string, "Acme-widget-1");
 *
 *  params[0].name = "channel";
 *  params[0].type = XMA_INT32;
 *  params[0].length = sizeof(channel_num);
 *  params[0].value = &channel_num;
 *  params[1].name = "data";
 *  params[1].type = XMA_UINT64;
 *  params[1].length = sizeof(data);
 *  params[1].value = &data;
 *  props.params = params;
 *  props.param_cnt = 2;
 *
 *  // Create a kernel session based on the requested properties
 *  XmaKernelSession *session;
 *  session = xma_kernel_session_create(&props);
 *  if (!session)
 *  {
 *      // Log message indicating session could not be created
 *      // return from function
 *  }
 *  // Save returned session for subsequent calls.
 *  @endcode
 *
 *  The code snippet that follows demonstrates how to send data
 *  to the kernel session and receive any available data in response:
 *
 *  @code
 *  // Code snippet for sending data to the kernel and checking
 *  // if data is available.
 *
 *  // Other non-XMA related includes
 *  ...
 *  #include <xma.h>
 *
 *  // For this example it is assumed that session is a pointer to
 *  // a previously created kernel session and data is being sent to the
 *  // low-level plugin that triggers writing to registers or writing to
 *  // DDR device memory.  The handling of read and write requests is
 *  // left solely to the discretion of the low-level plugin implementation.
 *
 *  XmaParameter params_in[2];
 *  XmaParameter *params_out;
 *  int32_t       channel_num = 1;
 *  uint64_t      data = 0xabcd12345UL;
 *  int32_t       param_cnt;
 *  int32_t rc;
 *
 *  params[0].name = "channel";
 *  params[0].type = XMA_INT32;
 *  params[0].length = sizeof(channel_num);
 *  params[0].value = &channel_num;
 *  params[1].name = "data";
 *  params[1].type = XMA_UINT64;
 *  params[1].length = sizeof(data);
 *  params[1].value = &data;
 *  param_cnt = 2;
 *  rc = xma_kernel_session_write(session, params_in, param_cnt);
 *  if (rc != 0)
 *  {
 *      // Log error indicating write could not be accepted
 *      return rc;
 *  }
 *
 *  // Read data if it is available.
 *  int32_t param_cnt;
 *  rc = xma_kernel_session_read(session, params_out, &param_cnt);
 *  if (rc != 0)
 *  {
 *      // No data to return at this time
 *      // This may not be an error - app needs to decide
 *      return rc;
 *  }
 *
 *  // Iterate through returned parameters and do something with the data
 *  for (i=0; i<param_cnt; i++)
 *  {
 *      // Read returned data in params_out and print the value
 *      printf("name=%s, ", params_out[i].name);
 *      switch(params_out[i].type)
 *      {
 *          case XMA_STRING:
 *              printf("type=XMA_STRING,value=%s\n",
 *                  (char*)params_out[i].value);
 *              break;
 *          case XMA_INT32:
 *              printf("type=XMA_INT32,value=%d\n",
 *                  (int32_t*)params_out[i].value);
 *              break;
 *          case XMA_UINT32:
 *              printf("type=XMA_UINT32,value=%lu\n",
 *                  (uint32_t*)params_out[i].value);
 *              break;
 *          case XMA_INT64:
 *              printf("type=XMA_INT64,value=%l\n",
 *                  (int64_t*)params_out[i].value);
 *              break;
 *          case XMA_UINT64:
 *              printf("type=XMA_UINT64,value=%lu\n",
 *                  (uint64_t*)params_out[i].value);
 *              break;
 *      }
 *      ...
 *  }
 *  return rc;
 *  @endcode
 *
 *  This last code snippet demonstrates the interface for destroying the
 *  session.  This allows all allocated resources to be freed and made
 *  available to other processes.
 *
 *  @code
 *  // Code snippet for destroying a session once a stream has ended
 *
 *  // Other non-XMA related includes
 *  ...
 *  #include <xma.h>
 *
 *  // This example assumes that the session is a pointer to a previously
 *  // created XmaKernelSession
 *  int32_t rc;
 *  rc = xma_kernel_session_destroy(session);
 *  if (rc != 0)
 *  {
 *      // TODO: Log message that the destroy function failed
 *      return rc;
 *  }
 *  return rc;
 *  @endcode
 */

/* @} */

/**
 * @addtogroup xmakernel
 * @{
 */

/**
 * @typedef XmaKernelType
 * Description of kernel represented by XmaKernel
 *
 * @typedef XmaKernelProperties
 * Properties used to initialize kernel and specify which kernel to allocate
 *
 * @typedef XmaKernelSession
 * Opaque pointer to a kernel instance. Used to specify the kernel instance for
 * all kernel application interface APIs
*/

/**
 * @enum XmaKernelType
 * Description of kernel represented by XmaKernel
*/
typedef enum XmaKernelType
{
    XMA_KERNEL_TYPE = 1, /**< 1 */
} XmaKernelType;

/**
 * @struct XmaKernelProperties
 * Properties used to initialize kernel and specify which kernel to allocate
 *
 * XmaKernels represent unspecified or custom kernels that may not necessarily
 * fit an existing video kernel type.  As such, they may take custom parameters
 * as properties.  You should consult the documentation for the kernel plugin
 * to get a list of XmaParameter parameters needed to specify how the kernel
 * should be initalized.
*/
typedef struct XmaKernelProperties
{
    XmaKernelType   hwkernel_type; /**< requested kernel type */
    char            hwvendor_string[MAX_VENDOR_NAME]; /**< requested vendor */
    XmaParameter    *param; /**< kernel-specific custom initialization parameters */
} XmaKernelProperties;
/**
 * @}
 */

/* Forward declaration */
typedef struct XmaKernelSession XmaKernelSession;

/**
 *  @brief Create a kernel session
 *
 *  This function creates a kernel session and must be called prior to
 *  invoking other kernel session functions.  A session reserves hardware
 *  resources until session destroy function is called.
 *
 *  @param props  Pointer to a XmaKernelProperties structure that
 *                contains the key configuration properties needed for
 *                finding available hardware resource.
 *
 *  @return       Not NULL on success
 *  @return       NULL on failure
*/
XmaKernelSession*
xma_kernel_session_create(XmaKernelProperties *props);

/**
 *  @brief Destroy a kernel session
 *
 *  This function destroys a kernel session that was previously created
 *  with the xma_kernel_session_create function.
 *
 *  @param session  Pointer to XmaKernelSession created with
 *                  xma_kernel_session_create
 *
 *  @return        XMA_SUCCESS on success
 *  @return        XMA_ERROR on failure.
*/
int32_t
xma_kernel_session_destroy(XmaKernelSession *session);

/**
 *  @brief Write data to the hardware accelerator
 *
 *  This function writes data to the hardware accelerator kernel.
 *  The meaning of the data is managed by the caller and low-level
 *  XMA plugin. This means that the data provided could contain
 *  information about how kernel registers are programmed, how device
 *  DDR memory is set, or some combination of both.
 *
 *  @param session    Pointer to session created by xm_enc_sesssion_create
 *  @param param      Pointer to an XmaParameter list
 *  @param param_cnt  Number of parameters provided in the list
 *
 *  @return        XMA_SUCCESS on success.
 *  @return        XMA_ERROR on error.
*/
int32_t
xma_kernel_session_write(XmaKernelSession  *session,
                         XmaParameter      *param,
                         int32_t            param_cnt);

/**
 *  @brief Read data from the hardware accelerator
 *
 *  This function reads data from the hardware accelerator kernel.
 *  The meaning of the data is managed by the caller and low-level
 *  XMA plugin.
 *
 *  @param session   Pointer to session created by xm_enc_sesssion_create
 *  @param param     Pointer to an XmaParameter
 *  @param param_cnt Pointer number of parameters in the list
 *
 *  @return        XMA_SUCCESS on success.
 *  @return        XMA_ERROR on error.
*/
int32_t
xma_kernel_session_read(XmaKernelSession  *session,
                        XmaParameter      *param,
                        int32_t           *param_cnt);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
