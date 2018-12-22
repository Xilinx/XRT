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

/**
 * @ingroup xma_app_intf
 * @file app/xmaerror.h
 * XMA library return and error codes
*/
#ifndef _XMAAPP_ERROR_H_
#define _XMAAPP_ERROR_H_
#include <stdio.h>

/**
 * @ingroup xma
 * @addtogroup xmaerr xmaerror.h
 * @{
*/

/**
 *  @def XMA_SUCCESS
 *  Normal return
 *  @def XMA_SEND_MORE_DATA
 *  More data needed by kernel before receive function can be called
 *  @def XMA_END_OF_FILE
 *  End of data stream
 *  @def XMA_TRY_AGAIN
 *  This can be returned by both xma_enc_session_send_frame() and
 *  xma_enc_session_recv_data(). When returned by xma_enc_session_send_frame(),
 *  it means that the component is busy and the input is not consumed. The
 *  user needs to resend the data again in the next call. User needs to call
 *  xma_enc_session_recv_data() to pull out data, before calling
 *  xma_enc_session_send_frame again.
 *  When returned by xma_enc_session_recv_data(), it means that new input data
 *  is required by the component to return new output, so user needs to call
 *  xma_enc_session_send_frame().
 *  A component must not return XMA_TRY_AGAIN for both sending and receiving, as
 *  this would put the component user into an endless loop.
 *  @def XMA_ERROR
 *  Unspecified error has occured. Check log files for more info.
 *  @def XMA_ERROR_INVALID
 *  Invalid or malformed data has been passed to function.
 *  @def XMA_ERROR_NO_KERNEL
 *  No session could be created because no kernel resource exists or is available.
 *  @def XMA_ERROR_NO_DEV
 *  No session could be created because there is no free device.
 *  @def XMA_ERROR_TIMEOUT
 *  Routine timed out.
*/
#define XMA_SUCCESS          (0)
#define XMA_SEND_MORE_DATA   (1)
#define XMA_END_OF_FILE      (2)
#define XMA_EOS      	     (3)
#define XMA_FLUSH_AGAIN      (4)
#define XMA_TRY_AGAIN        (5)

#define XMA_ERROR           (-1)
#define XMA_ERROR_INVALID   (-2)
#define XMA_ERROR_NO_KERNEL (-3)
#define XMA_ERROR_NO_DEV    (-4)
#define XMA_ERROR_TIMEOUT   (-5)

#define XMA_ERROR_MSG "XMA_ERROR: error condition\n"
#define XMA_ERROR_INVALID_MSG "XMA_ERROR_INVALID: invalid input supplied\n"
#define XMA_ERROR_NO_KERNEL_MSG "XMA_ERROR_NO_KERNEL: no kernel resource available\n"
#define XMA_ERROR_NO_DEV_MSG "XMA_ERROR_NO_DEV: no device resource available\n"
#define XMA_ERROR_TIMEOUT_MSG "XMA_ERROR_TIMEOUT: time alloted for call exceeded\n"

/**
 * Copy error message to buffer
 *
 * @param err return code for which a string description is requested
 * @param buff string buffer to hold string description
 * @param sz size of string buffer
 *
 * @returns pointer to buff populated with error string corresponding to err
*/
static inline char *xma_perror(int err, char *buff, size_t sz)
{
    switch (err) {
    case XMA_ERROR:
        snprintf(buff, sz, XMA_ERROR_MSG);
        break;
    case XMA_ERROR_INVALID:
        snprintf(buff, sz, XMA_ERROR_INVALID_MSG);
        break;
    case XMA_ERROR_NO_KERNEL:
        snprintf(buff, sz, XMA_ERROR_NO_KERNEL_MSG);
        break;
    case XMA_ERROR_NO_DEV:
        snprintf(buff, sz, XMA_ERROR_NO_DEV_MSG);
        break;
    case XMA_ERROR_TIMEOUT:
        snprintf(buff, sz, XMA_ERROR_TIMEOUT_MSG);
        break;
    }
    return buff;
}
/** @} */
#endif
