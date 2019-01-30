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
#include "lib/xmares.h"
#include "xmaplugin.h"

#define XMA_KERNEL_MOD "xmakernel"

extern XmaSingleton *g_xma_singleton;

int32_t
xma_kernel_plugins_load(XmaSystemCfg      *systemcfg,
                        XmaKernelPlugin   *kernels)
{
    // Get the plugin path
    char *pluginpath = systemcfg->pluginpath;
    char *error;
    int32_t k = 0;

    xma_logmsg(XMA_DEBUG_LOG, XMA_KERNEL_MOD, "%s()\n", __func__);
    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Failed to open plugin xmaplugin.so. Error msg: %s\n",
                   dlerror());
        return XMA_ERROR;
    }

    // For each plugin imagecfg/kernelcfg,
    for (int32_t i = 0; i < systemcfg->num_images; i++)
    {
        for (int32_t j = 0;
             j < systemcfg->imagecfg[i].num_kernelcfg_entries; j++)
        {
            char *func = systemcfg->imagecfg[i].kernelcfg[j].function;
            if (strcmp(func, XMA_CFG_FUNC_NM_KERNEL) != 0)
                continue;
            char *plugin = systemcfg->imagecfg[i].kernelcfg[j].plugin;
            char pluginfullname[PATH_MAX + NAME_MAX];
            sprintf(pluginfullname, "%s/%s", pluginpath, plugin);
            void *handle = dlopen(pluginfullname, RTLD_NOW);
            if (!handle)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }

            XmaKernelPlugin *plg =
                (XmaKernelPlugin*)dlsym(handle, "kernel_plugin");
            if ((error = dlerror()) != NULL)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }
            memcpy(&kernels[k++], plg, sizeof(XmaKernelPlugin));
        }
    }
    return XMA_SUCCESS;
}

XmaKernelSession*
xma_kernel_session_create(XmaKernelProperties *props)
{
    XmaKernelSession *session = malloc(sizeof(XmaKernelSession));
    XmaResources xma_shm_cfg = g_xma_singleton->shm_res_cfg;
    XmaKernelRes kern_res;
    int rc, dev_handle, kern_handle, k_handle;

    xma_logmsg(XMA_DEBUG_LOG, XMA_KERNEL_MOD, "%s()\n", __func__);
    if (!xma_shm_cfg) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "No reference to xma res database\n");
        free(session);
        return NULL;
    }

    memset(session, 0, sizeof(XmaKernelSession));
    // init session data
    session->kernel_props = *props;
    session->base.chan_id = -1;
    session->base.session_type = XMA_KERNEL;

    rc = xma_res_alloc_kernel_kernel(xma_shm_cfg, props->hwkernel_type,
                                     props->hwvendor_string,
                                     &session->base, false);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Failed to allocate free kernel. Return code %d\n", rc);
        return NULL;
    }

    kern_res = session->base.kern_res;

    dev_handle = xma_res_dev_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_KERNEL_MOD,"dev_handle = %d\n", dev_handle);
    if (dev_handle < 0)
        return NULL;

    kern_handle = xma_res_kern_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_KERNEL_MOD,"kern_handle = %d\n", kern_handle);
    if (kern_handle < 0)
        return NULL;

    k_handle = xma_res_plugin_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_KERNEL_MOD,"kernel_handle = %d\n", k_handle);
    if (k_handle < 0)
        return NULL;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    XmaHwHAL *hal = (XmaHwHAL*)hwcfg->devices[dev_handle].handle;

    session->base.hw_session.dev_handle = hal->dev_handle;
    session->base.hw_session.base_address =
        hwcfg->devices[dev_handle].kernels[kern_handle].base_address;
    session->base.hw_session.ddr_bank =
        hwcfg->devices[dev_handle].kernels[kern_handle].ddr_bank;

    //For execbo:
    session->base.hw_session.kernel_info = hwcfg->devices[dev_handle].kernels[kern_handle];
    session->base.hw_session.dev_index = hal->dev_index;

    session->kernel_plugin = &g_xma_singleton->kernelcfg[k_handle];

    // Allocate the private data
    session->base.plugin_data =
        calloc(g_xma_singleton->kernelcfg[k_handle].plugin_data_size, sizeof(uint8_t));

    // Call the plugins initialization function with this session data
    rc = session->kernel_plugin->init(session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Initalization of kernel plugin failed. Return code %d\n",
                   rc);
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

    /* free kernel/kernel-session */
    rc = xma_res_free_kernel(g_xma_singleton->shm_res_cfg,
                             session->base.kern_res);
    if (rc)
        xma_logmsg(XMA_ERROR_LOG, XMA_KERNEL_MOD,
                   "Error freeing kernel session. Return code %d\n", rc);

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
