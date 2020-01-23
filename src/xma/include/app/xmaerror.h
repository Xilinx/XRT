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

#ifndef _XMAAPP_ERROR_H_
#define _XMAAPP_ERROR_H_
#include <stdio.h>

/**
 * DOC:
 *  @def @XMA_SUCCESS - Normal return
 * 
 *  @def @XMA_SEND_MORE_DATA - More data needed by kernel before receive function can be called
 * 
 *  @def @XMA_END_OF_FILE - End of data stream
 * 
 *  @def @XMA_TRY_AGAIN - This can be returned by both xma_enc_session_send_frame() and
 *  xma_enc_session_recv_data(). When returned by xma_enc_session_send_frame(),
 *  it means that the component is busy and the input is not consumed. The
 *  user needs to resend the data again in the next call. User needs to call
 *  xma_enc_session_recv_data() to pull out data, before calling
 *  xma_enc_session_send_frame again.
 * 
 *  When returned by xma_enc_session_recv_data(), it means that new input data
 *  is required by the component to return new output, so user needs to call
 *  xma_enc_session_send_frame().
 * 
 *  A component must not return XMA_TRY_AGAIN for both sending and receiving, as
 *  this would put the component user into an endless loop.
 * 
 *  @def @XMA_ERROR - Unspecified error has occured. Check log files for more info.
 * 
 *  @def @XMA_ERROR_INVALID - Invalid or malformed data has been passed to function.
 * 
 *  @def @XMA_ERROR_NO_KERNEL - No session could be created because no kernel resource exists or is available.
 * 
 *  @def @XMA_ERROR_NO_DEV - No session could be created because there is no free device.
 * 
 *  @def @XMA_ERROR_TIMEOUT - Routine timed out.
 * 
 *  @def @XMA_ERROR_NO_CHAN - No channels remain to be allocated on the kernel
 * 
 *  @def @XMA_ERROR_NO_CHAN_CAP - The session would exceed the remaining channel capability on the kernel
*/

//See recommended API flow diagram for more info
#define XMA_SUCCESS             (0)
#define XMA_SEND_MORE_DATA      (1) //Do NOT use xma_recv_xxx API as no output is available. So use xma_send_xxx API
#define XMA_END_OF_FILE         (2)
#define XMA_EOS      	        (3)
#define XMA_FLUSH_AGAIN         (4)
#define XMA_TRY_AGAIN           (5) //Valid only for xma_send_xx; Resend the same data again
#define XMA_RESEND_AND_RECV     (6) //Can use xma_recv_xxx as well as xma_send_xxx API. But resend the same data again

#define XMA_ERROR               (-1)
#define XMA_ERROR_INVALID       (-2)
#define XMA_ERROR_NO_KERNEL     (-3)
#define XMA_ERROR_NO_DEV        (-4)
#define XMA_ERROR_TIMEOUT       (-5)
#define XMA_ERROR_NO_CHAN       (-6)
#define XMA_ERROR_NO_CHAN_CAP   (-7)

#define XMA_ERROR_MSG             "XMA_ERROR: error condition\n"
#define XMA_ERROR_INVALID_MSG     "XMA_ERROR_INVALID: invalid input supplied\n"
#define XMA_ERROR_NO_KERNEL_MSG   "XMA_ERROR_NO_KERNEL: no kernel resource available\n"
#define XMA_ERROR_NO_DEV_MSG      "XMA_ERROR_NO_DEV: no device resource available\n"
#define XMA_ERROR_TIMEOUT_MSG     "XMA_ERROR_TIMEOUT: time alloted for call exceeded\n"
#define XMA_ERROR_NO_CHAN_MSG     "XMA_ERROR_NO_CHAN: no more channels available on kernel\n"
#define XMA_ERROR_NO_CHAN_CAP_MSG "XMA_ERROR_NO_CHAN_CAP: session request exceeds available channel capacity\n"

/**
 * xma_perror() - Copy error message to buffer
 *
 * @err: return code for which a string description is requested
 * @buff: string buffer to hold string description
 * @sz: size of string buffer
 *
 * RETURN: pointer to buff populated with error string corresponding to err
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
    case XMA_ERROR_NO_CHAN:
        snprintf(buff, sz, XMA_ERROR_NO_CHAN_MSG);
        break;
    case XMA_ERROR_NO_CHAN_CAP:
        snprintf(buff, sz, XMA_ERROR_NO_CHAN_CAP_MSG);
        break;
    }
    return buff;
}

#endif
