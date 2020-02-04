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
#ifndef _XMA_CONNECT_H_
#define _XMA_CONNECT_H_

/**
 *  @file
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @addtogroup xmaconnect
 *  @{
 *  @section xmaconn_intro Connection Management API Overview
 *
 *  As part of the XMA, it is possible for two or more kernel components
 *  to send and receive data via device DDR memory.  Sending and receiving
 *  data through device DDR is more efficient than having to copy buffers
 *  to host memory.  This is especially true when raw video frames are passed
 *  between kernels as this reduces latency and PCIe bandwidth.
 *
 *  In order for kernels to communicate via device DDR, a number of conditions
 *  must be met.  First, the XMA configuration file must specify that "zerocopy"
 *  is enabled.  Second, the kernels must access the same DDR bank.  For example,
 *  a scaler kernel can only perform "zerocopy" with an encoder kernel that is on
 *  the same DDR bank.  Third, the kernel components must be connectable.  This
 *  means that the same format is expected from the sender and receiver and no
 *  intermediate component is placed between.
 *
 *  The XMA Connection Management API is used internally to ensure that all of
 *  the above conditions are met by tracking components that can participate in
 *  "zerocopy" and connecting these components when possible. If any
 *  of the conditions cannot be met, then communication between kernels falls
 *  back to copying data to host memory.
 *
 *  Since higher-level frameworks such as FFmpeg separate components into discrete
 *  plugins, the XMA Connection Management API must hook into session creation,
 *  send, and receive functions.  During session creation, a pending connection
 *  is established from the sender side of the connection and upon connection of
 *  a compatible receiver, an active connection is created. If an active connection
 *  exists, then the sender will use device buffers supplied by the receiver. These
 *  device buffers are encapsulated into an XmaFrame with a special property
 *  indicating a hardware buffer type.  Although the transferring of data is
 *  performed via DDR device memory, XmaFrame data is still forwarded to the
 *  receiver host plugin.  This is necessary for two reasons.  First, the
 *  higher-level framework expects frames to be passed as part of the interface.
 *  Second, the receiving component needs to be signalled when the frame has been
 *  written and is ready to be processed.
 *
 *  The XMA Connection Management API assumes that sessions are created in a
 *  specific order.  Specifically, if a pipeline comprised of a decoder, ABR scaler,
 *  and ABR encoder was requested, it is expected that these components are created
 *  in the same order (from input to output).  If the pipeline is created in the
 *  reverse order (from output to input), the connection manager will likely fall
 *  back to host copies versus "zerocopy".  In addition, if a component is placed
 *  between connectable XMA compoents that are not known to the XMA, unpredicatble
 *  results will occur since hardware buffers cannot be used by non-XMA components.
 *  If there is any doubt as to whether or not non-XMA components are in a pipeline,
 *  then the safest strategy is to disable "zerocopy" in the configuration file.
 *  @subsection Connection Management API details
 *
 *  The internal interface to the Connection Management API is comprised of the
 *  following functions:
 *
 *  @li @ref xma_connect_alloc()
 *  @li @ref xma_connect_free()
 *
 */

/* @} */

/**
 * @addtogroup xmaconnect
 * @{
 */

typedef enum XmaConnectType
{
    XMA_CONNECT_SENDER = 0,
    XMA_CONNECT_RECEIVER
} XmaConnectType;

typedef enum XmaConnectState
{
    XMA_CONNECT_UNUSED = 0,
    XMA_CONNECT_PENDING_ACTIVE,
    XMA_CONNECT_ACTIVE,
    XMA_CONNECT_PENDING_DELETE
} XmaConnectState;

/**
 * @}
 */

/**
 * @addtogroup xmaconnect
 * @{
 */
typedef struct XmaSession XmaSession;

typedef struct XmaEndpoint
{
    XmaSession      *session;
    XmaFormatType   format;
    int32_t         dev_id;
    int32_t         bits_per_pixel;
    int32_t         width;
    int32_t         height;
} XmaEndpoint;

typedef struct XmaConnect
{
    XmaConnectState     state;
    XmaEndpoint        *sender;
    XmaEndpoint        *receiver;
} XmaConnect;

/**
 *  @brief Allocate an XMA connection
 *
 *  This function allocates a connection and is called internally by the XMA
 *  framework.  This function first checks if "zerocopy" is enabled.  If not,
 *  the remainder of the function is skipped.  If "zerocopy" is enabled, then
 *  a connection entry is allocated if possible.
 *
 *  Since a component can act as a receiver, sender, or both, the type of
 *  connection is required in order to create the correct entry type.  If both
 *  types are required, then this function should be called twice in order to
 *  create two unique entries.  In addition, if multiple outputs are supported
 *  by the underlying connection (such as an ABR Scaler), then this function
 *  must be called for each output.
 *
 *  @param session Pointer to a XmaEndpoint
 *
 *  @param type    Type of connection to allocate sender or receiver
 *
 *  @return        Connection handle
 *                 -1 indicates a connection entry could not be
 *                    created
*/
int32_t
xma_connect_alloc(XmaEndpoint *endpt, XmaConnectType type);

/**
 *  @brief Free an XMA connection
 *
 *  This function frees an existing connection entry and reclaims it for
 *  another connection.
 *
 *  @param c_handle Connection handle created with
 *                  xma_connect_alloc function
 *
 *  @param type    Type of connection to free sender or receiver
 *
 *  @return         0 on success
 *                 -1 on failure.
*/
int32_t
xma_connect_free(int32_t c_handle, XmaConnectType type);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
