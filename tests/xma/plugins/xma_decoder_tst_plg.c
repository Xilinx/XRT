#include <string.h>
#include <xmaplugin.h>
#include "xma_test_plg.h"


static int32_t xma_decoder_init(XmaDecoderSession *sess)
{
    if(*(uint8_t*)sess->base.plugin_data != 0)
    {
        return XMA_ERROR;
    }
    return 0;
}

static int32_t xma_decoder_send(XmaDecoderSession *sess, XmaDataBuffer *data,
                                int32_t *data_used)
{
    return (XMA_PLG_SEND | XMA_PLG_DEC);
}

static int32_t xma_decoder_recv(XmaDecoderSession *sess, XmaFrame *frame)
{
    return (XMA_PLG_RECV | XMA_PLG_DEC);
}

static int32_t xma_decoder_close(XmaDecoderSession *sess)
{
    return 0;
}

static int32_t xma_decoder_getp(XmaDecoderSession *sess,
                                XmaFrameProperties *fprops)
{
    return XMA_PLG_DEC;
}

XmaDecoderPlugin decoder_plugin = {
    .hwdecoder_type = XMA_H264_DECODER_TYPE,
    .hwvendor_string = "Xilinx",
    .plugin_data_size = 1,
    .init = xma_decoder_init,
    .send_data = xma_decoder_send,
    .get_properties = xma_decoder_getp,
    .recv_frame = xma_decoder_recv,
    .close = xma_decoder_close,
};
