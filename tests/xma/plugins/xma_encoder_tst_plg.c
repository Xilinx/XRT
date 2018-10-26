#include <string.h>
#include <xmaplugin.h>
#include "xma_test_plg.h"


static int32_t xma_encoder_init(XmaEncoderSession *sess)
{
    return 0;
}

static int32_t xma_encoder_send(XmaEncoderSession *sess, XmaFrame *frame)
{
    return (XMA_PLG_SEND | XMA_PLG_ENC);
}

static int32_t xma_encoder_recv(XmaEncoderSession *sess, XmaDataBuffer *data,
                                int32_t *data_size)
{
    return (XMA_PLG_RECV | XMA_PLG_ENC);
}

static int32_t xma_encoder_close(XmaEncoderSession *sess)
{
    return 0;
}

XmaEncoderPlugin encoder_plugin = {
    .hwencoder_type = XMA_COPY_ENCODER_TYPE,
    .hwvendor_string = "Xilinx",
    .format = XMA_NONE_FMT_TYPE,
    .bits_per_pixel = 0,
    .kernel_data_size = 0,
    .plugin_data_size = 0,
    .init = xma_encoder_init,
    .send_frame = xma_encoder_send,
    .recv_data = xma_encoder_recv,
    .close = xma_encoder_close,
    .alloc_chan = NULL,
    .get_dev_input_paddr = NULL,
};
