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

#define XMA_SCALER_MOD "xmascaler"

extern XmaSingleton *g_xma_singleton;

static int16_t fixed_coeff_taps12[64][12] =
{
    {48, 143, 307, 504, 667, 730, 669, 507, 310, 145, 49, 18, },
    {47, 141, 304, 501, 665, 730, 670, 510, 313, 147, 50, 18, },
    {46, 138, 301, 498, 663, 730, 672, 513, 316, 149, 51, 18, },
    {45, 136, 298, 495, 661, 730, 674, 516, 319, 151, 52, 18, },
    {44, 134, 295, 492, 659, 730, 676, 519, 322, 153, 53, 18, },
    {44, 132, 292, 489, 657, 730, 677, 522, 325, 155, 54, 18, },
    {43, 130, 289, 486, 655, 729, 679, 525, 328, 157, 55, 19, },
    {42, 129, 287, 483, 653, 729, 681, 528, 331, 160, 56, 19, },
    {41, 127, 284, 480, 651, 729, 683, 531, 334, 162, 57, 19, },
    {40, 125, 281, 477, 648, 729, 684, 534, 337, 164, 58, 19, },
    {40, 123, 278, 474, 646, 728, 686, 537, 340, 166, 59, 20, },
    {39, 121, 275, 471, 644, 728, 687, 539, 343, 169, 60, 20, },
    {38, 119, 272, 468, 642, 727, 689, 542, 346, 171, 61, 20, },
    {37, 117, 269, 465, 640, 727, 690, 545, 349, 173, 62, 20, },
    {37, 115, 266, 461, 638, 727, 692, 548, 353, 175, 63, 21, },
    {36, 114, 264, 458, 635, 726, 693, 551, 356, 178, 65, 21, },
    {35, 112, 261, 455, 633, 726, 695, 554, 359, 180, 66, 21, },
    {35, 110, 258, 452, 631, 725, 696, 556, 362, 183, 67, 21, },
    {34, 108, 255, 449, 628, 724, 698, 559, 365, 185, 68, 22, },
    {33, 107, 252, 446, 626, 724, 699, 562, 368, 187, 69, 22, },
    {33, 105, 250, 443, 624, 723, 700, 565, 371, 190, 71, 22, },
    {32, 103, 247, 440, 621, 723, 702, 567, 374, 192, 72, 23, },
    {32, 101, 244, 437, 619, 722, 703, 570, 377, 195, 73, 23, },
    {31, 100, 241, 433, 617, 721, 704, 573, 380, 197, 75, 23, },
    {31, 98, 239, 430, 614, 720, 705, 576, 383, 200, 76, 24, },
    {30, 97, 236, 427, 612, 720, 707, 578, 387, 202, 77, 24, },
    {29, 95, 233, 424, 609, 719, 708, 581, 390, 205, 79, 24, },
    {29, 93, 231, 421, 607, 718, 709, 584, 393, 207, 80, 25, },
    {28, 92, 228, 418, 604, 717, 710, 586, 396, 210, 81, 25, },
    {28, 90, 225, 415, 602, 716, 711, 589, 399, 212, 83, 26, },
    {27, 89, 223, 412, 599, 715, 712, 591, 402, 215, 84, 26, },
    {27, 87, 220, 408, 597, 714, 713, 594, 405, 217, 86, 27, },
    {27, 86, 217, 405, 594, 713, 714, 597, 408, 220, 87, 27, },
    {26, 84, 215, 402, 591, 712, 715, 599, 412, 223, 89, 27, },
    {26, 83, 212, 399, 589, 711, 716, 602, 415, 225, 90, 28, },
    {25, 81, 210, 396, 586, 710, 717, 604, 418, 228, 92, 28, },
    {25, 80, 207, 393, 584, 709, 718, 607, 421, 231, 93, 29, },
    {24, 79, 205, 390, 581, 708, 719, 609, 424, 233, 95, 29, },
    {24, 77, 202, 387, 578, 707, 720, 612, 427, 236, 97, 30, },
    {24, 76, 200, 383, 576, 705, 720, 614, 430, 239, 98, 31, },
    {23, 75, 197, 380, 573, 704, 721, 617, 433, 241, 100, 31, },
    {23, 73, 195, 377, 570, 703, 722, 619, 437, 244, 101, 32, },
    {23, 72, 192, 374, 567, 702, 723, 621, 440, 247, 103, 32, },
    {22, 71, 190, 371, 565, 700, 723, 624, 443, 250, 105, 33, },
    {22, 69, 187, 368, 562, 699, 724, 626, 446, 252, 107, 33, },
    {22, 68, 185, 365, 559, 698, 724, 628, 449, 255, 108, 34, },
    {21, 67, 183, 362, 556, 696, 725, 631, 452, 258, 110, 35, },
    {21, 66, 180, 359, 554, 695, 726, 633, 455, 261, 112, 35, },
    {21, 65, 178, 356, 551, 693, 726, 635, 458, 264, 114, 36, },
    {21, 63, 175, 353, 548, 692, 727, 638, 461, 266, 115, 37, },
    {20, 62, 173, 349, 545, 690, 727, 640, 465, 269, 117, 37, },
    {20, 61, 171, 346, 542, 689, 727, 642, 468, 272, 119, 38, },
    {20, 60, 169, 343, 539, 687, 728, 644, 471, 275, 121, 39, },
    {20, 59, 166, 340, 537, 686, 728, 646, 474, 278, 123, 40, },
    {19, 58, 164, 337, 534, 684, 729, 648, 477, 281, 125, 40, },
    {19, 57, 162, 334, 531, 683, 729, 651, 480, 284, 127, 41, },
    {19, 56, 160, 331, 528, 681, 729, 653, 483, 287, 129, 42, },
    {19, 55, 157, 328, 525, 679, 729, 655, 486, 289, 130, 43, },
    {18, 54, 155, 325, 522, 677, 730, 657, 489, 292, 132, 44, },
    {18, 53, 153, 322, 519, 676, 730, 659, 492, 295, 134, 44, },
    {18, 52, 151, 319, 516, 674, 730, 661, 495, 298, 136, 45, },
    {18, 51, 149, 316, 513, 672, 730, 663, 498, 301, 138, 46, },
    {18, 50, 147, 313, 510, 670, 730, 665, 501, 304, 141, 47, },
    {18, 49, 145, 310, 507, 669, 730, 667, 504, 307, 143, 48, },
};

static void copy_coeffecients(int16_t coeff[64][12])
{
    int32_t i, j;

    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    for (i = 0; i < 64; i++)
    {
        for (j = 0; j < 12; j++)
        {
            coeff[i][j] = fixed_coeff_taps12[i][j];
        }
    }
}

int32_t
xma_scaler_plugins_load(XmaSystemCfg      *systemcfg,
                       XmaScalerPlugin    *scalers)
{
    // Get the plugin path
    char *pluginpath = systemcfg->pluginpath;
    char *error;
    int32_t k = 0;

    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
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
            if (strcmp(func, XMA_CFG_FUNC_NM_SCALE) != 0)
                continue;
            char *plugin = systemcfg->imagecfg[i].kernelcfg[j].plugin;
            char pluginfullname[PATH_MAX + NAME_MAX];
            sprintf(pluginfullname, "%s/%s", pluginpath, plugin);
            void *handle = dlopen(pluginfullname, RTLD_NOW);
            if (!handle)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }

            XmaScalerPlugin *plg =
                (XmaScalerPlugin*)dlsym(handle, "scaler_plugin");
            if ((error = dlerror()) != NULL)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }
            memcpy(&scalers[k++], plg, sizeof(XmaScalerPlugin));
        }
    }
    return XMA_SUCCESS;
}

void xma_scaler_default_filter_coeff_set(XmaScalerFilterProperties *props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    copy_coeffecients(props->h_coeff0);
    copy_coeffecients(props->h_coeff1);
    copy_coeffecients(props->h_coeff2);
    copy_coeffecients(props->h_coeff3);
    copy_coeffecients(props->v_coeff0);
    copy_coeffecients(props->v_coeff1);
    copy_coeffecients(props->v_coeff2);
    copy_coeffecients(props->v_coeff3);
}

XmaScalerSession*
xma_scaler_session_create(XmaScalerProperties *sc_props)
{
    XmaScalerSession *sc_session = (XmaScalerSession*) malloc(sizeof(XmaScalerSession));
	XmaResources xma_shm_cfg = g_xma_singleton->shm_res_cfg;
    XmaKernelRes kern_res;
	int rc, dev_handle, kern_handle, scal_handle, i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
	if (!xma_shm_cfg) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "No reference to xma res database\n");
        free(sc_session);
		return NULL;
    }

    memset(sc_session, 0, sizeof(XmaScalerSession));
    // init session data
    sc_session->props = *sc_props;
    sc_session->base.chan_id = -1;
    sc_session->base.session_type = XMA_SCALER;

    // Just assume this is an ABR scaler for now and that the FPGA
    // has been downloaded.  This is accomplished by getting the
    // first device (dev_handle, base_addr, ddr_bank) and making a
    // XmaHwSession out of it.  Later this needs to be done by searching
    // for an available resource.
	/* JPM TODO default to exclusive access.  Ensure multiple threads
	   can access this device if in-use pid = requesting thread pid */
    rc = xma_res_alloc_scal_kernel(xma_shm_cfg, sc_props->hwscaler_type,
                                   sc_props->hwvendor_string,
	                               &sc_session->base, false);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Failed to allocate free scaler kernel. Return code %d\n", rc);
        return NULL;
    }

    kern_res = sc_session->base.kern_res;

    dev_handle = xma_res_dev_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_SCALER_MOD,"dev_handle = %d\n", dev_handle);
    if (dev_handle < 0)
        return NULL;

    kern_handle = xma_res_kern_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_SCALER_MOD,"kern_handle = %d\n", kern_handle);
    if (kern_handle < 0)
        return NULL;

    scal_handle = xma_res_plugin_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_SCALER_MOD,"scal_handle = %d\n", scal_handle);
    if (scal_handle < 0)
        return NULL;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    XmaHwHAL *hal = (XmaHwHAL*)hwcfg->devices[dev_handle].handle;
    sc_session->base.hw_session.dev_handle = hal->dev_handle;

    //For execbo:
    sc_session->base.hw_session.kernel_info = &hwcfg->devices[dev_handle].kernels[kern_handle];

    sc_session->base.hw_session.dev_index = hal->dev_index;

    // Assume it is the first scaler plugin for now
    sc_session->scaler_plugin = &g_xma_singleton->scalercfg[scal_handle];

    // Allocate the private data
    sc_session->base.plugin_data =
        calloc(g_xma_singleton->scalercfg[scal_handle].plugin_data_size, sizeof(uint8_t));

    // Allocate a connection for each output
    for (i = 0; i < sc_props->num_outputs; i++)
    {
        XmaEndpoint *end_pt = (XmaEndpoint*) malloc(sizeof(XmaEndpoint));
        end_pt->session = &sc_session->base;
        end_pt->dev_id = dev_handle;
        end_pt->format = sc_props->output[i].format;
        end_pt->bits_per_pixel = sc_props->output[i].bits_per_pixel;
        end_pt->width = sc_props->output[i].width;
        end_pt->height = sc_props->output[i].height;
        sc_session->conn_send_handles[i] =
            xma_connect_alloc(end_pt, XMA_CONNECT_SENDER);
    }

    // TODO: fix to use allocate handle making sure that
    //       we don't connect to ourselves
    sc_session->conn_recv_handle = -1;
#if 0
    // Allocate a connection for the input
    XmaEndpoint *end_pt = malloc(sizeof(XmaEndpoint));
    end_pt->session = &sc_session->base;
    end_pt->dev_id = dev_handle;
    end_pt->format = sc_props->input.format;
    end_pt->bits_per_pixel = sc_props->input.bits_per_pixel;
    end_pt->width = sc_props->input.width;
    end_pt->height = sc_props->input.height;
    sc_session->conn_recv_handle =
        xma_connect_alloc(end_pt, XMA_CONNECT_RECEIVER);
#endif

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = sc_session->scaler_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    //Sarab: Stop here for now
    //Sarab: Remove it later on
    return NULL;

    rc = sc_session->scaler_plugin->init(sc_session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Initalization of scaler plugin failed. Return code %d\n",
                   rc);
        return NULL;
    }

    return sc_session;
}

int32_t
xma_scaler_session_destroy(XmaScalerSession *session)
{
    int32_t rc, i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    rc  = session->scaler_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Error closing scaler plugin. Return code %d\n", rc);

    // Clean up the private data
    free(session->base.plugin_data);

    // Free each sender connection
    for (i = 0; i < session->props.num_outputs; i++)
        xma_connect_free(session->conn_send_handles[i], XMA_CONNECT_SENDER);

#if 0
    // Free the receiver connection
    xma_connect_free(session->conn_recv_handle, XMA_CONNECT_RECEIVER);
#endif

    /* free kernel/kernel-session */
    rc = xma_res_free_kernel(g_xma_singleton->shm_res_cfg,
                             session->base.kern_res);
    if (rc)
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Error freeing kernel session. Return code %d\n", rc);

    // Free the session
    // TODO: (should also free the Hw sessions)
    free(session);

    return XMA_SUCCESS;
}

int32_t
xma_scaler_session_send_frame(XmaScalerSession  *session,
                              XmaFrame          *frame)
{
    //int32_t i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    /*Sarab: Remove zerocopy stuff
    for (i = 0; i < session->props.num_outputs; i++)
    {
        if (session->conn_send_handles[i] != -1)
        {
            // Get the connection entry to find the receiver
            int32_t c_handle = session->conn_send_handles[i];
            XmaConnect *conn = &g_xma_singleton->connections[c_handle];
            XmaEndpoint *recv = conn->receiver;
            if (recv)
            {
                if (is_xma_encoder(recv->session))
                {
                    XmaEncoderSession *e_ses =
                        to_xma_encoder(recv->session);
                    if (!e_ses->encoder_plugin->get_dev_input_paddr) {
                        xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                            "encoder plugin does not support zero copy\n");
                        continue;
                    }
                    session->out_dev_addrs[i] =
                        e_ses->encoder_plugin->get_dev_input_paddr(e_ses);
                    session->zerocopy_dests[i] = true;
                }
            }
        }
    }
    */

    return session->scaler_plugin->send_frame(session, frame);
}

int32_t
xma_scaler_session_recv_frame_list(XmaScalerSession  *session,
                                   XmaFrame          **frame_list)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    return session->scaler_plugin->recv_frame_list(session,
                                                   frame_list);
}
