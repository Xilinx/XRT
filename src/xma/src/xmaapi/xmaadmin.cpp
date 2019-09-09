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
#include "xmaplugin.h"
#include <bitset>

#define XMA_ADMIN_MOD "xmaadmin"

extern XmaSingleton *g_xma_singleton;

XmaAdminSession*
xma_admin_session_create(XmaAdminProperties *props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_ADMIN_MOD, "%s()\n", __func__);
    if (!g_xma_singleton->xma_initialized) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "XMA session creation must be after initialization\n");
        return NULL;
    }
    if (props->plugin_lib == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "AdminProperties must set plugin_lib\n");
        return NULL;
    }

    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "Failed to open plugin xmaplugin.so. Error msg: %s\n",
                   dlerror());
        return NULL;
    }
    void *handle = dlopen(props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            props->plugin_lib, dlerror());
        return NULL;
    }

    XmaAdminPlugin *plg =
        (XmaAdminPlugin*)dlsym(handle, "admin_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
            "Failed to get admin_plugin from %s\n Error msg: %s\n",
            props->plugin_lib, dlerror());
        return NULL;
    }
    if (plg->xma_version == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "AdminPlugin library must have xma_version function\n");
        return NULL;
    }

    XmaAdminSession *session = (XmaAdminSession*) malloc(sizeof(XmaAdminSession));
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
            "Failed to allocate memory for AdminSession\n");
        return NULL;
    }
    memset(session, 0, sizeof(XmaAdminSession));
    // init session data
    session->admin_props = *props;
    session->base.session_type = XMA_ADMIN;
    session->base.stats = NULL;
    session->base.channel_id = -1;
    session->private_session_data = NULL;//Managed by host video application
    session->private_session_data_size = -1;//Managed by host video application
    session->admin_plugin = plg;

    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

    int32_t rc, dev_index;
    dev_index = props->dev_index;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices || dev_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "XMA session creation failed. dev_index not found\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(session);
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
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "XMA session creation failed. dev_index not loaded with xclbin\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(session);
        return NULL;
    }

    void* dev_handle = hwcfg->devices[hwcfg_dev_index].handle;
    session->base.hw_session.dev_index = hwcfg->devices[hwcfg_dev_index].dev_index;
    session->base.hw_session.bank_index = -1;

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = session->admin_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    if ((xma_main_ver == 2019 && xma_sub_ver < 2) || xma_main_ver < 2019 || rc < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "Initalization of plugin failed. Plugin is incompatible with this XMA version\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(session);
        return NULL;
    }

    // Allocate the private data
    session->base.plugin_data =
        calloc(session->admin_plugin->plugin_data_size, sizeof(uint8_t));

    session->base.session_id = g_xma_singleton->num_of_sessions + 1;
    xma_logmsg(XMA_INFO_LOG, XMA_ADMIN_MOD,
                "XMA session_id: %d\n", session->base.session_id);

    XmaHwSessionPrivate *priv1 = new XmaHwSessionPrivate();
    priv1->dev_handle = dev_handle;
    priv1->kernel_info = NULL;
    priv1->kernel_complete_count = 0;
    priv1->device = &hwcfg->devices[hwcfg_dev_index];
    session->base.hw_session.private_do_not_use = (void*) priv1;

    session->base.session_signature = (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved));

    rc = session->admin_plugin->init(session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "Initalization of kernel plugin failed. Return code %d\n",
                   rc);
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(session->base.plugin_data);
        free(session);
        delete priv1;
        return NULL;
    }

    g_xma_singleton->num_admins++;
    g_xma_singleton->num_of_sessions = session->base.session_id;
    g_xma_singleton->all_sessions.emplace(g_xma_singleton->num_of_sessions, session->base);

    //Release singleton lock
    g_xma_singleton->locked = false;

    return session;
}

int32_t
xma_admin_session_destroy(XmaAdminSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_ADMIN_MOD, "%s()\n", __func__);
    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "Session is already released\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    if (session->base.hw_session.private_do_not_use == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "Session is corrupted\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    if (session->admin_plugin == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "Session is corrupted\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    rc  = session->admin_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD,
                   "Error closing admin plugin\n");

    // Clean up the private data
    free(session->base.plugin_data);

    // Free the session
    delete (XmaHwSessionPrivate*)session->base.hw_session.private_do_not_use;
    session->base.hw_session.private_do_not_use = NULL;
    session->base.plugin_data = NULL;
    session->base.stats = NULL;
    session->admin_plugin = NULL;
    //do not change kernel in_use as it maybe in use by another plugin
    session->base.hw_session.dev_index = -1;
    session->base.session_signature = NULL;
    free(session);
    session = NULL;

    //Release singleton lock
    g_xma_singleton->locked = false;

    return XMA_SUCCESS;
}

int32_t
xma_admin_session_write(XmaAdminSession *session,
                         XmaParameter     *param,
                         int32_t           param_cnt)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_ADMIN_MOD, "%s()\n", __func__);
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    return session->admin_plugin->write(session, param, param_cnt);
}

int32_t
xma_admin_session_read(XmaAdminSession *session,
                        XmaParameter      *param,
                        int32_t           *param_cnt)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_ADMIN_MOD, "%s()\n", __func__);
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ADMIN_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    return session->admin_plugin->read(session, param, param_cnt);
}
