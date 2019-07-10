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
#include <limits.h>

#include "app/xmabuffers.h"
#include "app/xmalogger.h"
#include "app/xmaerror.h"
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
    }
    frame->side_data = NULL;
    frame->nb_side_data = 0;
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
    }

    return frame;
}

XmaFrameSideData*
xma_frame_get_side_data(XmaFrame                  *frame,
                        enum XmaFrameSideDataType type)
{
    uint32_t i;

    for (i = 0; i < frame->nb_side_data; i++) {
        if (frame->side_data[i]->type == type)
            return frame->side_data[i];
    }
    return NULL;
}

int32_t
xma_frame_set_side_data(XmaFrame          *frame,
                        XmaFrameSideData  *sd)
{
    XmaFrameSideData **tmp;
    XmaFrameSideData *side_data;

    if (!frame || !sd) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                   "%s() frame %p side_data %p\n", __func__,
                   frame, sd);
        return XMA_ERROR_INVALID;
    }

    if (frame->nb_side_data > (INT_MAX/sizeof(XmaFrameSideData*)) - 1) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                   "%s() OOM\n", __func__);
        return XMA_ERROR_INVALID;
    }
    side_data = xma_frame_get_side_data(frame, sd->type);
    if (side_data) {
        return XMA_ERROR_INVALID;
    } else {
        tmp = (XmaFrameSideData**)realloc(frame->side_data,
                      (frame->nb_side_data + 1) * sizeof(XmaFrameSideData*));
        if (!tmp) {
            xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                       "%s() OOM\n", __func__);
            return XMA_ERROR;
        }
        frame->side_data = tmp;

        frame->side_data[frame->nb_side_data++] = sd;
        sd->sdata_ref.refcount++;
    }
    return XMA_SUCCESS;
}

XmaFrameSideData*
xma_frame_add_side_data(XmaFrame                  *frame,
                        enum XmaFrameSideDataType type,
                        void                      *side_data,
                        int32_t                   size,
                        int32_t                   use_buffer)
{
    XmaBufferRef *xmaBuf;
    XmaFrameSideData *sd;
    if (frame  == NULL)
        return NULL;
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() frame %p side_data %p type %d size %d use_buffer=%d\n",
               __func__, frame, side_data, type, size, use_buffer);
    sd = (XmaFrameSideData*)calloc(1, sizeof(XmaFrameSideData));
    if (!sd) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
            "%s() OOM!!\n", __func__);
        return NULL;
    }
    xmaBuf = &sd->sdata_ref;
    if (!use_buffer) {
        sd->sdata = calloc(1, size);
        if (!sd->sdata) {
            xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                "%s() OOM!!\n", __func__);
            free(sd);
            return NULL;
        }
        if (side_data) {
            memcpy(sd->sdata, side_data, size);
        }
        xmaBuf->is_clone = false;
    } else {
        sd->sdata = side_data;
        xmaBuf->is_clone = true;
    }

    sd->type = type;
    sd->size = size;

    xmaBuf->refcount = 0;
    xmaBuf->buffer_type = XMA_HOST_BUFFER_TYPE;
    xmaBuf->buffer = sd->sdata;
    //Remove if old side data of the same type is present
    xma_frame_remove_side_data(frame, sd->type);

    if (xma_frame_set_side_data(frame, sd)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
		    "%s() Error set side data %p failed!!\n", __func__, sd);
        free(sd->sdata);
        free(sd);
        return NULL;
    }

    return sd;
}

static void
free_side_data(XmaFrameSideData *sd)
{
    XmaBufferRef *xmaBuf = &sd->sdata_ref;
    xmaBuf->refcount--;
    if (xmaBuf->refcount != 0) return;
    if (!xmaBuf->is_clone) {
        free(xmaBuf->buffer);
    }
    free(sd);
}

static void
clear_side_data(XmaFrame *frame)
{
    uint32_t i;

    for (i = 0; i < frame->nb_side_data; i++) {
        free_side_data(frame->side_data[i]);
    }
    frame->nb_side_data = 0;

    free(frame->side_data);
}

void
xma_frame_remove_side_data(XmaFrame                  *frame,
                           enum XmaFrameSideDataType type)
{
    uint32_t i;

    for (i = 0; i < frame->nb_side_data; i++) {
        if (frame->side_data[i]->type == type) {
            free_side_data(frame->side_data[i]);
            frame->side_data[i] = NULL;
            frame->side_data[i] = frame->side_data[frame->nb_side_data - 1];
            frame->nb_side_data--;
        }
    }
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

    clear_side_data(frame);
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
    buffer->alloc_size = size;
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

    free(data);
}

