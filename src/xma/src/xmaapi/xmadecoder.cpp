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
#include "lib/xmahw_hal.h"
//#include "lib/xmares.h"
#include "app/xmalogger.h"
#include "xmaplugin.h"

#define XMA_DECODER_MOD "xmadecoder"

extern XmaSingleton *g_xma_singleton;

XmaDecoderSession*
xma_dec_session_create(XmaDecoderProperties *dec_props)
{
    XmaDecoderSession *dec_session = (XmaDecoderSession*) malloc(sizeof(XmaDecoderSession));
    if (dec_session == NULL)
        return NULL;
    //XmaResources xma_shm_cfg = g_xma_singleton->shm_res_cfg;
    //XmaKernelRes kern_res;

    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    /*Sarab: Remove xma_res stuff
    if (!xma_shm_cfg) {
        free(dec_session);
        return NULL;
    }
    */

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
    //g_xma_singleton->decoders.emplace_back(plg);
    memset(dec_session, 0, sizeof(XmaDecoderSession));
    // init session data
    dec_session->decoder_props = *dec_props;
    dec_session->base.channel_id = dec_props->channel_id;
    dec_session->base.session_type = XMA_DECODER;
    dec_session->decoder_plugin = plg;

    /*Sarab: Remove xma_res stuff
    // Just assume this is a H.264 decoder for now and that the FPGA
    // has been downloaded.  This is accomplished by getting the
    // first device (dev_handle, base_addr, ddr_bank) and making a
    // XmaHwSession out of it.  Later this needs to be done by searching
    // for an available resource.
    /--* JPM TODO default to exclusive device access.  Ensure multiple threads
       can access this device if in-use pid = requesting thread pid *--/
    rc = xma_res_alloc_dec_kernel(xma_shm_cfg, dec_props->hwdecoder_type,
                                  dec_props->hwvendor_string,
                                  &dec_session->base, false);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Failed to allocate free decoder kernel. Return code %d\n",
                   rc);
        free(dec_session);
        return NULL;
    }

    kern_res = dec_session->base.kern_res;

    dev_handle = xma_res_dev_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
               "dev_handle = %d\n", dev_handle);
    if (dev_handle < 0) {
        free(dec_session);
        return NULL;
    }

    kern_handle = xma_res_kern_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
               "kern_handle = %d\n", kern_handle);
    if (kern_handle < 0) {
        free(dec_session);
        return NULL;
    }

    dec_handle = xma_res_plugin_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
              "dec_handle = %d\n", dec_handle);
    if (dec_handle < 0) {
        free(dec_session);
        return NULL;
    }
    */

    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

   //Sarab: TODO Fix device index, CU index & session->xx_plugin assigned above
    //int rc, dev_handle, kern_handle, dec_handle;
    int rc, dev_index, cu_index;
    dev_index = dec_props->dev_index;
    cu_index = dec_props->cu_index;
    //dec_handle = dec_props->cu_index;
    
    g_xma_singleton->num_decoders++;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    XmaHwHAL *hal = (XmaHwHAL*)hwcfg->devices[dev_index].handle;

    dec_session->base.hw_session.dev_handle = hal->dev_handle;
    //For execbo:
    dec_session->base.hw_session.kernel_info = &hwcfg->devices[dev_index].kernels[cu_index];

    dec_session->base.hw_session.dev_index = hal->dev_index;

    //dec_session->decoder_plugin = &g_xma_singleton->decodercfg[dec_handle];

    // Allocate the private data
    dec_session->base.plugin_data =
        calloc(dec_session->decoder_plugin->plugin_data_size, sizeof(uint8_t));

    dec_session->base.session_id = g_xma_singleton->num_decoders;
    dec_session->base.session_signature = (void*)(((uint64_t)dec_session->base.hw_session.kernel_info) | ((uint64_t)dec_session->base.hw_session.dev_handle));

    //Release singleton lock
    g_xma_singleton->locked = false;

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = dec_session->decoder_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    if (rc < 0) {
        return NULL;
    }
    //Sarab: TODO. Check version match. Stop here for now
    //Sarab: Remove it later on
    return NULL;

    if (dec_session->decoder_plugin->init(dec_session)) {
        free(dec_session->base.plugin_data);
        free(dec_session);
        return NULL;
    }

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
