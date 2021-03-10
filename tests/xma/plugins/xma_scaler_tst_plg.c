#include <string.h>
#include <xmaplugin.h>
#include "xma_test_plg.h"


static int32_t xma_scaler_init(XmaScalerSession *sess)
{
    if(*(uint8_t*)sess->base.plugin_data != 0)
    {
        return XMA_ERROR;
    }
    return 0;
}

static int32_t xma_scaler_send(XmaScalerSession *sess, XmaFrame *frame)
{
    return (XMA_PLG_SEND | XMA_PLG_SCAL);
}

static int32_t xma_scaler_recv(XmaScalerSession *sess, XmaFrame **frame_list)
{
    return (XMA_PLG_RECV | XMA_PLG_SCAL);
}

static int32_t xma_scaler_close(XmaScalerSession *sess)
{
    return 0;
}

XmaScalerPlugin scaler_plugin = {
    .hwscaler_type = XMA_POLYPHASE_SCALER_TYPE,
    .hwvendor_string = "Xilinx",
    .input_format = XMA_NONE_FMT_TYPE,
    .output_format = XMA_NONE_FMT_TYPE,
    .bits_per_pixel = 0,
    .plugin_data_size = 1,
    .init = xma_scaler_init,
    .send_frame = xma_scaler_send,
    .recv_frame_list = xma_scaler_recv,
    .close = xma_scaler_close,
    .alloc_chan = NULL,
};
