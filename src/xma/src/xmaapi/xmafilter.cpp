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
#include "xmaplugin.h"
#include <bitset>

#define XMA_FILTER_MOD "xmafilter"

extern XmaSingleton *g_xma_singleton;

XmaFilterSession*
xma_filter_session_create(XmaFilterProperties *filter_props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    if (!g_xma_singleton->xma_initialized) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "XMA session creation must be after initialization\n");
        return nullptr;
    }
    if (filter_props->plugin_lib == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "FilterProperties must set plugin_lib\n");
        return nullptr;
    }

    void *handle = dlopen(filter_props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            filter_props->plugin_lib, dlerror());
        return nullptr;
    }

    XmaFilterPlugin *plg =
        (XmaFilterPlugin*)dlsym(handle, "filter_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
            "Failed to get struct filter_plugin from %s\n Error msg: %s\n",
            filter_props->plugin_lib, dlerror());
        return nullptr;
    }
    if (plg->xma_version == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "FilterPlugin library must have xma_version function\n");
        return nullptr;
    }

    XmaFilterSession *filter_session = (XmaFilterSession*) malloc(sizeof(XmaFilterSession));
    if (filter_session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
            "Failed to allocate memory for filterSession\n");
        return nullptr;
    }
    memset(filter_session, 0, sizeof(XmaFilterSession));
    // init session data
    filter_session->props = *filter_props;
    filter_session->base.channel_id = filter_props->channel_id;
    filter_session->base.session_type = XMA_FILTER;
    filter_session->base.stats = NULL;
    filter_session->private_session_data = NULL;//Managed by host video application
    filter_session->private_session_data_size = -1;//Managed by host video application

    filter_session->filter_plugin = plg;

    int32_t rc, dev_index, cu_index;
    dev_index = filter_props->dev_index;
    cu_index = filter_props->cu_index;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices || dev_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "XMA session creation failed. dev_index not found\n");
        free(filter_session);
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
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "XMA session creation failed. dev_index not loaded with xclbin\n");
        free(filter_session);
        return nullptr;
    }
    if ((cu_index > 0 && (uint32_t)cu_index >= hwcfg->devices[hwcfg_dev_index].number_of_cus) || (cu_index < 0 && filter_props->cu_name == NULL)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "XMA session creation failed. Invalid cu_index = %d\n", cu_index);
        free(filter_session);
        return nullptr;
    }
    if (cu_index < 0) {
        std::string cu_name = std::string(filter_props->cu_name);
        found = false;
        for (XmaHwKernel& kernel: g_xma_singleton->hwcfg.devices[hwcfg_dev_index].kernels) {
            if (std::string((char*)kernel.name) == cu_name) {
                found = true;
                cu_index = kernel.cu_index;
                break;
            }
        }
        if (!found) {
            xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                    "XMA session creation failed. cu %s not found\n", cu_name.c_str());
            free(filter_session);
            return nullptr;
        }
    }

    void* dev_handle = hwcfg->devices[hwcfg_dev_index].handle;
    XmaHwKernel* kernel_info = &hwcfg->devices[hwcfg_dev_index].kernels[cu_index];

    filter_session->base.hw_session.dev_index = hwcfg->devices[hwcfg_dev_index].dev_index;

    //Allow user selected default ddr bank per XMA session
    if (xma_core::finalize_ddr_index(kernel_info, filter_props->ddr_bank_index, 
        filter_session->base.hw_session.bank_index, XMA_FILTER_MOD) != XMA_SUCCESS) {
        free(filter_session);
        return nullptr;
    }

    if (kernel_info->kernel_channels) {
        if (filter_session->base.channel_id > (int32_t)kernel_info->max_channel_id) {
            xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                "Selected dataflow CU with channels has ini setting with max channel_id of %d. Cannot create session with higher channel_id of %d\n", kernel_info->max_channel_id, filter_session->base.channel_id);
            
            free(filter_session);
            return nullptr;
        }
    }

    // Call the plugins initialization function with this session data
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = filter_session->filter_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    int32_t tmp_check = xma_core::check_plugin_version(xma_main_ver, xma_sub_ver);

    if (rc < 0 || tmp_check == -1) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Initalization of plugin failed. Plugin is incompatible with this XMA version\n");
        free(filter_session);
        return nullptr;
    }
    if (tmp_check <= -2) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Initalization of plugin failed. Newer plugin is not allowed with old XMA library\n");
        free(filter_session);
        return nullptr;
    }

    XmaHwDevice& dev_tmp1 = hwcfg->devices[hwcfg_dev_index];
    // Allocate the private data
    filter_session->base.plugin_data =
        calloc(filter_session->filter_plugin->plugin_data_size, sizeof(uint8_t));

    XmaHwSessionPrivate *priv1 = new XmaHwSessionPrivate();
    priv1->dev_handle = dev_handle;
    priv1->kernel_info = kernel_info;
    priv1->kernel_complete_count = 0;
    priv1->device = &hwcfg->devices[hwcfg_dev_index];
    filter_session->base.hw_session.private_do_not_use = (void*) priv1;
    filter_session->base.session_signature = (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved));

    int32_t num_execbo = g_xma_singleton->num_execbos;
    priv1->kernel_execbos.reserve(num_execbo);
    priv1->num_execbo_allocated = num_execbo;
    if (xma_core::create_session_execbo(priv1, num_execbo, XMA_FILTER_MOD) != XMA_SUCCESS) {
        free(filter_session->base.plugin_data);
        free(filter_session);
        delete priv1;
        return nullptr;
    }

    //Obtain lock only for a) singleton changes & b) kernel_info changes
    std::unique_lock<std::mutex> guard1(g_xma_singleton->m_mutex);
    //Singleton lock acquired

    if (!kernel_info->soft_kernel && !kernel_info->in_use && !kernel_info->context_opened) {
        if (xclOpenContext(dev_handle, dev_tmp1.uuid, kernel_info->cu_index_ert, true) != 0) {
            xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD, "Failed to open context to CU %s for this session\n", kernel_info->name);
            free(filter_session->base.plugin_data);
            free(filter_session);
            delete priv1;
            return nullptr;
        }
    }
    filter_session->base.session_id = g_xma_singleton->num_of_sessions + 1;
    xma_logmsg(XMA_INFO_LOG, XMA_FILTER_MOD,
                "XMA session channel_id: %d; session_id: %d\n", filter_session->base.channel_id, filter_session->base.session_id);

    if (kernel_info->in_use) {
        kernel_info->is_shared = true;
        xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD,
                   "XMA session sharing CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    } else {
        kernel_info->in_use = true;
        xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD,
                   "XMA session with CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    }
    kernel_info->num_sessions++;
    g_xma_singleton->num_filters++;
    g_xma_singleton->num_of_sessions = filter_session->base.session_id;

    g_xma_singleton->all_sessions_vec.push_back(filter_session->base);

    //Release singleton lock
    guard1.unlock();

    //init can execute cu cmds as well so must be fater adding to singleton above
    rc = filter_session->filter_plugin->init(filter_session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Initalization of filter plugin failed. Return code %d\n",
                   rc);
        free(filter_session->base.plugin_data);
        //free(filter_session); Added to singleton above; Keep it as checked for cu cmds
        //delete priv1;
        return nullptr;
    }

    return filter_session;
}

int32_t
xma_filter_session_destroy(XmaFilterSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    std::lock_guard<std::mutex> guard1(g_xma_singleton->m_mutex);
    //Singleton lock acquired

    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Session is already released\n");

        return XMA_ERROR;
    }
    if (session->base.hw_session.private_do_not_use == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Session is corrupted\n");

        return XMA_ERROR;
    }
    if (session->filter_plugin == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Session is corrupted\n");

        return XMA_ERROR;
    }
    rc  = session->filter_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Error closing filter plugin\n");

    // Clean up the private data
    free(session->base.plugin_data);

    // Free the session
    /*
    delete (XmaHwSessionPrivate*)session->base.hw_session.private_do_not_use;
    */
    session->base.hw_session.private_do_not_use = nullptr;
    session->base.plugin_data = nullptr;
    session->base.stats = NULL;
    session->filter_plugin = NULL;
    //do not change kernel in_use as it maybe in use by another plugin
    session->base.hw_session.dev_index = -1;
    session->base.session_signature = NULL;
    free(session);
    session = nullptr;

    return XMA_SUCCESS;
}

int32_t
xma_filter_session_send_frame(XmaFilterSession  *session,
                              XmaFrame          *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "xma_filter_session_send_frame failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD, "xma_filter_session_send_frame failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }

    return session->filter_plugin->send_frame(session, frame);
}

int32_t
xma_filter_session_recv_frame(XmaFilterSession  *session,
                              XmaFrame          *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "xma_filter_session_recv_frame failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD, "xma_filter_session_recv_frame failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    return session->filter_plugin->recv_frame(session, frame);
}
