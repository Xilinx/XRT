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
#include "app/xma_utils.hpp"
#include "lib/xma_utils.hpp"
//#include "lib/xmahw_hal.h"
//#include "lib/xmares.h"
#include "app/xmalogger.h"
#include "xmaplugin.h"
#include <bitset>

#define XMA_DECODER_MOD "xmadecoder"

extern XmaSingleton *g_xma_singleton;

XmaDecoderSession*
xma_dec_session_create(XmaDecoderProperties *dec_props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);

    if (!g_xma_singleton->xma_initialized) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "XMA session creation must be after initialization\n");
        return nullptr;
    }
    if (dec_props->plugin_lib == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "DecoderProperties must set plugin_lib\n");
        return nullptr;
    }

    void *handle = dlopen(dec_props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            dec_props->plugin_lib, dlerror());
        return nullptr;
    }

    XmaDecoderPlugin *plg =
        (XmaDecoderPlugin*)dlsym(handle, "decoder_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
            "Failed to get struct decoder_plugin from %s\n Error msg: %s\n",
            dec_props->plugin_lib, dlerror());
        return nullptr;
    }
    if (plg->xma_version == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "DecoderPlugin library must have xma_version function\n");
        return nullptr;
    }

    XmaDecoderSession *dec_session = (XmaDecoderSession*) malloc(sizeof(XmaDecoderSession));
    if (dec_session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
            "Failed to allocate memory for decoderSession\n");
        return nullptr;
    }
    memset(dec_session, 0, sizeof(XmaDecoderSession));
    // init session data
    //Sarab: TODO initialize all XMaSession fields
    //It is a C-struct so doesn't have constructor..
    dec_session->decoder_props = *dec_props;
    dec_session->base.stats = NULL;
    dec_session->base.channel_id = dec_props->channel_id;
    dec_session->base.session_type = XMA_DECODER;
    dec_session->private_session_data = NULL;//Managed by host video application
    dec_session->private_session_data_size = -1;//Managed by host video application

    dec_session->decoder_plugin = plg;

    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
        return nullptr;
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
        return nullptr;
    }
    if ((cu_index > 0 && (uint32_t)cu_index >= hwcfg->devices[hwcfg_dev_index].number_of_cus) || (cu_index < 0 && dec_props->cu_name == NULL)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "XMA session creation failed. Invalid cu_index = %d\n", cu_index);
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return nullptr;
    }
    if (cu_index < 0) {
        std::string cu_name = std::string(dec_props->cu_name);
        found = false;
        for (XmaHwKernel& kernel: g_xma_singleton->hwcfg.devices[hwcfg_dev_index].kernels) {
            if (std::string((char*)kernel.name) == cu_name) {
                found = true;
                cu_index = kernel.cu_index;
                break;
            }
        }
        if (!found) {
            xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                    "XMA session creation failed. cu %s not found\n", cu_name.c_str());
            //Release singleton lock
            g_xma_singleton->locked = false;
            free(dec_session);
            return nullptr;
        }
    }

    if (hwcfg->devices[hwcfg_dev_index].kernels[cu_index].in_use) {
        xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD,
                   "XMA session sharing CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    } else {
        xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD,
                   "XMA session with CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    }

    void* dev_handle = hwcfg->devices[hwcfg_dev_index].handle;
    XmaHwKernel* kernel_info = &hwcfg->devices[hwcfg_dev_index].kernels[cu_index];
    dec_session->base.hw_session.dev_index = hwcfg->devices[hwcfg_dev_index].dev_index;

    //Allow user selected default ddr bank per XMA session
    if (xma_core::finalize_ddr_index(kernel_info, dec_props->ddr_bank_index, 
        dec_session->base.hw_session.bank_index, XMA_DECODER_MOD) != XMA_SUCCESS) {
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return nullptr;
    }

    if (kernel_info->kernel_channels) {
        if (dec_session->base.channel_id > (int32_t)kernel_info->max_channel_id) {
            xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                "Selected dataflow CU with channels has ini setting with max channel_id of %d. Cannot create session with higher channel_id of %d\n", kernel_info->max_channel_id, dec_session->base.channel_id);
            
            //Release singleton lock
            g_xma_singleton->locked = false;
            free(dec_session);
            return nullptr;
        }
    }

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = dec_session->decoder_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    int32_t tmp_check = xma_core::check_plugin_version(xma_main_ver, xma_sub_ver);

    if (rc < 0 || tmp_check == -1) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Initalization of plugin failed. Plugin is incompatible with this XMA version\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return nullptr;
    }
    if (tmp_check <= -2) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Initalization of plugin failed. Newer plugin is not allowed with old XMA library\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session);
        return nullptr;
    }

    XmaHwDevice& dev_tmp1 = hwcfg->devices[hwcfg_dev_index];
    if (!kernel_info->soft_kernel && !kernel_info->in_use) {
        if (xclOpenContext(dev_handle, dev_tmp1.uuid, kernel_info->cu_index_ert, true) != 0) {
            xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD, "Failed to open context to CU %s for this session\n", kernel_info->name);
            //Release singleton lock
            g_xma_singleton->locked = false;
            free(dec_session);
            return nullptr;
        }
    }
    // Allocate the private data
    dec_session->base.plugin_data =
        calloc(dec_session->decoder_plugin->plugin_data_size, sizeof(uint8_t));

    dec_session->base.session_id = g_xma_singleton->num_of_sessions + 1;
    xma_logmsg(XMA_INFO_LOG, XMA_DECODER_MOD,
                "XMA session channel_id: %d; session_id: %d\n", dec_session->base.channel_id, dec_session->base.session_id);

    XmaHwSessionPrivate *priv1 = new XmaHwSessionPrivate();
    priv1->dev_handle = dev_handle;
    priv1->kernel_info = kernel_info;
    priv1->kernel_complete_count = 0;
    priv1->device = &hwcfg->devices[hwcfg_dev_index];
    dec_session->base.hw_session.private_do_not_use = (void*) priv1;
    dec_session->base.session_signature = (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved));

    int32_t num_execbo = g_xma_singleton->num_execbos;
    priv1->kernel_execbos.reserve(num_execbo);
    priv1->num_execbo_allocated = num_execbo;
    if (xma_core::create_session_execbo(priv1, num_execbo, XMA_DECODER_MOD) != XMA_SUCCESS) {
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session->base.plugin_data);
        free(dec_session);
        delete priv1;
        return nullptr;
    }

    if (dec_session->decoder_plugin->init(dec_session)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Initalization of plugin failed\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(dec_session->base.plugin_data);
        free(dec_session);
        delete priv1;
        return nullptr;
    }
    if (kernel_info->in_use) {
        kernel_info->is_shared = true;
    } else {
        kernel_info->in_use = true;
    }
    kernel_info->num_sessions++;
    g_xma_singleton->num_decoders++;
    g_xma_singleton->num_of_sessions = dec_session->base.session_id;

    g_xma_singleton->all_sessions.emplace(g_xma_singleton->num_of_sessions, dec_session->base);

    //Release singleton lock
    g_xma_singleton->locked = false;

    return dec_session;
}

int32_t
xma_dec_session_destroy(XmaDecoderSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        expected = false;
    }
    //Singleton lock acquired

    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Session is already released\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    if (session->base.hw_session.private_do_not_use == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Session is corrupted\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    if (session->decoder_plugin == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "Session is corrupted\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
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
    /*
    delete (XmaHwSessionPrivate*)session->base.hw_session.private_do_not_use;
    */
    session->base.hw_session.private_do_not_use = nullptr;
    session->base.plugin_data = nullptr;
    session->base.stats = NULL;
    session->decoder_plugin = NULL;
    //do not change kernel in_use as it maybe in use by another plugin
    session->base.hw_session.dev_index = -1;
    session->base.session_signature = NULL;
    free(session);
    session = nullptr;

    //Release singleton lock
    g_xma_singleton->locked = false;

    return XMA_SUCCESS;
}

int32_t
xma_dec_session_send_data(XmaDecoderSession *session,
                          XmaDataBuffer     *data,
						  int32_t           *data_used)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "xma_dec_session_send_data failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD, "xma_dec_session_send_data failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    return session->decoder_plugin->send_data(session, data, data_used);
}

int32_t
xma_dec_session_get_properties(XmaDecoderSession  *session,
		                       XmaFrameProperties *fprops)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "xma_dec_session_get_properties failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD, "xma_dec_session_get_properties failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    return session->decoder_plugin->get_properties(session, fprops);
}

int32_t
xma_dec_session_recv_frame(XmaDecoderSession *session,
                           XmaFrame           *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_DECODER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD,
                   "xma_dec_session_recv_frame failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD, "xma_dec_session_recv_frame failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_DECODER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    return session->decoder_plugin->recv_frame(session, frame);
}
