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

#define XMA_FILTER_MOD "xmafilter"

extern XmaSingleton *g_xma_singleton;

int32_t
xma_filter_plugins_load(XmaSystemCfg      *systemcfg,
                       XmaFilterPlugin    *filters)
{
    // Get the plugin path
    char *pluginpath = systemcfg->pluginpath;
    char *error;
    int32_t k = 0;

    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
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
            if (strcmp(func, XMA_CFG_FUNC_NM_FILTER) != 0)
                continue;
            char *plugin = systemcfg->imagecfg[i].kernelcfg[j].plugin;
            char pluginfullname[PATH_MAX + NAME_MAX];
            sprintf(pluginfullname, "%s/%s", pluginpath, plugin);
            void *handle = dlopen(pluginfullname, RTLD_NOW);
            if (!handle)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }

            XmaFilterPlugin *plg =
                (XmaFilterPlugin*)dlsym(handle, "filter_plugin");
            if ((error = dlerror()) != NULL)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }
            memcpy(&filters[k++], plg, sizeof(XmaFilterPlugin));
        }
    }
    return XMA_SUCCESS;
}

XmaFilterSession*
xma_filter_session_create(XmaFilterProperties *filter_props)
{
    XmaFilterSession *filter_session = malloc(sizeof(XmaFilterSession));
	XmaResources xma_shm_cfg = g_xma_singleton->shm_res_cfg;
    XmaKernelRes kern_res;
	int rc, dev_handle, kern_handle, filter_handle;

	if (!xma_shm_cfg) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "No reference to xma res database\n");
        free(filter_session);
		return NULL;
    }

    memset(filter_session, 0, sizeof(XmaFilterSession));
    // init session data
    filter_session->props = *filter_props;
    filter_session->base.chan_id = -1;
    filter_session->base.session_type = XMA_FILTER;

    // Just assume this is an ABR filter for now and that the FPGA
    // has been downloaded.  This is accomplished by getting the
    // first device (dev_handle, base_addr, ddr_bank) and making a
    // XmaHwSession out of it.  Later this needs to be done by searching
    // for an available resource.
	/* JPM TODO default to exclusive access.  Ensure multiple threads
	   can access this device if in-use pid = requesting thread pid */
    rc = xma_res_alloc_filter_kernel(xma_shm_cfg,
                                     filter_props->hwfilter_type,
                                     filter_props->hwvendor_string,
		                             &filter_session->base, false);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Failed to allocate free filter kernel. Return code %d\n", rc);
        return NULL;
    }

    kern_res = filter_session->base.kern_res;

    dev_handle = xma_res_dev_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_FILTER_MOD,"dev_handle = %d\n", dev_handle);
    if (dev_handle < 0)
        return NULL;

    kern_handle = xma_res_kern_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_FILTER_MOD,"kern_handle = %d\n", kern_handle);
    if (kern_handle < 0)
        return NULL;

    filter_handle = xma_res_plugin_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_FILTER_MOD,"filter_handle = %d\n",
               filter_handle);
    if (filter_handle < 0)
        return NULL;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    XmaHwHAL *hal = (XmaHwHAL*)hwcfg->devices[dev_handle].handle;
    filter_session->base.hw_session.dev_handle = hal->dev_handle;
    filter_session->base.hw_session.base_address =
        hwcfg->devices[dev_handle].kernels[kern_handle].base_address;
    filter_session->base.hw_session.ddr_bank =
        hwcfg->devices[dev_handle].kernels[kern_handle].ddr_bank;

    // Assume it is the first filter plugin for now
    filter_session->filter_plugin = &g_xma_singleton->filtercfg[filter_handle];

    // Allocate the private data
    filter_session->base.plugin_data =
        malloc(g_xma_singleton->filtercfg[filter_handle].plugin_data_size);

    XmaEndpoint *end_pt = malloc(sizeof(XmaEndpoint));
    end_pt->session = &filter_session->base;
    end_pt->dev_id = dev_handle;
    end_pt->format = filter_props->output.format;
    end_pt->bits_per_pixel = filter_props->output.bits_per_pixel;
    end_pt->width = filter_props->output.width;
    end_pt->height = filter_props->output.height;
    filter_session->conn_send_handle =
        xma_connect_alloc(end_pt, XMA_CONNECT_SENDER);

    // TODO: fix to use allocate handle making sure that
    //       we don't connect to ourselves
    filter_session->conn_recv_handle = -1;
/*
    end_pt = malloc(sizeof(XmaEndpoint));
    end_pt->session = &filter_session->base;
    end_pt->dev_id = dev_handle;
    end_pt->format = filter_props->input.format;
    end_pt->bits_per_pixel = filter_props->input.bits_per_pixel;
    end_pt->width = filter_props->input.width;
    end_pt->height = filter_props->input.height;
*/

    // Call the plugins initialization function with this session data
    rc = filter_session->filter_plugin->init(filter_session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Initalization of filter plugin failed. Return code %d\n",
                   rc);
        return NULL;
    }

    return filter_session;
}

int32_t
xma_filter_session_destroy(XmaFilterSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    rc  = session->filter_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Error closing filter plugin\n");

    // Clean up the private data
    free(session->base.plugin_data);

    // Free each sender connection
    xma_connect_free(session->conn_send_handle, XMA_CONNECT_SENDER);

    // Free the receiver connection
    xma_connect_free(session->conn_recv_handle, XMA_CONNECT_RECEIVER);

    /* free kernel/kernel-session */
    rc = xma_res_free_kernel(g_xma_singleton->shm_res_cfg,
                             session->base.kern_res);
    if (rc)
        xma_logmsg(XMA_ERROR_LOG, XMA_FILTER_MOD,
                   "Error freeing filter session. Return code %d\n", rc);

    // Free the session
    // TODO: (should also free the Hw sessions)
    free(session);

    return XMA_SUCCESS;
}

int32_t
xma_filter_session_send_frame(XmaFilterSession  *session,
                              XmaFrame          *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    if (session->conn_send_handle != -1)
    {
        // Get the connection entry to find the receiver
        int32_t c_handle = session->conn_send_handle;
        XmaConnect *conn = &g_xma_singleton->connections[c_handle];
        XmaEndpoint *recv = conn->receiver;
        if (recv)
        {
            if (is_xma_encoder(recv->session))
            {
                XmaEncoderSession *e_ses =
                    to_xma_encoder(recv->session);
                if (!e_ses->encoder_plugin->get_dev_input_paddr)
                    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD,
                        "encoder plugin does not support zero copy\n");
                goto send;
                session->out_dev_addr =
                    e_ses->encoder_plugin->get_dev_input_paddr(e_ses);
                session->zerocopy_dest = true;
            }
        }
    }
send:
    return session->filter_plugin->send_frame(session, frame);
}

int32_t
xma_filter_session_recv_frame(XmaFilterSession  *session,
                              XmaFrame          *frame)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_FILTER_MOD, "%s()\n", __func__);
    return session->filter_plugin->recv_frame(session, frame);
}
