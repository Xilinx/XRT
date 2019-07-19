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
#ifndef _XMA_BUFFERS_H_
#define _XMA_BUFFERS_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "xmalimits.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * DOC: 
 * Video buffer data structures needed for sharing and receiving data from
 * kernels. Library functions for allocating host buffers as well as buffer
 * data structures for sending/receiving data to/from video kernels.
*/

/**
 * struct XmaFraction - Used for describing video frame rates
*/
typedef struct XmaFraction
{
    int32_t numerator; /**< numerator of fraction */
    int32_t denominator; /**< denominator of fraction */
} XmaFraction;

/**
 * enum XmaBufferType - Describes the location of a buffer. Device buffers
 * reside on DDR banks located on the PCIe board hosting
 * the device.
*/
typedef enum XmaBufferType
{
    XMA_HOST_BUFFER_TYPE = 1, /**< 1 */
    XMA_DEVICE_BUFFER_TYPE, /**< 2 Has both host and device allocated memory*/
    XMA_DEVICE_ONLY_BUFFER_TYPE, /**< 3 Has only device memory. Use for zero copy*/
} XmaBufferType;

typedef struct XmaBufferObj
{
   uint8_t* data;
   uint64_t size;
   uint64_t paddr;
   int32_t  bank_index;
   int32_t  dev_index;
   bool     device_only_buffer;
   void*    private_do_not_touch;

} XmaBufferObj;

/**
 * struct XmaBufferRef - Reference counted buffer used in XmaFrame and XmaDataBuffer
 *
*/
typedef struct XmaBufferRef
{
    int32_t         refcount; /**< references to buffer */
    XmaBufferType   buffer_type; /**< location of buffer */
    void           *buffer; /**< data */
    bool            is_clone; /**< buffer member allocated externally */
    XmaBufferObj    *xma_device_buf; /* xma_device_buffer if allocated */
} XmaBufferRef;

/**
 * enum XmaFormatType - ID describing fourcc format of video frame buffer
*/
typedef enum XmaFormatType
{
    XMA_NONE_FMT_TYPE = 0, /**< 0 */
    XMA_YUV420_FMT_TYPE, /**< 1 */
    XMA_YUV422_FMT_TYPE, /**< 2 */
    XMA_YUV444_FMT_TYPE, /**< 3 */
    XMA_RGB888_FMT_TYPE, /**< 4 */
    XMA_RGBP_FMT_TYPE,   /**< 5 */
} XmaFormatType;

/**
 * struct XmaFrameProperties - Description of frame dimensions for XmaFrame
*/
typedef struct XmaFrameProperties
{
    XmaFormatType   format; /**< id specifying fourcc */
    int32_t         width; /**< width of primary plane */
    int32_t         height; /**< height of primary plane */
    int32_t         bits_per_pixel; /**< bits per pixel of primary plane */
} XmaFrameProperties;

/**
 * struct XmaFrame - Data structure describing a raw video frame and its buffers
*/
typedef struct XmaFrame
{
    XmaBufferRef       data[XMA_MAX_PLANES]; /**< data buffers */
    XmaFrameProperties frame_props; /**< description of primary plane */
    XmaFraction        time_base; /**< time base as a fraction */
    XmaFraction        frame_rate; /**< frames per second as a fraction */
    uint64_t           pts; /**< presentation timestamp */
    int32_t            is_idr; /**< flag indicating that frame should be treated as an IDR frame */
    int32_t            do_not_encode; /**< flag instruction to not encode frame */
    int32_t            is_last_frame; /**< flag indicating this is the last frame to encode */
} XmaFrame;

/**
 * struct XmaDataBuffer - A structure describing a raw data buffer
*/
typedef struct XmaDataBuffer
{
    XmaBufferRef    data; /**< description of data buffer*/
    int32_t         alloc_size; /**< allocated size of data buffer */
    int32_t         is_eof; /**< flag to indicate that this buffer is EOF */
    int32_t         pts; /**< presentation time stamp looping back to application */
    int32_t         poc; /**< Picture order count for current output frame */
} XmaDataBuffer;

/**
 * struct XmaFrameData - Member structure with array of raw data pointers for multiplane buffer
*/
typedef struct XmaFrameData
{
    uint8_t         *data[XMA_MAX_PLANES]; /**< buffer pointers */
    XmaBufferObj    *dev_buf[XMA_MAX_PLANES]; /**< device buffer pointers */
} XmaFrameData;

/**
 * struct XmaFrameFormatDesc - Member data structure describing video format and frame count
*/
typedef struct XmaFrameFormatDesc
{
    XmaFormatType   format; /**< id identifying fourcc code */
    int32_t         num_planes; /**< number of planes for format */
} XmaFrameFormatDesc;

/**
 * xma_frame_alloc() - Allocate a new frame buffer according to specified frame properties
 *
 * @frame_props: Description of frame buffer to be allocated
 *
 * RETURN: XmaFrame pointer
*/
XmaFrame*
xma_frame_alloc(XmaFrameProperties *frame_props);

/**
 * xma_frame_planes_get() - Return the number of planes in the frame specified
 *
 * @frame_props: Properties of frame being queried
 *
 * RETURN: number of planes in format specified by frame_props (0-3)
*/
int32_t
xma_frame_planes_get(XmaFrameProperties *frame_props);

/**
 * xma_frame_from_buffers_clone() - Wraps buffers described in XmaFrameData into XmaFrame container
 *
 * @frame_props: Properties of XmaFrame to create
 * @frame_data: Container of previously allocated frame buffers
 *
 * RETURN: XmaFrame pointer populated with frame properties and pointers
 * to frame data specified by parameters
*/
XmaFrame*
xma_frame_from_buffers_clone(XmaFrameProperties *frame_props,
                             XmaFrameData       *frame_data);

XmaFrame*
xma_frame_from_dev_buffers_clone(XmaFrameProperties *frame_props,
                             XmaFrameData       *frame_data);

/**
 * xma_frame_free() - Free frame data structure
 *
 * @frame: frame instance to free
 *
 * Note: A buffer with is_clone flag set will not be freed
 * by XMA when the refcount is == 0.  Any XMA container with
 * references to this buffer will be freed (e.g. XmaFrame), however.
*/
void
xma_frame_free(XmaFrame *frame);

/**
 * xma_data_buffer_alloc() - Allocate a single buffer and return as XmaDataBuffer pointer
 *
 * @size: of buffer to allocate from heap
 *
 * RETURN: pointer to XmaDataBuffer with allocated memory
*/
XmaDataBuffer*
xma_data_buffer_alloc(size_t size);

/**
 * xma_data_from_buffer_clone() - Create an XmaDataBuffer object from data of given size
 *
 * @data: pointer to raw data previously allocated
 * @size: size of data
 *
 * RETURN: pointer to XmaDataBuffer container with members initialized
 * to point to data with provided size
*/
XmaDataBuffer*
xma_data_from_buffer_clone(uint8_t *data, size_t size);

XmaDataBuffer*
xma_data_from_device_buffer_clone(XmaBufferObj *dev_buf);

/**
 * xma_data_buffer_free() - Free XmaDataBuffer container structure
 *
 * @data: structure to be freed
 *
 * Note: A buffer with is_clone flag set will not be freed
 * by XMA when the refcount is == 0.
*/
void
xma_data_buffer_free(XmaDataBuffer *data);

#ifdef __cplusplus
}
#endif

#endif
