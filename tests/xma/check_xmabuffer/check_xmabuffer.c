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

#include <memory.h>
//#include <strings.h>
#include <string>
#include <iostream>
#include "xma.h"
#include "lib/xmaapi.h"
#include "lib/xmares.h"
#include "lib/xmahw.h"
#include "lib/xmahw_private.h"
#include "lib/xmacfg.h"

int ck_assert_int_eq(int rc1, int rc2) {
  if (rc1 != rc2) {
    return -1;
  } else {
    return 0;
  }
}

int ck_assert_str_eq(const char* str1, const char* str2) {
  if (std::string(str1) != std::string(str2)) {
    return -1;
  } else {
    return 0;
  }
}

int ck_assert(bool result) {
  if (!result) {
    return -1;
  } else {
    return 0;
  }
}


static void xmabuffer_unchecked_setup(void)
{
    extern XmaSingleton *g_xma_singleton;
    g_xma_singleton = (XmaSingleton*) malloc(sizeof(*g_xma_singleton));
    memset(g_xma_singleton, 0, sizeof(*g_xma_singleton));

    g_xma_singleton->systemcfg.logger_initialized = false;
    xma_logger_init(&g_xma_singleton->logger);
    return;
}

/* default 1080p 420p allocation */
int xma_frame_alloc_1080p_420()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL;
    int plane_cnt, i;
    int rc;

    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.format = XMA_YUV420_FMT_TYPE;
    frame_props.width = 1920;
    frame_props.height = 1080;
    frame_props.bits_per_pixel = 8;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    plane_cnt = xma_frame_planes_get(&frame_props);
    rc |= ck_assert_int_eq(plane_cnt, 3);

    for(i = 0; i < plane_cnt; i++)
        rc |= ck_assert(frame->data[i].buffer != NULL);

    return rc;
}

/* send an uninitalized data structure */
int neg_xma_frame_alloc_uninit_props()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL;
    int rc;

    memset(&frame_props, 0, sizeof(XmaFrameProperties));

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    rc |= ck_assert(frame->data[0].buffer == NULL);

    return rc;
}

int neg_xma_frame_alloc_1080p_0bpp()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL;
    int rc;

    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.width = 1920;
    frame_props.height = 1080;
    frame_props.bits_per_pixel = 0;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    return rc;
}

int xma_frame_alloc_1080p_none_fmt()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL;
    int plane_cnt;
    int rc;

    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.format = (XmaFormatType) 0;
    frame_props.width = 1920;
    frame_props.height = 1080;
    frame_props.bits_per_pixel = 8;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    plane_cnt = xma_frame_planes_get(&frame_props);
    rc |= ck_assert_int_eq(plane_cnt, 0);
    /* JPM why is buffer 0 == NULL ? */
    rc |= ck_assert(frame->data[0].buffer == NULL);
    rc |= ck_assert(frame->data[1].buffer == NULL);

    return rc;
}

int xma_frame_alloc_720p_422()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL;
    int plane_cnt, i;
    int rc;

    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.format = XMA_YUV422_FMT_TYPE;
    frame_props.width = 1280;
    frame_props.height = 720;
    frame_props.bits_per_pixel = 8;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    plane_cnt = xma_frame_planes_get(&frame_props);
    rc |= ck_assert_int_eq(plane_cnt, 3);

    for(i = 0; i < plane_cnt; i++)
        rc |= ck_assert(frame->data[i].buffer != NULL);

    return rc;
}

int xma_frame_alloc_144p_444()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL;
    int plane_cnt, i;
    int rc;

    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.format = XMA_YUV444_FMT_TYPE;
    frame_props.width = 240;
    frame_props.height = 144;
    frame_props.bits_per_pixel = 8;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    plane_cnt = xma_frame_planes_get(&frame_props);
    rc |= ck_assert_int_eq(plane_cnt, 3);

    for(i = 0; i < plane_cnt; i++)
        rc |= ck_assert(frame->data[i].buffer != NULL);

    return rc;
}

int xma_frame_alloc_360p_rgb()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame;
    int plane_cnt, i;
    int rc;

    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.format = XMA_RGB888_FMT_TYPE;
    frame_props.width = 640;
    frame_props.height = 360;
    frame_props.bits_per_pixel = 8;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    plane_cnt = xma_frame_planes_get(&frame_props);
    rc |= ck_assert_int_eq(plane_cnt, 1);

    for(i = 0; i < plane_cnt; i++)
        rc |= ck_assert(frame->data[i].buffer != NULL);

    return rc;
}

int xma_data_buffer_alloc_1080p()
{
    XmaDataBuffer *d_buff = NULL;
    size_t buff_size = 1920 * 1080 * 8;

    d_buff = xma_data_buffer_alloc(buff_size);

    ck_assert(d_buff != NULL);
    ck_assert(d_buff->data.buffer != NULL);
    ck_assert_int_eq(d_buff->data.refcount, 1);
    ck_assert_int_eq(d_buff->data.buffer_type, XMA_HOST_BUFFER_TYPE);
    ck_assert_int_eq(d_buff->alloc_size, buff_size);
    ck_assert_int_eq(d_buff->is_eof, 0);
}

int xma_data_buffer_alloc_0()
{
    XmaDataBuffer *d_buff = NULL;
    size_t buff_size = 0;

    d_buff = xma_data_buffer_alloc(buff_size);

    ck_assert(d_buff != NULL);
    /* JPM shouldn't data.buffer == NULL ??? */
    ck_assert(d_buff->data.buffer != NULL);
    ck_assert_int_eq(d_buff->data.refcount, 1);
    ck_assert_int_eq(d_buff->data.buffer_type, XMA_HOST_BUFFER_TYPE);
    ck_assert_int_eq(d_buff->alloc_size, buff_size);
    ck_assert_int_eq(d_buff->is_eof, 0);
}

int xma_data_buffer_free_tst()
{
    XmaDataBuffer *d_buff = NULL;
    size_t buff_size = 1920 * 1080 * 8;
    void *data;
    int rc;

    d_buff = xma_data_buffer_alloc(buff_size);

    rc = ck_assert(d_buff != NULL);
    rc |= ck_assert(d_buff->data.buffer != NULL);
    data = d_buff->data.buffer;
    rc |= ck_assert_int_eq(d_buff->data.refcount, 1);
    rc |= ck_assert_int_eq(d_buff->data.buffer_type, XMA_HOST_BUFFER_TYPE);
    rc |= ck_assert_int_eq(d_buff->alloc_size, buff_size);
    rc |= ck_assert_int_eq(d_buff->is_eof, 0);

    d_buff->data.refcount++;

    xma_data_buffer_free(d_buff);
    rc |= ck_assert_int_eq(d_buff->data.refcount, 1);
    rc |= ck_assert(d_buff->data.buffer == data);
    /*
    xma_data_buffer_free(d_buff);
    ck_assert_int_eq(d_buff->data.refcount, 0);
    ck_assert(d_buff->data.buffer == data);
    */

    return rc;
}

int xma_data_buffer_clone_tst()
{
    XmaDataBuffer *d_buff = NULL, *d_buff_clone = NULL;
    size_t buff_size = 1920 * 1080 * 8;
    int rc;

    d_buff = xma_data_buffer_alloc(buff_size);

    rc = ck_assert(d_buff != NULL);
    rc |= ck_assert(d_buff->data.buffer != NULL);
    rc |= ck_assert_int_eq(d_buff->data.refcount, 1);
    rc |= ck_assert_int_eq(d_buff->data.buffer_type, XMA_HOST_BUFFER_TYPE);
    rc |= ck_assert_int_eq(d_buff->alloc_size, buff_size);
    rc |= ck_assert_int_eq(d_buff->is_eof, 0);

    d_buff_clone = xma_data_from_buffer_clone((uint8_t*)d_buff->data.buffer, buff_size);

    rc |= ck_assert(d_buff_clone->data.buffer == d_buff->data.buffer);
    rc |= ck_assert_int_eq(d_buff_clone->data.refcount, 1);
    rc |= ck_assert_int_eq(d_buff->data.refcount, 1);
    rc |= ck_assert_int_eq(d_buff_clone->data.buffer_type, XMA_HOST_BUFFER_TYPE);
    rc |= ck_assert_int_eq(d_buff_clone->alloc_size, buff_size);
    rc |= ck_assert_int_eq(d_buff_clone->is_eof, 0);

    return rc;
}

int xma_frame_clone_tst()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL, *frame_clone = NULL;
    XmaFrameData frame_data;
    int plane_cnt, i;
    int rc;

    /* allocate first frame w/buffers */
    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.format = XMA_YUV420_FMT_TYPE;
    frame_props.width = 1920;
    frame_props.height = 1080;
    frame_props.bits_per_pixel = 8;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    plane_cnt = xma_frame_planes_get(&frame_props);
    rc |= ck_assert_int_eq(plane_cnt, 3);

    for(i = 0; i < plane_cnt; i++)
    {
        rc |= ck_assert(frame->data[i].buffer != NULL);
        frame_data.data[i] = (uint8_t*)frame->data[i].buffer;
    }

    frame_clone = xma_frame_from_buffers_clone(&frame_props, &frame_data);
    rc |= ck_assert(frame_clone != NULL);
    rc |= ck_assert_int_eq(frame_clone->data[0].refcount, 1);
    rc |= ck_assert_int_eq(frame_clone->data[1].refcount, 1);
    rc |= ck_assert_int_eq(frame_clone->data[2].refcount, 1);
    rc |= ck_assert(frame_clone->data[0].buffer == frame->data[0].buffer);

    return rc;
}

int xma_frame_free_tst()
{
    XmaFrameProperties frame_props;
    XmaFrame *frame = NULL;
    int plane_cnt, i;
    int rc;

    /* allocate first frame w/buffers */
    memset(&frame_props, 0, sizeof(XmaFrameProperties));
    frame_props.format = XMA_YUV420_FMT_TYPE;
    frame_props.width = 1920;
    frame_props.height = 1080;
    frame_props.bits_per_pixel = 8;

    frame = xma_frame_alloc(&frame_props);
    rc = ck_assert(frame != NULL);

    plane_cnt = xma_frame_planes_get(&frame_props);
    rc |= ck_assert_int_eq(plane_cnt, 3);

    for(i = 0; i < plane_cnt; i++)
    {
        rc |= ck_assert(frame->data[i].buffer != NULL);
        rc |= ck_assert_int_eq(frame->data[i].refcount, 1);
        frame->data[i].refcount++;
    }

    xma_frame_free(frame);
    rc |= ck_assert(frame != NULL);
    rc |= ck_assert_int_eq(frame->data[0].refcount, 1);
    rc |= ck_assert_int_eq(frame->data[1].refcount, 1);
    rc |= ck_assert_int_eq(frame->data[2].refcount, 1);

    return rc;
}

static inline int32_t check_xmaapi_probe(XmaHwCfg *hwcfg) {
    return 0;
}

static inline bool check_xmaapi_is_compatible(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg) {
    return true;
}

/* TODO: JPM include basic yaml config with single kernel xclbin
 * so that hw configure could be executed with respect to populating
 * the XmaHwCfg data structure
*/
static inline bool check_xmaapi_hw_configure(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg, bool hw_cfg_status) {
    return true;
}


int main()
{
    int number_failed = 0;
    int32_t rc;
    extern XmaHwInterface hw_if;

    hw_if.is_compatible = check_xmaapi_is_compatible;
    hw_if.configure = check_xmaapi_hw_configure;
    hw_if.probe = check_xmaapi_probe;

    xmabuffer_unchecked_setup();

    rc = xma_frame_alloc_1080p_420();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_frame_alloc_1080p_none_fmt();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_frame_alloc_720p_422();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_frame_alloc_360p_rgb();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_frame_alloc_144p_444();
    if (rc != 0) {
      number_failed++;
    }

    rc = neg_xma_frame_alloc_uninit_props();
    if (rc != 0) {
      number_failed++;
    }

    rc = neg_xma_frame_alloc_1080p_0bpp();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_data_buffer_alloc_1080p();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_data_buffer_alloc_0();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_data_buffer_free_tst();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_data_buffer_clone_tst();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_frame_clone_tst();
    if (rc != 0) {
      number_failed++;
    }

    rc = xma_frame_free_tst();
    if (rc != 0) {
      number_failed++;
    }


   if (number_failed == 0) {
     printf("XMA check_xmabuffer test completed successfully\n");
     return EXIT_SUCCESS;
    } else {
     printf("ERROR: XMA check_xmabuffer test failed\n");
     return EXIT_FAILURE;
    }
 //return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
