#include <string.h>
#include <xmaplugin.h>
#include "xma_test_plg.h"


static int32_t xma_filter_init(XmaFilterSession *sess)
{
    return 0;
}

static int32_t xma_filter_send(XmaFilterSession *sess, XmaFrame *frame)
{
    return (XMA_PLG_SEND | XMA_PLG_FIL);
}

static int32_t xma_filter_recv(XmaFilterSession *sess, XmaFrame *frame)
{
    return (XMA_PLG_RECV | XMA_PLG_FIL);
}

static int32_t xma_filter_close(XmaFilterSession *sess)
{
    return 0;
}

XmaFilterPlugin filter_plugin = {
    .hwfilter_type = XMA_2D_FILTER_TYPE,
    .hwvendor_string = "Xilinx",
    .plugin_data_size = 0,
    .init = xma_filter_init,
    .send_frame = xma_filter_send,
    .recv_frame = xma_filter_recv,
    .close = xma_filter_close,
    .alloc_chan = NULL,
};
