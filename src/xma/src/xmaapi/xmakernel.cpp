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
#include "xmaplugin.h"

#define XMA_KERNEL_MOD "xmakernel"

extern XmaSingleton *g_xma_singleton;

XmaKernelSession*
xma_kernel_session_create(XmaKernelProperties *props)
{
    if (!g_xma_singleton->xma_initialized) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "XMA session creation must be after initialization\n");
        return NULL;
    }

    XmaKernelSession *session = (XmaKernelSession*) malloc(sizeof(XmaKernelSession));
    if (session == NULL) {
        return NULL;
    }
    //XmaResources xma_shm_cfg = g_xma_singleton->shm_res_cfg;
    //XmaKernelRes kern_res;

    xma_logmsg(XMA_DEBUG_LOG, XMA_KERNEL_MOD, "%s()\n", __func__);
    /*Sarab: Remove xma_res stuff
    if (!xma_shm_cfg) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "No reference to xma res database\n");
        free(session);
        return NULL;
    }
    */

    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Failed to open plugin xmaplugin.so. Error msg: %s\n",
                   dlerror());
        return NULL;
    }
    void *handle = dlopen(props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            props->plugin_lib, dlerror());
        return NULL;
    }

    XmaKernelPlugin *plg =
        (XmaKernelPlugin*)dlsym(handle, "kernel_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
            "Failed to get kernel_plugin from %s\n Error msg: %s\n",
            props->plugin_lib, dlerror());
        return NULL;
    }

    memset(session, 0, sizeof(XmaKernelSession));
    // init session data
    session->kernel_props = *props;
    session->base.channel_id = props->channel_id;
    session->base.session_type = XMA_KERNEL;
    session->base.stats = NULL;
    session->kernel_plugin = plg;

    /*Sarab: Remove xma_res stuff
    rc = xma_res_alloc_kernel_kernel(xma_shm_cfg, props->hwkernel_type,
                                     props->hwvendor_string,
                                     &session->base, false);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Failed to allocate free kernel. Return code %d\n", rc);
        free(session);
        return NULL;
    }

    kern_res = session->base.kern_res;

    dev_handle = xma_res_dev_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_KERNEL_MOD,"dev_handle = %d\n", dev_handle);
    if (dev_handle < 0) {
        free(session);
        return NULL;
    }

    kern_handle = xma_res_kern_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_KERNEL_MOD,"kern_handle = %d\n", kern_handle);
    if (kern_handle < 0) {
        free(session);
        return NULL;
    }

    k_handle = xma_res_plugin_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_KERNEL_MOD,"kernel_handle = %d\n", k_handle);
    if (k_handle < 0) {
        free(session);
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
    int rc, dev_index, cu_index;
    dev_index = props->dev_index;
    cu_index = props->cu_index;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "XMA session creation failed. dev_index not found\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        return NULL;
    }

    g_xma_singleton->num_kernels++;
    
    session->base.hw_session.dev_handle = hwcfg->devices[dev_index].handle;

    //For execbo:
    session->base.hw_session.kernel_info = &hwcfg->devices[dev_index].kernels[cu_index];

    session->base.hw_session.dev_index = hwcfg->devices[dev_index].dev_index;

    //session->kernel_plugin = &g_xma_singleton->kernelcfg[k_handle];

    // Allocate the private data
    session->base.plugin_data =
        calloc(session->kernel_plugin->plugin_data_size, sizeof(uint8_t));


    session->base.session_id = g_xma_singleton->num_kernels;
    session->base.session_signature = (void*)(((uint64_t)session->base.hw_session.kernel_info) | ((uint64_t)session->base.hw_session.dev_handle));

    //Release singleton lock
    g_xma_singleton->locked = false;

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = session->kernel_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    //Sarab: TODO. Check version match. Stop here for now
    //Sarab: Remove it later on
    if (rc < 0) {
        return NULL;
    }
    return NULL;

    rc = session->kernel_plugin->init(session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Initalization of kernel plugin failed. Return code %d\n",
                   rc);
        free(session->base.plugin_data);
        free(session);
        return NULL;
    }

    return session;
}

int32_t
xma_kernel_session_destroy(XmaKernelSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_KERNEL_MOD, "%s()\n", __func__);
    rc  = session->kernel_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Error closing kernel plugin\n");

    // Clean up the private data
    free(session->base.plugin_data);

    /* Remove xma_res stuff free kernel/kernel-session *--/
    rc = xma_res_free_kernel(g_xma_singleton->shm_res_cfg,
                             session->base.kern_res);
    if (rc)
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Error freeing kernel session. Return code %d\n", rc);
    */
    // Free the session
    // TODO: (should also free the Hw sessions)
    free(session);

    return XMA_SUCCESS;
}

int32_t
xma_kernel_session_write(XmaKernelSession *session,
                         XmaParameter     *param,
                         int32_t           param_cnt)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_KERNEL_MOD, "%s()\n", __func__);
    return session->kernel_plugin->write(session, param, param_cnt);
}

int32_t
xma_kernel_session_read(XmaKernelSession *session,
                        XmaParameter      *param,
                        int32_t           *param_cnt)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_KERNEL_MOD, "%s()\n", __func__);
    return session->kernel_plugin->read(session, param, param_cnt);
}
