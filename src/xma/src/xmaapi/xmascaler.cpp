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
#include <bitset>

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
    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    if (!g_xma_singleton->xma_initialized) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "XMA session creation must be after initialization\n");
        return NULL;
    }
    if (sc_props->plugin_lib == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "ScalerProperties must set plugin_lib\n");
        return NULL;
    }

    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxma2plugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Failed to open plugin xmaplugin.so. Error msg: %s\n",
                   dlerror());
        return NULL;
    }
    void *handle = dlopen(sc_props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            sc_props->plugin_lib, dlerror());
        return NULL;
    }

    XmaScalerPlugin *plg =
        (XmaScalerPlugin*)dlsym(handle, "scaler_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
            "Failed to get scaler_plugin from %s\n Error msg: %s\n",
            sc_props->plugin_lib, dlerror());
        return NULL;
    }
    if (plg->xma_version == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "ScalerPlugin library must have xma_version function\n");
        return NULL;
    }

    XmaScalerSession *sc_session = (XmaScalerSession*) malloc(sizeof(XmaScalerSession));
    if (sc_session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
            "Failed to allocate memory for scalerSession\n");
        return NULL;
    }
    memset(sc_session, 0, sizeof(XmaScalerSession));
    // init session data
    sc_session->props = *sc_props;
    sc_session->base.channel_id = sc_props->channel_id;
    sc_session->base.session_type = XMA_SCALER;
    sc_session->base.stats = NULL;
    sc_session->private_session_data = NULL;//Managed by host video application
    sc_session->private_session_data_size = -1;//Managed by host video application

    sc_session->scaler_plugin = plg;

    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

    int32_t rc, dev_index, cu_index;
    dev_index = sc_props->dev_index;
    cu_index = sc_props->cu_index;
    //enc_handle = enc_props->cu_index;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices || dev_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "XMA session creation failed. dev_index not found\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(sc_session);
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
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "XMA session creation failed. dev_index not loaded with xclbin\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(sc_session);
        return NULL;
    }
    if ((cu_index > 0 && (uint32_t)cu_index >= hwcfg->devices[hwcfg_dev_index].number_of_cus) || (cu_index < 0 && sc_props->cu_name == NULL)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "XMA session creation failed. Invalid cu_index = %d\n", cu_index);
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(sc_session);
        return NULL;
    }
    if (cu_index < 0) {
        std::string cu_name = std::string(sc_props->cu_name);
        found = false;
        for (XmaHwKernel& kernel: g_xma_singleton->hwcfg.devices[hwcfg_dev_index].kernels) {
            if (std::string((char*)kernel.name) == cu_name) {
                found = true;
                cu_index = kernel.cu_index;
                break;
            }
        }
        if (!found) {
            xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                    "XMA session creation failed. cu %s not found\n", cu_name.c_str());
            //Release singleton lock
            g_xma_singleton->locked = false;
            free(sc_session);
            return NULL;
        }
    }

    if (hwcfg->devices[hwcfg_dev_index].kernels[cu_index].in_use) {
        xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                   "XMA session sharing CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    } else {
        xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                   "XMA session with CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    }

    void* dev_handle = hwcfg->devices[hwcfg_dev_index].handle;
    XmaHwKernel* kernel_info = &hwcfg->devices[hwcfg_dev_index].kernels[cu_index];
    sc_session->base.hw_session.dev_index = hwcfg->devices[hwcfg_dev_index].dev_index;

    //Allow user selected default ddr bank per XMA session
    if (sc_props->ddr_bank_index < 0) {
        if (hwcfg->devices[hwcfg_dev_index].kernels[cu_index].soft_kernel) {
            //Only allow ddr_bank == 0;
            sc_session->base.hw_session.bank_index = 0;
            xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                "XMA session with soft_kernel default ddr_bank: %d\n", sc_session->base.hw_session.bank_index);
        } else {
            sc_session->base.hw_session.bank_index = kernel_info->default_ddr_bank;
            xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                "XMA session default ddr_bank: %d\n", sc_session->base.hw_session.bank_index);
        }
    } else {
        if (hwcfg->devices[hwcfg_dev_index].kernels[cu_index].soft_kernel) {
            if (sc_props->ddr_bank_index != 0) {
                xma_logmsg(XMA_WARNING_LOG, XMA_SCALER_MOD,
                    "XMA session with soft_kernel only allows ddr bank of zero\n");
            }
            //Only allow ddr_bank == 0;
            sc_session->base.hw_session.bank_index = 0;
            xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                "XMA session with soft_kernel default ddr_bank: %d\n", sc_session->base.hw_session.bank_index);
        } else {
            std::bitset<MAX_DDR_MAP> tmp_bset;
            tmp_bset = kernel_info->ip_ddr_mapping;
            if (tmp_bset[sc_props->ddr_bank_index]) {
                sc_session->base.hw_session.bank_index = sc_props->ddr_bank_index;
                xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                    "Using user supplied default ddr_bank. XMA session default ddr_bank: %d\n", sc_session->base.hw_session.bank_index);
            } else {
                xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                    "User supplied default ddr_bank is invalid. Valid ddr_bank mapping for this CU: %s\n", tmp_bset.to_string().c_str());
                
                //Release singleton lock
                g_xma_singleton->locked = false;
                free(sc_session);
                return NULL;
            }
        }
    }

    if (kernel_info->kernel_channels) {
        if (sc_session->base.channel_id > (int32_t)kernel_info->max_channel_id) {
            xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                "Selected dataflow CU with channels has ini setting with max channel_id of %d. Cannot create session with higher channel_id of %d\n", kernel_info->max_channel_id, sc_session->base.channel_id);
            
            //Release singleton lock
            g_xma_singleton->locked = false;
            free(sc_session);
            return NULL;
        }
    }

    // Call the plugins initialization function with this session data
    //Sarab: Check plugin compatibility to XMA
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = sc_session->scaler_plugin->xma_version(&xma_main_ver, & xma_sub_ver);
    if ((xma_main_ver == 2019 && xma_sub_ver < 2) || xma_main_ver < 2019 || rc < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Initalization of plugin failed. Plugin is incompatible with this XMA version\n");
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(sc_session);
        return NULL;
    }

    // Allocate the private data
    sc_session->base.plugin_data =
        calloc(sc_session->scaler_plugin->plugin_data_size, sizeof(uint8_t));

    sc_session->base.session_id = g_xma_singleton->num_of_sessions + 1;
    /*
    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD,
                "XMA session signature is: 0x%04llx", sc_session->base.session_signature);
    */
    xma_logmsg(XMA_INFO_LOG, XMA_SCALER_MOD,
                "XMA session channel_id: %d; session_id: %d", sc_session->base.channel_id, sc_session->base.session_id);

    XmaHwSessionPrivate *priv1 = new XmaHwSessionPrivate();
    priv1->dev_handle = dev_handle;
    priv1->kernel_info = kernel_info;
    priv1->kernel_complete_count = 0;
    priv1->device = &hwcfg->devices[hwcfg_dev_index];
    sc_session->base.session_signature = (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved));

    sc_session->base.hw_session.private_do_not_use = (void*) priv1;

    rc = sc_session->scaler_plugin->init(sc_session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Initalization of plugin failed. Return code %d\n",
                   rc);
        //Release singleton lock
        g_xma_singleton->locked = false;
        free(sc_session->base.plugin_data);
        free(sc_session);
        delete priv1;
        return NULL;
    }
    kernel_info->in_use = true;
    g_xma_singleton->num_scalers++;
    g_xma_singleton->num_of_sessions = sc_session->base.session_id;

    g_xma_singleton->all_sessions.emplace(g_xma_singleton->num_of_sessions, sc_session->base);

    //Release singleton lock
    g_xma_singleton->locked = false;

    return sc_session;
}

int32_t
xma_scaler_session_destroy(XmaScalerSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Session is already released\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    if (session->base.hw_session.private_do_not_use == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Session is corrupted\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    if (session->scaler_plugin == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Session is corrupted\n");

        //Release singleton lock
        g_xma_singleton->locked = false;

        return XMA_ERROR;
    }
    rc  = session->scaler_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "Error closing scaler plugin. Return code %d\n", rc);

    // Clean up the private data
    free(session->base.plugin_data);

    // Free the session
    /*
    delete (XmaHwSessionPrivate*)session->base.hw_session.private_do_not_use;
    */
    session->base.hw_session.private_do_not_use = NULL;
    session->base.plugin_data = NULL;
    session->base.stats = NULL;
    session->scaler_plugin = NULL;
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
xma_scaler_session_send_frame(XmaScalerSession  *session,
                              XmaFrame          *frame)
{
    //int32_t i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "xma_scaler_session_send_frame failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD, "xma_scaler_session_send_frame failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }

    return session->scaler_plugin->send_frame(session, frame);
}

int32_t
xma_scaler_session_recv_frame_list(XmaScalerSession  *session,
                                   XmaFrame          **frame_list)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_SCALER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD,
                   "xma_scaler_session_recv_frame_list failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD, "xma_scaler_session_recv_frame_list failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_SCALER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    return session->scaler_plugin->recv_frame_list(session,
                                                   frame_list);
}
