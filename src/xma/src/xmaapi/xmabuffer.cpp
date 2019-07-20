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
#include <stdlib.h>
#include <string.h>

#include "app/xmabuffers.h"
#include "app/xmalogger.h"

#define XMA_BUFFER_MOD "xmabuffer"

int32_t
xma_frame_planes_get(XmaFrameProperties *frame_props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD, "%s()\n", __func__);
    XmaFrameFormatDesc frame_format_desc[] =
    {
        {XMA_NONE_FMT_TYPE,    0},
        {XMA_YUV420_FMT_TYPE,  3},
        {XMA_YUV422_FMT_TYPE,  3},
        {XMA_YUV444_FMT_TYPE,  3},
        {XMA_RGB888_FMT_TYPE,  1},
        {XMA_RGBP_FMT_TYPE,    3},
    };

    return frame_format_desc[frame_props->format].num_planes;
}

XmaFrame*
xma_frame_alloc(XmaFrameProperties *frame_props)
{
    int32_t num_planes;

    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD, "%s()\n", __func__);
    XmaFrame *frame = (XmaFrame*) malloc(sizeof(XmaFrame));
    if (frame  == NULL)
        return NULL;
    memset(frame, 0, sizeof(XmaFrame));
    frame->frame_props = *frame_props;
    num_planes = xma_frame_planes_get(frame_props);

    for (int32_t i = 0; i < num_planes; i++)
    {
        frame->data[i].refcount++;
        frame->data[i].buffer_type = XMA_HOST_BUFFER_TYPE;
        frame->data[i].is_clone = false;
        // TODO: Get plane size for each plane
        frame->data[i].buffer = malloc(frame_props->width *
                                       frame_props->height);
        frame->data[i].xma_device_buf = NULL;
    }

    return frame;
}

XmaFrame*
xma_frame_from_buffers_clone(XmaFrameProperties *frame_props,
                             XmaFrameData       *frame_data)
{
    int32_t num_planes;

    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() frame_props %p and frame_data %p\n",
               __func__, frame_props, frame_data);
    XmaFrame *frame = (XmaFrame*) malloc(sizeof(XmaFrame));
    if (frame  == NULL)
        return NULL;
    memset(frame, 0, sizeof(XmaFrame));
    frame->frame_props = *frame_props;
    num_planes = xma_frame_planes_get(frame_props);

    for (int32_t i = 0; i < num_planes; i++)
    {
        frame->data[i].refcount++;
        frame->data[i].buffer_type = XMA_HOST_BUFFER_TYPE;
        frame->data[i].buffer = frame_data->data[i];
        frame->data[i].is_clone = true;
        frame->data[i].xma_device_buf = NULL;
    }

    return frame;
}

XmaFrame*
xma_frame_from_device_buffers(XmaFrameProperties *frame_props,
                             XmaFrameData *frame_data, bool clone)
{
    int32_t num_planes;

    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() frame_props %p and frame_data %p\n",
               __func__, frame_props, frame_data);
    XmaFrame *frame = (XmaFrame*) malloc(sizeof(XmaFrame));
    if (frame  == NULL)
        return NULL;
    memset(frame, 0, sizeof(XmaFrame));
    frame->frame_props = *frame_props;
    num_planes = xma_frame_planes_get(frame_props);

    for (int32_t i = 0; i < num_planes; i++)
    {
        frame->data[i].refcount++;
        if (frame_data->dev_buf[i] == NULL) {
            xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                    "%s(): dev_buf XmaBufferObj is NULL in frame_data\n", __func__);
            return NULL;
        }
        if (frame_data->dev_buf[i]->device_only_buffer) {
            frame->data[i].buffer_type = XMA_DEVICE_ONLY_BUFFER_TYPE;
            frame->data[i].buffer = NULL;
        } else {
            frame->data[i].buffer_type = XMA_DEVICE_BUFFER_TYPE;
            frame->data[i].buffer = frame_data->dev_buf[i]->data;
        }
        frame->data[i].xma_device_buf = frame_data->dev_buf[i];
        frame->data[i].is_clone = clone;
    }

    return frame;
}

void
xma_frame_free(XmaFrame *frame)
{
    int32_t num_planes;

    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Free frame %p\n", __func__, frame);
    num_planes = xma_frame_planes_get(&frame->frame_props);

    for (int32_t i = 0; i < num_planes; i++)
        frame->data[i].refcount--;

    if (frame->data[0].refcount > 0)
        return;

    for (int32_t i = 0; i < num_planes && !frame->data[i].is_clone; i++)
        free(frame->data[i].buffer);
        //Sarab: TODO Free device buffer also

    free(frame);
}

XmaDataBuffer*
xma_data_from_buffer_clone(uint8_t *data, size_t size)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Cloning buffer from %p of size %lu\n",
               __func__, data, size);
    XmaDataBuffer *buffer = (XmaDataBuffer*) malloc(sizeof(XmaDataBuffer));
    if (buffer  == NULL)
        return NULL;
    memset(buffer, 0, sizeof(XmaDataBuffer));
    buffer->data.refcount++;
    buffer->data.buffer_type = XMA_HOST_BUFFER_TYPE;
    buffer->data.is_clone = true;
    buffer->data.buffer = data;
    buffer->data.xma_device_buf = NULL;
    buffer->alloc_size = size;
    buffer->is_eof = 0;
    buffer->pts = 0;
    buffer->poc = 0;

    return buffer;
}

XmaDataBuffer*
xma_data_from_device_buffer(XmaBufferObj *dev_buf, bool clone)
{
    if (dev_buf == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                "%s(): dev_buf XmaBufferObj is NULL\n", __func__);
        return NULL;
    }
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Cloning buffer from %p of size %lu\n",
               __func__, dev_buf->data, dev_buf->size);
    XmaDataBuffer *buffer = (XmaDataBuffer*) malloc(sizeof(XmaDataBuffer));
    if (buffer  == NULL)
        return NULL;
    memset(buffer, 0, sizeof(XmaDataBuffer));

    buffer->data.refcount++;

    if (dev_buf->device_only_buffer) {
        buffer->data.buffer_type = XMA_DEVICE_ONLY_BUFFER_TYPE;
        buffer->data.buffer = NULL;
    } else {
        buffer->data.buffer_type = XMA_DEVICE_BUFFER_TYPE;
        buffer->data.buffer = dev_buf->data;
    }
    buffer->data.xma_device_buf = dev_buf;
    buffer->data.is_clone = clone;
    buffer->alloc_size = dev_buf->size;
    buffer->is_eof = 0;
    buffer->pts = 0;
    buffer->poc = 0;

    return buffer;
}


XmaDataBuffer*
xma_data_buffer_alloc(size_t size)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Allocate buffer from of size %lu\n", __func__, size);
    XmaDataBuffer *buffer = (XmaDataBuffer*) malloc(sizeof(XmaDataBuffer));
    if (buffer  == NULL)
        return NULL;
    memset(buffer, 0, sizeof(XmaDataBuffer));
    buffer->data.refcount++;
    buffer->data.buffer_type = XMA_HOST_BUFFER_TYPE;
    buffer->data.is_clone = false;
    buffer->data.buffer = malloc(size);
    buffer->data.xma_device_buf = NULL;
    buffer->alloc_size = size;
    buffer->is_eof = 0;
    buffer->pts = 0;
    buffer->poc = 0;

    return buffer;
}

void
xma_data_buffer_free(XmaDataBuffer *data)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Free buffer %p\n", __func__, data);
    data->data.refcount--;
    if (data->data.refcount > 0)
        return;

    if (!data->data.is_clone)
        free(data->data.buffer);
        //Sarab: TODO free device buffer also

    free(data);
}

