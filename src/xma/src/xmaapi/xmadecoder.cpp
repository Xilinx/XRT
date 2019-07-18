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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "lib/xmaapi.h"
//#include "lib/xmahw_hal.h"
//#include "lib/xmares.h"
#include "app/xmalogger.h"
#include "xmaplugin.h"

#define XMA_DECODER_MOD "xmadecoder"

extern XmaSingleton *g_xma_singleton;

XmaDecoderSession*
xma_dec_session_create(XmaDecoderProperties *dec_props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);

    if (!g_xma_singleton->xma_initialized) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "XMA session creation must be after initialization\n");
        return NULL;
    }
    if (dec_props->plugin_lib == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "DecoderProperties must set plugin_lib\n");
        return NULL;
    }

    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Failed to open plugin xmaplugin.so. Error msg: %s\n",
                   dlerror());
        return NULL;
    }
    void *handle = dlopen(dec_props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            dec_props->plugin_lib, dlerror());
        return NULL;
    }

    XmaDecoderPlugin *plg =
        (XmaDecoderPlugin*)dlsym(handle, "decoder_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
            "Failed to get decoder_plugin from %s\n Error msg: %s\n",
            dec_props->plugin_lib, dlerror());
        return NULL;
    }
    if (plg->xma_version == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "DecoderPlugin library must have xma_version function\n");
        return NULL;
    }

    XmaDecoderSession *dec_session = (XmaDecoderSession*) malloc(sizeof(XmaDecoderSession));
    if (dec_session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
            "Failed to allocate memory for decoderSession\n");
        return NULL;
    }
    memset(dec_session, 0, sizeof(XmaDecoderSession));
    // init session data
    //Sarab: TODO initialize all XMaSession fields
    //It is a C-struct so doesn't have constructor..
    dec_session->decoder_props = *dec_props;
    dec_session->base.stats = NULL;
    dec_session->base.channel_id = dec_props->channel_id;
    dec_session->base.session_type = XMA_DECODER;
    dec_session->decoder_plugin = plg;

    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

    int32_t rc, dev_index, cu_index;
    dev_index = dec_props->dev_index;
    cu_index = dec_props->cu_index;
    //dec_handle = dec_props->cu_index;
    
    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices || dev_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "XMA session creation failed. dev_index not found\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return NULL;
    }

    uint32_t hwcfg_dev_index = 0;
    bool found = false;
    for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
        if (hw_device.dev_index == (uint32_t)dev_index) {
            found = true;
            break;
        }
        hwcfg_dev_index++;
    }
    if (!found) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "XMA session creation failed. dev_index not loaded with xclbin\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return NULL;
    }
    if ((uint32_t)cu_index >= hwcfg->devices[hwcfg_dev_index].number_of_cus || cu_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "XMA session creation failed. Invalid cu_index = %d\n", cu_index);
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return NULL;
    }
    if (hwcfg->devices[hwcfg_dev_index].kernels[cu_index].in_use) {
        xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
                   "XMA session sharing CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    } else {
        xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
                   "XMA session with CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    }

    dec_session->base.hw_session.dev_handle = hwcfg->devices[hwcfg_dev_index].handle;
    //For execbo:
    dec_session->base.hw_session.kernel_info = &hwcfg->devices[hwcfg_dev_index].kernels[cu_index];

    dec_session->base.hw_session.dev_index = hwcfg->devices[hwcfg_dev_index].dev_index;
    xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
                "XMA session ddr_bank: %d\n", dec_session->base.hw_session.kernel_info->ddr_bank);

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = dec_session->decoder_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    if ((xma_main_ver == 2019 && xma_sub_ver < 2) || xma_main_ver < 2019 || rc < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Initalization of plugin failed. Plugin is incompatible with this XMA version\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return NULL;
    }

    // Allocate the private data
    dec_session->base.plugin_data =
        calloc(dec_session->decoder_plugin->plugin_data_size, sizeof(uint8_t));

    dec_session->base.session_id = g_xma_singleton->num_decoders + 1;
    dec_session->base.session_signature = (void*)(((uint64_t)dec_session->base.hw_session.kernel_info) | ((uint64_t)dec_session->base.hw_session.dev_handle));
    xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
                "XMA session channel_id: %d; decoder_id: %d\n", dec_session->base.channel_id, dec_session->base.session_id);

    if (dec_session->decoder_plugin->init(dec_session)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Initalization of plugin failed\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session->base.plugin_data);
        free(dec_session);
        return NULL;
    }
    dec_session->base.hw_session.kernel_info->in_use = true;
    g_xma_singleton->num_decoders = dec_session->base.session_id;

    //Release singleton lock
    g_xma_singleton->locked = false;

    return dec_session;
}

int32_t
xma_dec_session_destroy(XmaDecoderSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    rc  = session->decoder_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Error closing decoder plugin\n");

    // Clean up the private data
    free(session->base.plugin_data);

    /* Remove xma_res stuff free kernel/kernel-session *--/
    rc = xma_res_free_kernel(g_xma_singleton->shm_res_cfg,
                             session->base.kern_res);
    if (rc)
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Error freeing kernel session. Return code %d\n", rc);

    */
    // Free the session
    // TODO: (should also free the Hw sessions)
    free(session);

    return XMA_SUCCESS;
}

int32_t
xma_dec_session_send_data(XmaDecoderSession *session,
                          XmaDataBuffer     *data,
						  int32_t           *data_used)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    return session->decoder_plugin->send_data(session, data, data_used);
}

int32_t
xma_dec_session_get_properties(XmaDecoderSession  *session,
		                       XmaFrameProperties *fprops)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    return session->decoder_plugin->get_properties(session, fprops);
}

int32_t
xma_dec_session_recv_frame(XmaDecoderSession *session,
                           XmaFrame           *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    return session->decoder_plugin->recv_frame(session, frame);
}
