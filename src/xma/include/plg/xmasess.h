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
 * @ingroup xma_plg_intf
 * @file plg/xmasess.h
 * Abstract XMA plugin session base class
*/
#ifndef __XMA_SESS_H__
#define __XMA_SESS_H__

#include <stdbool.h>

#include "lib/xmahw.h"
#include "lib/xmares.h"

/**
 * @ingroup xmaplugin
 * @addtogroup xmasess xmasess.h
 * @{
*/

/**
 * @typedef XmaSessionType
 * Indicates what class of plugin this session represents
 *
 * @typedef XmaSession
 * Base class for all other session types
*/

/**
 * @enum XmaSessionType
 * Indicates what class of plugin this session represents
*/
typedef enum {
    XMA_SCALER = 0, /**< 0 */
    XMA_ENCODER,    /**< 1 */
    XMA_DECODER,    /**< 2 */
    XMA_FILTER,     /**< 3 */
    XMA_KERNEL,     /**< 4 */
} XmaSessionType;   /**< 5 */

/**
 * @struct XmaSession
 * Base class for all other session types
*/
typedef struct XmaSession {
    /** Subclass this session is a part of */
    XmaSessionType session_type;
    /** Hardware handle to kernel */
    XmaHwSession   hw_session;
    /** Opaque object tracking indexes to XMA resource managment database.
    Used internally. */
    XmaKernelRes   kern_res;
    /** For kernels that support channels, this is the channel id assigned
    by the plugin code. Initalized to -1. */
    int32_t        chan_id;
    /** Data allocated by XMA once for a given kernel during initialization
    and freed only after all sessions connected to a kernel have closed.
    Used to maintain global kernel state information as may be needed by kernel
    plugin developer. */
    void          *kernel_data;
    /** Private kernel data attached to a specific kernel session. Allocated
    by XMA prior to calling plugin init() and freed automatically as part of
    close. */
    void          *plugin_data;
} XmaSession;

/**
 * Determine if XmaSession is a member of XmaDecoderSession
*/
static inline bool is_xma_decoder(XmaSession *s)
{
    return s->session_type == XMA_DECODER ? true : false;
}

/**
 * Determine if XmaSession is a member of XmaEncoderSession
*/
static inline bool is_xma_encoder(XmaSession *s)
{
    return s->session_type == XMA_ENCODER ? true : false;
}

/**
 * Determine if XmaSession is a member of XmaScalerSession
*/
static inline bool is_xma_scaler(XmaSession *s)
{
    return s->session_type == XMA_SCALER ? true : false;
}

/**
 * Determine if XmaSession is a member of XmaFilterSession
*/
static inline bool is_xma_filter(XmaSession *s)
{
    return s->session_type == XMA_FILTER ? true : false;
}

/**
 * Determine if XmaSession is a member of XmaKernelSession
*/
static inline bool is_xma_kernel(XmaSession *s)
{
    return s->session_type == XMA_KERNEL ? true : false;
}

/**
 * Determine if XmaSession has been allocated a channel
*/
static inline bool xma_sess_has_chan(XmaSession *s)
{
    return s->chan_id < 0 ? false : true;
}
/**
 * @}
*/
#endif
