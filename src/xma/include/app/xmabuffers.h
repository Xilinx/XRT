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
    NO_BUFFER /**< 4 Frame/Data is dummy without any buffer*/
} XmaBufferType;

typedef struct XmaBufferObj
{
   uint8_t* data;
   uint64_t size;
   uint64_t paddr;
   int32_t  bank_index;
   int32_t  dev_index;
//   int32_t  ref_cnt;//For use by FFMPEG/Plugins; Not managed by XMA
   void*    user_ptr;//For use by FFMPEG/Plugins; Not managed by XMA
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
 * enum XmaFrameSideDataType - ID describing type of side data
*/
typedef enum XmaFrameSideDataType
{
    XMA_FRAME_SIDE_DATA_START,
    XMA_FRAME_QP_MAP = XMA_FRAME_SIDE_DATA_START,
    XMA_FRAME_SIDE_DATA_MAX_COUNT
} XmaFrameSideDataType;

/**
 * XmaSideDataHandle - A Handle to the Side Data Buffer.
*/
typedef void* XmaSideDataHandle;

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
    int32_t         linesize[XMA_MAX_PLANES]; /**< linesize per components */
    int32_t         bits_per_pixel; /**< bits per pixel of primary plane */
} XmaFrameProperties;

/**
 * struct XmaFrame - Data structure describing a raw video frame and its buffers
*/
typedef struct XmaFrame
{
    XmaBufferRef       data[XMA_MAX_PLANES]; /**< data buffers */
    XmaSideDataHandle  *side_data;
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
 * @dummy: Allocate dummy frame without any buffer
 *
 * RETURN: XmaFrame pointer
*/
XmaFrame*
xma_frame_alloc(XmaFrameProperties *frame_props, bool dummy);

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
xma_frame_from_device_buffers(XmaFrameProperties *frame_props,
                             XmaFrameData *frame_data, bool clone);

/**
 * xma_frame_free() - Free frame data structure
 * The associated side data handles, if any, are also cleared.
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
 * xma_side_data_alloc() - Allocates side data handle, with
 * reference count equal to 1. The side data buffer 'side_data'
 * can be re-used if 'use_buffer' is set to 1.
 *
 * @side_data: Buffer pointer containing the side data.
 * If it is Null, then 'use_buffer' must be 0.
 * @sd_type: side data to be set.
 * @size: size of the side data buffer.
 * @use_buffer: If it is set to 0, then a new buffer of size
 * 'size' is allocated and the 'side_data' (if not NULL) provided
 * by the user is copied into it.
 * Else if, 'use_buffer' is set to 1, then 'side_data' buffer is
 * used to create the buffer handle.
 *
 * RETURN: XmaSideDataHandle on success, which
 * contains the side data buffer.
 * In case of failure, NULL is returned.
*/
XmaSideDataHandle
xma_side_data_alloc(void                      *side_data,
                    enum XmaFrameSideDataType sd_type,
                    size_t                    size,
                    int32_t                   use_buffer);

/**
 * xma_side_data_free() - Decrements the reference count of the
 * side_data by 1. If the refrence count is 0, then the buffer handle
 * is deallocated. If the actual data buffer was also allocated,
 * while creating the buffer handle then it is also released.
 * In case, the actual side data buffer provided by the user was
 * re-used while creating this buffer handle, then only the buffer
 * handle is released.
 *
 * @side_data: The side data handle which needs to be freed
 *
*/
void
xma_side_data_free(XmaSideDataHandle side_data);

/**
 * xma_side_data_inc_ref() - The side data is reference
 * counted. If any user wants to use this buffer, then,
 * it should call this API to hold a reference to this buffer.
 *
 * @side_data: The side data handle whose refcount needs to be
 * incremented.
 *
 * RETURN: The reference count of the side data buffer after
 * incrementing it by 1. If the 'side_data' is NULL, then
 * 'XMA_ERROR_INVALID' error is returned.
*/
int32_t
xma_side_data_inc_ref(XmaSideDataHandle side_data);

/**
 * xma_side_data_dec_ref() - The side data is reference
 * counted. If any user wants to free this buffer, then,
 * it should call this API to release the reference to this buffer.
 * If the refrence count is 0, then the buffer handle
 * is deallocated. If the actual data buffer was also allocated,
 * while creating the buffer handle then it is also released.
 * In case, the actual side data buffer provided by the user was
 * re-used while creating this buffer handle, then only the buffer
 * handle is released.
 *
 * @side_data: The side data handle whose refcount needs to be
 * decremented.
 *
 * RETURN: The reference count of the side data buffer after
 * decrementing it by 1. If the 'side_data' is NULL, then
 * 'XMA_ERROR_INVALID' error is returned.
*/
int32_t
xma_side_data_dec_ref(XmaSideDataHandle side_data);

/**
 * xma_side_data_get_refcount() - The side data is reference
 * counted. The user can get the current refrence count of the
 * side data using this API.
 *
 * @side_data: The side data handle whose refcount is needed.
 *
 * RETURN: The reference count of the side data buffer after
 * decrementing it by 1. If the 'side_data' is NULL, then
 * 'XMA_ERROR_INVALID' error is returned.
*/
int32_t
xma_side_data_get_refcount(XmaSideDataHandle side_data);

/**
 * xma_side_data_get_buffer() - Use this API to get the pointer
 * to the side data buffer.
 *
 * @side_data: The side data handle which holds the side data buffer.
 *
 * RETURN: Pointer to the side data buffer.
*/
void*
xma_side_data_get_buffer(XmaSideDataHandle side_data);

/**
 * xma_side_data_get_size() - Use this API to get the size of
 * the side data buffer.
 *
 * @side_data: The side data handle which holds the side data buffer.
 *
 * RETURN: size of the side data buffer.
*/
size_t
xma_side_data_get_size(XmaSideDataHandle side_data);

/**
 * xma_frame_add_side_data() - Sets the side data of the frame.
 * In case, there is already same type of side data associated
 * with the frame, it is removed and the new side data
 * is set. The reference count of the side_data buffer is
 * incremented by 1, on successful execution.
 *
 * @frame: Frame, with which, the side data needs to be associated.
 * @side_data: The side data handle to be added to the XmaFrame.
 *
 * RETURN: XMA_ERROR_INVALID, if 'frame' or 'side_data' is NULL.
 * XMA_ERROR, in case no memory is available.
 * XMA_SUCCESS, on successful execution.
*/
int32_t
xma_frame_add_side_data(XmaFrame          *frame,
                        XmaSideDataHandle side_data);

/**
 * xma_frame_get_side_data() - Return the handle to the required
 * type of side data buffer
 *
 * @frame: Frame, to which, the required side data is associated.
 * @sd_type: type of side data buffer required
 *
 * RETURN: XmaSideDataHandle, a handle to the requested type of
 * side data. If no side data of the requested type is present,
 * then NULL is returned.
*/
XmaSideDataHandle
xma_frame_get_side_data(XmaFrame                  *frame,
                        enum XmaFrameSideDataType sd_type);

/**
 * xma_frame_remove_side_data() - Removes the side data handle
 * from the frame. The side data buffer refrence count is
 * decremented by 1. If, it results in zero, then, the side data
 * handle is freed.
 * Also, if the buffer handle owns the actual side data buffer,
 * then, that is also freed.
 *
 * @frame: Frame, with which, the side data is associated.
 * @side_data: The side data handle, to be removed from the XmaFrame.
 *
 * RETURN: XMA_ERROR_INVALID, if 'frame' does not have the specified
 * 'side_data' buffer handle is NULL.
 * XMA_SUCCESS, on successful execution.
*/
int32_t
xma_frame_remove_side_data(XmaFrame          *frame,
                           XmaSideDataHandle side_data);


/**
 * xma_frame_remove_side_data_type() - Removes the specified
 * type of the side data handle reference from the frame.
 * The side data buffer refrence count is decremented by 1.
 * If, it results in zero, then the side data buffer handle is freed.
 * Also, if the buffer handle owns the actual side data buffer,
 * then, that is also freed.
 *
 * @frame: Frame, with which, the side data is associated.
 * @side_data: The type of side data buffer to be removed.
 *
 * RETURN: XMA_ERROR_INVALID, if 'frame' does not have the specified
 * 'side_data' buffer handle.
 * XMA_SUCCESS, on successful execution.
*/
int32_t
xma_frame_remove_side_data_type(XmaFrame                  *frame,
                                enum XmaFrameSideDataType sd_type);

/**
 * xma_frame_clear_all_side_data() - Removes all types of
 * the side data handle references from the frame.
 * The refrence count of each side data buffer associated to the
 * frame is decremented by 1. If, it results in zero, then
 * the side data handle is freed.
 * Also, if the buffer handle owns the actual side data buffer,
 * then, that is also freed.
 *
 * @frame: Frame, with which, the side data is associated.
 * @side_data: The type of side data buffer to be removed.
 *
*/
void
xma_frame_clear_all_side_data(XmaFrame *frame);

/**
 * xma_data_buffer_alloc() - Allocate a single buffer and return as XmaDataBuffer pointer
 *
 * @size: of buffer to allocate from heap
 * @dummy: Allocate dummy XmaDataBuffer without any memory
 *
 * RETURN: pointer to XmaDataBuffer with allocated memory
*/
XmaDataBuffer*
xma_data_buffer_alloc(size_t size, bool dummy);

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
xma_data_from_device_buffer(XmaBufferObj *dev_buf, bool clone);

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

int32_t xma_add_ref_cnt(XmaBufferObj *b_obj, int32_t num);//Returns new value after adding

#ifdef __cplusplus
}
#endif

#endif
