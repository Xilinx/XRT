/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2020-2021 Xilinx, Inc. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <xma.h>
#include <xmaplugin.h>


/* This is dummy encoder plugin */

static int32_t dummy_init(XmaEncoderSession *enc_session)
{
    return 0;
}

static int32_t dummy_send_frame(XmaEncoderSession *enc_session, XmaFrame *frame)
{
    return 0;
}

static int32_t dummy_recv_data(XmaEncoderSession *enc_session, XmaDataBuffer *data, int32_t *data_size)
{
    return 0;
}

static int32_t dummy_close(XmaEncoderSession *enc_session)
{

    return 0;
}

static int32_t dummy_xma_version(int32_t *main_version, int32_t *sub_version)
{
    *main_version = 2020;
    *sub_version = 1;

    return 0;
}


XmaEncoderPlugin encoder_plugin = {
    .hwencoder_type    = XMA_COPY_ENCODER_TYPE,
    .hwvendor_string   = "Xilinx",
    .format            = XMA_YUV420_FMT_TYPE,
    .bits_per_pixel    = 8,
    .plugin_data_size  = 1,//dummy_size
    .init              = dummy_init,
    .send_frame        = dummy_send_frame,
    .recv_data         = dummy_recv_data,
    .close             = dummy_close,
    .xma_version             = dummy_xma_version
};
