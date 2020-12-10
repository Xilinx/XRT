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
#include "app/xmaerror.h"
#include "lib/xmahw_lib.h"
//#include <cstdio>
#include <iostream>
#include <cstring>

#define XMA_BUFFER_MOD "xmabuffer"

typedef struct XmaFrameSideData
{
    XmaBufferRef              sdata_ref;
    size_t                    size;
    enum XmaFrameSideDataType type;
} XmaFrameSideData;

int32_t
xma_frame_planes_get(XmaFrameProperties *frame_props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD, "%s()\n", __func__);
    if (!frame_props)
        return XMA_ERROR;
    XmaFrameFormatDesc frame_format_desc[] =
    {
        {XMA_NONE_FMT_TYPE,    0},
        {XMA_YUV420_FMT_TYPE,  3},
        {XMA_YUV422_FMT_TYPE,  3},
        {XMA_YUV444_FMT_TYPE,  3},
        {XMA_RGB888_FMT_TYPE,  1},
        {XMA_RGBP_FMT_TYPE,    3},
        {XMA_VCU_NV12_FMT_TYPE,        2},
        {XMA_VCU_NV16_FMT_TYPE,        2},
        {XMA_VCU_NV12_10LE32_FMT_TYPE, 2},
        {XMA_VCU_NV16_10LE32_FMT_TYPE, 2},
    };

    return frame_format_desc[frame_props->format].num_planes;
}

XmaFrame*
xma_frame_alloc(XmaFrameProperties *frame_props, bool dummy)
{
    int32_t num_planes;

    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD, "%s()\n", __func__);
    if (!frame_props)
        return nullptr;
    if (!dummy && (frame_props->width > MAX_FRAME_W_H || frame_props->height > MAX_FRAME_W_H)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD, "Frame size is above limits: w=%d, h=%d", frame_props->width, frame_props->height);
        return nullptr;
    }
    if (!dummy && (frame_props->width <= 0 || frame_props->height <= 0)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD, "Frame size is invalid: w=%d, h=%d", frame_props->width, frame_props->height);
        return nullptr;
    }

    XmaFrame *frame = (XmaFrame*) malloc(sizeof(XmaFrame));
    if (!frame)
        return nullptr;
    memset(frame, 0, sizeof(XmaFrame));
    frame->frame_props = *frame_props;
    num_planes = xma_frame_planes_get(frame_props);

    for (int32_t i = 0; i < num_planes; i++)
    {
        frame->data[i].refcount++;
        frame->data[i].is_clone = false;
        // TODO: Get plane size for each plane
        if (dummy) {
            frame->data[i].buffer_type = NO_BUFFER;
            frame->data[i].buffer = nullptr;
        } else {
            frame->data[i].buffer_type = XMA_HOST_BUFFER_TYPE;
            frame->data[i].buffer = malloc(frame_props->width *
                                       frame_props->height);
        }
        frame->data[i].xma_device_buf = nullptr;
    }
    frame->side_data = nullptr;
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
    if (!frame_props)
        return nullptr;
    if (!frame_data)
        return nullptr;
    XmaFrame *frame = (XmaFrame*) malloc(sizeof(XmaFrame));
    if (!frame)
        return nullptr;
    memset(frame, 0, sizeof(XmaFrame));
    frame->frame_props = *frame_props;
    num_planes = xma_frame_planes_get(frame_props);

    for (int32_t i = 0; i < num_planes; i++)
    {
        frame->data[i].refcount++;
        frame->data[i].buffer_type = XMA_HOST_BUFFER_TYPE;
        frame->data[i].buffer = frame_data->data[i];
        frame->data[i].is_clone = true;
        frame->data[i].xma_device_buf = nullptr;
    }

    return frame;
}


int32_t xma_check_device_buffer(XmaBufferObj *b_obj) {
    if (!b_obj) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD, "xma_check_device_buffer failed. XMABufferObj failed allocation\n");
        return XMA_ERROR;
    }

    XmaBufferObjPrivate* b_obj_priv = (XmaBufferObjPrivate*) b_obj->private_do_not_touch;
    if (!b_obj_priv) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD, "xma_check_device_buffer failed. XMABufferObj failed allocation\n");
        return XMA_ERROR;
    }
    if (b_obj_priv->dev_index < 0 || b_obj_priv->bank_index < 0 || b_obj_priv->size <= 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD, "xma_check_device_buffer failed. XMABufferObj failed allocation\n");
        return XMA_ERROR;
    }
    if (b_obj_priv->dummy != (void*)(((uint64_t)b_obj_priv) | signature)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD, "xma_check_device_buffer failed. XMABufferObj is corrupted.\n");
        return XMA_ERROR;
    }
    if (!b_obj_priv->dev_handle) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD, "xma_check_device_buffer failed. XMABufferObj is corrupted.\n");
        return XMA_ERROR;
    }
    return XMA_SUCCESS;
}

int32_t xma_add_ref_cnt(XmaBufferObj *b_obj, int32_t num) {
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s(), line# %d\n", __func__, __LINE__);

    if (xma_check_device_buffer(b_obj) != XMA_SUCCESS) {
        return -999;
    }
    XmaBufferObjPrivate* b_obj_priv = (XmaBufferObjPrivate*) b_obj->private_do_not_touch;
    b_obj_priv->ref_cnt += num;
    return b_obj_priv->ref_cnt;
}

XmaFrame*
xma_frame_from_device_buffers(XmaFrameProperties *frame_props,
                             XmaFrameData *frame_data, bool clone)
{
    int32_t num_planes;

    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() frame_props %p and frame_data %p\n",
               __func__, frame_props, frame_data);
    if (!frame_props)
        return nullptr;
    if (!frame_data)
        return nullptr;
    XmaFrame *frame = (XmaFrame*) malloc(sizeof(XmaFrame));
    if (!frame)
        return nullptr;
    memset(frame, 0, sizeof(XmaFrame));
    frame->frame_props = *frame_props;
    num_planes = xma_frame_planes_get(frame_props);

    for (int32_t i = 0; i < num_planes; i++)
    {
        frame->data[i].refcount++;
        if (!frame_data->dev_buf[i]) {
            xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                    "%s(): dev_buf XmaBufferObj is NULL in frame_data\n", __func__);
            free(frame);
            return nullptr;
        }
        if (xma_check_device_buffer(frame_data->dev_buf[i]) != XMA_SUCCESS) {
            free(frame);
            return nullptr;
        }
        if (frame_data->dev_buf[i]->device_only_buffer) {
            frame->data[i].buffer_type = XMA_DEVICE_ONLY_BUFFER_TYPE;
            frame->data[i].buffer = nullptr;
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
xma_device_buffer_free(XmaBufferObj *b_obj)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD, "%s()\n", __func__);
    if (xma_check_device_buffer(b_obj) != XMA_SUCCESS) {
        return;
    }
    XmaBufferObjPrivate* b_obj_priv = (XmaBufferObjPrivate*) b_obj->private_do_not_touch;

    xclFreeBO(b_obj_priv->dev_handle, b_obj_priv->boHandle);
    b_obj_priv->dummy = nullptr;
    b_obj_priv->size = -1;
    b_obj_priv->bank_index = -1;
    b_obj_priv->dev_index = -1;
    free(b_obj_priv);
    b_obj->data = nullptr;
    b_obj->size = -1;
    b_obj->bank_index = -1;
    b_obj->dev_index = -1;
    b_obj->device_only_buffer = false;
    b_obj->private_do_not_touch = nullptr;
    delete b_obj;
    b_obj = nullptr;
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

    for (int32_t i = 0; i < num_planes; i++) {
        if (!frame->data[i].is_clone) {
            switch (frame->data[i].buffer_type) {
                case XMA_DEVICE_ONLY_BUFFER_TYPE:
                case XMA_DEVICE_BUFFER_TYPE:
                    xma_device_buffer_free(frame->data[i].xma_device_buf);
                    break;
                case XMA_HOST_BUFFER_TYPE:
                    free(frame->data[i].buffer);
                    break;
                default:
                    break;
            }
        }
        frame->data[i].buffer = nullptr;
        frame->data[i].xma_device_buf = nullptr;
    }

    xma_frame_clear_all_side_data(frame);
    free(frame);
    frame = nullptr;
}

XmaSideDataHandle
xma_side_data_alloc(void                      *side_data,
                    enum XmaFrameSideDataType sd_type,
                    size_t                    size,
                    int32_t                   use_buffer)
{
    XmaBufferRef *xmaBuf;
    XmaFrameSideData *sd;
    void *sdata = nullptr;
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() frame side_data %p type %d size %zu use_buffer=%d\n",
               __func__, side_data, sd_type, size, use_buffer);
    sd = (XmaFrameSideData*)calloc(1, sizeof(XmaFrameSideData));
    if (!sd) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                   "%s() OOM!!\n", __func__);
        return nullptr;
    }
    xmaBuf = &sd->sdata_ref;
    if (!use_buffer) {
        sdata = calloc(1, size);
        if (!sdata) {
            xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                "%s() OOM!!\n", __func__);
            free(sd);
            return nullptr;
        }
        if (side_data) {
            memcpy(sdata, side_data, size);
        }
        xmaBuf->is_clone = false;
    } else {
        sdata = side_data;
        xmaBuf->is_clone = true;
    }

    sd->type = sd_type;
    sd->size = size;

    xmaBuf->refcount = 1;
    xmaBuf->buffer_type = XMA_HOST_BUFFER_TYPE;
    xmaBuf->buffer = sdata;

    return (XmaSideDataHandle)sd;
}

void
xma_side_data_free(XmaSideDataHandle side_data)
{
    xma_side_data_dec_ref(side_data);
}

int32_t
xma_side_data_inc_ref(XmaSideDataHandle side_data)
{
    XmaFrameSideData *sd = (XmaFrameSideData*)side_data;
    if (!sd) return XMA_ERROR_INVALID;
    sd->sdata_ref.refcount++;
    return sd->sdata_ref.refcount;
}

int32_t
xma_side_data_dec_ref(XmaSideDataHandle side_data)
{
    XmaBufferRef *xmaBuf = nullptr;
    XmaFrameSideData *sd = (XmaFrameSideData*)side_data;

    if (!sd) return XMA_ERROR_INVALID;
    xmaBuf = &sd->sdata_ref;
    xmaBuf->refcount--;
    if (xmaBuf->refcount != 0) return xmaBuf->refcount;
    if (!xmaBuf->is_clone) {
        free(xmaBuf->buffer);
    }
    free(sd);

    return 0;
}

int32_t
xma_side_data_get_refcount(XmaSideDataHandle side_data)
{
    XmaBufferRef *xmaBuf = nullptr;
    XmaFrameSideData *sd = (XmaFrameSideData*)side_data;

    if (!sd) return XMA_ERROR_INVALID;
    xmaBuf = &sd->sdata_ref;
    return xmaBuf->refcount;
}

void*
xma_side_data_get_buffer(XmaSideDataHandle side_data)
{
    XmaFrameSideData *sd = (XmaFrameSideData*)side_data;
    if (!sd) return nullptr;
    return sd->sdata_ref.buffer;
}

size_t
xma_side_data_get_size(XmaSideDataHandle side_data)
{
    XmaFrameSideData *sd = (XmaFrameSideData*)side_data;
    if (!sd) return 0;
    return sd->size;
}

int32_t
xma_frame_add_side_data(XmaFrame          *frame,
                        XmaSideDataHandle side_data)
{
    XmaFrameSideData *sd = (XmaFrameSideData*)side_data;
    if (!frame || !sd) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                   "%s() frame %p side_data %p\n",
                   __func__, frame, sd);
        return XMA_ERROR_INVALID;
    }
    //Remove, if old side data of the same type is present
    if (xma_frame_get_side_data(frame, sd->type)) {
        xma_frame_remove_side_data_type(frame, sd->type);
    }
    if (!frame->side_data) {
        frame->side_data = (XmaSideDataHandle*)
                            calloc(XMA_FRAME_SIDE_DATA_MAX_COUNT,
                                    sizeof(XmaFrameSideData*));
        if (!frame->side_data) {
            xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                        "%s() OOM\n", __func__);
            return XMA_ERROR;
        }
    }
    frame->side_data[sd->type] = sd;
    xma_side_data_inc_ref(side_data);

    return XMA_SUCCESS;
}

XmaSideDataHandle
xma_frame_get_side_data(XmaFrame                  *frame,
                        enum XmaFrameSideDataType type)
{
    if (frame->side_data) {
        return (XmaSideDataHandle)frame->side_data[type];
    }
    return nullptr;
}

int32_t
xma_frame_remove_side_data(XmaFrame          *frame,
                           XmaSideDataHandle side_data)
{
    XmaSideDataHandle sd;
    XmaFrameSideData *in_sd = (XmaFrameSideData*)side_data;
	if (!in_sd) return XMA_ERROR_INVALID;

    sd = xma_frame_get_side_data(frame, in_sd->type);
    if (sd != side_data) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                   "%s() Frame %p has no side data buffer %p\n",
                   __func__, frame, side_data);
        return XMA_ERROR_INVALID;
    }

    if (xma_side_data_dec_ref(side_data) != 0) {
        frame->side_data[in_sd->type] = nullptr;
    }

    return XMA_SUCCESS;
}

int32_t
xma_frame_remove_side_data_type(XmaFrame                  *frame,
                                enum XmaFrameSideDataType sd_type)
{
    XmaSideDataHandle sd = xma_frame_get_side_data(frame, sd_type);
    if (sd) {
        xma_side_data_dec_ref(sd);
        frame->side_data[sd_type] = nullptr;
    } else {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                   "%s() Frame %p has no side data of type %d\n",
                   __func__, frame, sd_type);
        return XMA_ERROR_INVALID;
    }

    return XMA_SUCCESS;
}

void
xma_frame_clear_all_side_data(XmaFrame *frame)
{
    uint32_t i;

    if (frame->side_data) {
        for (i = 0; i < XMA_FRAME_SIDE_DATA_MAX_COUNT; i++) {
            if (frame->side_data[i]) {
                xma_side_data_dec_ref(frame->side_data[i]);
            }
        }
        free(frame->side_data);
        frame->side_data = nullptr;
    }
}

XmaDataBuffer*
xma_data_from_buffer_clone(uint8_t *data, size_t size)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Cloning buffer from %p of size %lu\n",
               __func__, data, size);
    if (!data)
        return nullptr;
    XmaDataBuffer *buffer = (XmaDataBuffer*) malloc(sizeof(XmaDataBuffer));
    if (!buffer)
        return nullptr;
    memset(buffer, 0, sizeof(XmaDataBuffer));
    buffer->data.refcount++;
    buffer->data.buffer_type = XMA_HOST_BUFFER_TYPE;
    buffer->data.is_clone = true;
    buffer->data.buffer = data;
    buffer->data.xma_device_buf = nullptr;
    buffer->alloc_size = size;
    buffer->is_eof = 0;
    buffer->pts = 0;
    buffer->poc = 0;

    return buffer;
}

XmaDataBuffer*
xma_data_from_device_buffer(XmaBufferObj *dev_buf, bool clone)
{
    if (!dev_buf) {
        xma_logmsg(XMA_ERROR_LOG, XMA_BUFFER_MOD,
                "%s(): dev_buf XmaBufferObj is NULL\n", __func__);
        return nullptr;
    }
    if (xma_check_device_buffer(dev_buf) != XMA_SUCCESS) {
        return nullptr;
    }
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Cloning buffer from %p of size %lu\n",
               __func__, dev_buf->data, dev_buf->size);
    XmaDataBuffer *buffer = (XmaDataBuffer*) malloc(sizeof(XmaDataBuffer));
    if (!buffer)
        return nullptr;
    memset(buffer, 0, sizeof(XmaDataBuffer));

    buffer->data.refcount++;

    if (dev_buf->device_only_buffer) {
        buffer->data.buffer_type = XMA_DEVICE_ONLY_BUFFER_TYPE;
        buffer->data.buffer = nullptr;
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
xma_data_buffer_alloc(size_t size, bool dummy)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_BUFFER_MOD,
               "%s() Allocate buffer from of size %lu\n", __func__, size);
    XmaDataBuffer *buffer = (XmaDataBuffer*) malloc(sizeof(XmaDataBuffer));
    if (!buffer)
        return nullptr;
    memset(buffer, 0, sizeof(XmaDataBuffer));
    buffer->data.refcount++;
    buffer->data.is_clone = false;
    if (dummy) {
        buffer->data.buffer_type = NO_BUFFER;
        buffer->data.buffer = nullptr;
        buffer->alloc_size = -1;
    } else {
        buffer->data.buffer_type = XMA_HOST_BUFFER_TYPE;
        buffer->data.buffer = malloc(size);
        buffer->alloc_size = size;
    }
    buffer->data.xma_device_buf = nullptr;
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
    if (!data)
        return;
        
    data->data.refcount--;
    if (data->data.refcount > 0)
        return;

    if (!data->data.is_clone) {
        switch (data->data.buffer_type) {
            case XMA_DEVICE_ONLY_BUFFER_TYPE:
            case XMA_DEVICE_BUFFER_TYPE:
                xma_device_buffer_free(data->data.xma_device_buf);
                break;
            case XMA_HOST_BUFFER_TYPE:
                free(data->data.buffer);
                break;
            default:
                break;
        }
        data->data.buffer = nullptr;
        data->data.xma_device_buf = nullptr;
        data->alloc_size = -1;
    }

    free(data);
    data = nullptr;
}

