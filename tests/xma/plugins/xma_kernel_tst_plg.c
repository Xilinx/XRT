#include <string.h>
#include <xmaplugin.h>
#include "xma_test_plg.h"


static int32_t xma_kernel_init(XmaKernelSession *sess)
{
    return 0;
}

static int32_t xma_kernel_write(XmaKernelSession *sess, XmaParameter *params,
                                int32_t param_cnt)
{
    return (XMA_PLG_SEND | XMA_PLG_KERN);
}

static int32_t xma_kernel_read(XmaKernelSession *sess, XmaParameter *params,
                               int32_t *param_cnt)
{
    return (XMA_PLG_RECV | XMA_PLG_KERN);
}

static int32_t xma_kernel_close(XmaKernelSession *sess)
{
    return 0;
}

XmaKernelPlugin kernel_plugin = {
    .hwkernel_type = XMA_KERNEL_TYPE,
    .hwvendor_string = "Xilinx",
    .plugin_data_size = 0,
    .init = xma_kernel_init,
    .write = xma_kernel_write,
    .read = xma_kernel_read,
    .close = xma_kernel_close,
};
