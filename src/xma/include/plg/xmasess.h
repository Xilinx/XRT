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
 * @def XMA_MAX_CHAN_LOAD
 * Maximum aggreate load for a kernel supporting
 * channels.
 *
 * All plugins should calculate and normalize per channel load
 * against this value (conceptually, this number is % capacity
 * to 3 significant figures -- a load of 475 = 47.5%)
*/
#define XMA_MAX_CHAN_LOAD 1000
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
 * @struct XmaChannel
 *
 * This is the output parameter for the alloc_chan_mp function. The
 * protocol for filling out this parameter is as follows:
 * A plugin, after evaluating the session data (*pending_sess),
 * should fill out the fields of this data structure as follows:
 *
 * chan_load:
 * Compute a load factor for the new session.  Channel load should
 * be a value of between 1-1000 (with 1000 representing a maximal
 * value indicating that the current kernel is loaded 100%).  Compare
 * the computed load factor with the curr_kern_load value passed
 * into the callback.  If the curr_kern_load + your newly computed
 * load factor > 1000, then the channel request should be rejected
 * and channel id should = -1;
 *
 * chan_id:
 * If a channel is allocated, fill the chan_id property with the
 * assigned channel number.
 *
 * Example:
 * Accept a new session as channel 3 utilzing approximately 45.7% of the kernel.
 * new_channel->chan_id = 3
 * new_channel->chan_load = 457
 *
 * @note: in all cases wherein a channel request is rejected, the alloc_chan
 * implementation should return an error code status.
*/
typedef struct {
    int32_t  chan_id; /* assigned channel id */
    uint16_t chan_load; /* load value (0-1000); % to 3 sig figs */
} XmaChannel;

/**
 * Optional plugin callback called when app calls xma_enc_session_create()
 * Common to all core plugin kernel types (encoder, decoder, filter, scaler)
 *
 * Kernels which support channels that are capabile of being shared across
 * processes should implement this callback.
 * @param pending_sess new session requesting access to this kernel
 * @param curr_kern_load aggreate load of all previously approved channel
 *  requests
 * @param chan_ids sorted array of channel ids already assigned to active
 *  channels on this kernel
 * @param chan_ids_cnt size of chan_ids array
 * @param new_channel output parameter to be filled in by plugin containing
 *  the newly assigned channel for the pending_sess (if approved) and the
 *  calculated load value for this channel
 *
 * @return
 * XMA_SUCCESS if a channel has been allocated to this kernel.
 * XMA_ERROR_NO_CHAN if no additional channels can be allocated to this kernel
 * XMA_ERROR_NO_CHAN_CAP if the channel exceeds available capacity of available
 *  channel
*/
typedef int32_t (*xma_plg_alloc_chan_mp)(XmaSession *pending_sess,
                                      uint16_t    curr_kern_load,
                                      int32_t    *chan_ids,
                                      uint8_t     chan_ids_cnt,
                                      XmaChannel *new_channel);
/**
 * Optional plugin callback called when app calls xma_enc_session_create()
 * Common to all core plugin kernel types (encoder, decoder, filter, scaler)
 *
 * Kernels which support channels that are NOT capabile of being shared across
 * processes should implement this callback. This is a legacy callback.  All
 * new plugins should implement the multi-process version of alloc_chan.
 *
 * @param pending_sess new session requesting access to this kernel
 * @param curr_sess array of session objects which are already actively using
 *  this kernel
 * @param sess_cnt size of curr_sess array
 *
 * @return
 * XMA_SUCCESS if a channel has been allocated to this kernel.
 * XMA_ERROR_NO_CHAN if no additional channels can be allocated to this kernel
 * XMA_ERROR_NO_CHAN_CAP if the channel exceeds available capacity of available
 *  channel
*/
typedef int32_t (*xma_plg_alloc_chan)(XmaSession *pending_sess,
                                      XmaSession **curr_sess,
                                      uint32_t    sess_cnt);

/**
 * Determine next available channel id from array of in-use channel ids
 *
 * Helper function which can be used within a plugin's implementation of alloc_chan
 * to determine the next available channel id from among the array of chan_ids
 * currently in-use.  Relevant to xma_plg_alloc_chan_mp.
 *
 * @param chan_ids array of channel ids in-use
 * @param cnt size of chan_ids array
 *
 * @return next available channel id
*/
static inline int32_t xma_plg_find_next_chan_id(int32_t *chan_ids, uint8_t cnt)
{
    int i;

    for(i = 0; i < cnt && i == chan_ids[i]; i++);

    return i;
}

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
    /** Private stats data attached to a specific session. This field is
    allocated and managed by XMA for each session type. */
    void          *stats;
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
