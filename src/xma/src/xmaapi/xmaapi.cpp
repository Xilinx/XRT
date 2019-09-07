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

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "app/xmaerror.h"
#include "app/xmalogger.h"
#include "app/xmaparam.h"
#include "lib/xmaapi.h"
//#include "lib/xmahw_hal.h"
#include "lib/xmasignal.h"
#include <iostream>
#include <thread>

#define XMAAPI_MOD "xmaapi"

//Create singleton on the stack
XmaSingleton xma_singleton_internal;

XmaSingleton *g_xma_singleton = &xma_singleton_internal;

void xma_thread1() {
    bool expected = false;
    bool desired = true;
    std::list<XmaLogMsg> list1;
    while (!g_xma_singleton->xma_exit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        while (!g_xma_singleton->log_msg_list_locked.compare_exchange_weak(expected, desired)) {
            expected = false;
        }
        //log msg list lock acquired

        if (!g_xma_singleton->log_msg_list.empty()) {
            auto itr1 = list1.end();
            list1.splice(itr1, g_xma_singleton->log_msg_list);
        }

        //Release log msg list lock
        g_xma_singleton->log_msg_list_locked = false;

        while (!list1.empty()) {
            auto itr1 = list1.begin();
            xclLogMsg(NULL, (xrtLogMsgLevel)itr1->level, "XMA", itr1->msg.c_str());
            list1.pop_front();
        }
    }
    //Print all stats here
    //Sarab: TODO
    xclLogMsg(NULL, XMA_INFO_LOG, "XMA", "CU Usage Stats: ");

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

int32_t xma_initialize(XmaXclbinParameter *devXclbins, int32_t num_parms)
{
    int32_t ret;
    //bool    rc;

    if (g_xma_singleton == NULL) {
        std::cout << "XMA FATAL: Singleton is NULL" << std::endl;
        return XMA_ERROR;
    }
    if (num_parms < 1) {
        std::cout << "XMA FATAL: Must provide atleast one XmaXclbinParameter." << std::endl;
        return XMA_ERROR;
    }

    //Sarab: TODO initialize all elements of singleton
    bool expected = false;
    bool desired = true;
    while (!(g_xma_singleton->locked).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Singleton lock acquired

    if (g_xma_singleton->xma_initialized) {
        std::cout << "XMA FATAL: XMA is already initialized" << std::endl;

        //Release singleton lock
        g_xma_singleton->locked = false;
        return XMA_ERROR;
    }
    //g_xma_singleton->encoders.reserve(32);
    //g_xma_singleton->encoders.emplace_back(XmaEncoderPlugin{});
    g_xma_singleton->hwcfg.devices.reserve(MAX_XILINX_DEVICES);

    /*Sarab: Remove yaml cfg stuff
    ret = xma_cfg_parse(cfgfile, &g_xma_singleton->systemcfg);
    if (ret != XMA_SUCCESS) {
        printf("XMA ERROR: yaml cfg parsing failed\n");
        return ret;
    }
    */

    /*Sarab: Remove xma_res stuff
    ret = xma_logger_init(&g_xma_singleton->logger);
    if (ret != XMA_SUCCESS) {
        return ret;
        printf("XMA ERROR: logger init failed\n");
    }

    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD,
               "Creating resource shared mem database\n");
    g_xma_singleton->shm_res_cfg = xma_res_shm_map(&g_xma_singleton->systemcfg);

    if (!g_xma_singleton->shm_res_cfg)
        return XMA_ERROR;
    */
   
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Probing hardware\n");
    ret = xma_hw_probe(&g_xma_singleton->hwcfg);
    if (ret != XMA_SUCCESS) {
        //Release singleton lock
        g_xma_singleton->locked = false;
        for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
            hw_device.kernels.clear();
        }
        g_xma_singleton->hwcfg.devices.clear();
        g_xma_singleton->hwcfg.num_devices = -1;

        return ret;
    }

    /*Sarab: Remove yaml cfg stuff
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Checking hardware compatibility\n");
    rc = xma_hw_is_compatible(&g_xma_singleton->hwcfg,
                              &g_xma_singleton->systemcfg);
    if (!rc)
        return XMA_ERROR_INVALID;
    */

    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Configure hardware\n");
    if (!xma_hw_configure(&g_xma_singleton->hwcfg, devXclbins, num_parms)) {
        //Release singleton lock
        g_xma_singleton->locked = false;
        for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
            hw_device.kernels.clear();
        }
        g_xma_singleton->hwcfg.devices.clear();
        g_xma_singleton->hwcfg.num_devices = -1;

        return XMA_ERROR;
    }

    /*Sarab: Move plugin loading to session_create
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Load scaler plugins\n");
    ret = xma_scaler_plugins_load(&g_xma_singleton->systemcfg,
                                  g_xma_singleton->scalercfg);

    if (ret != XMA_SUCCESS)
        goto error;

    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Load encoder plugins\n");
    ret = xma_enc_plugins_load(&g_xma_singleton->systemcfg,
                               g_xma_singleton->encodercfg);

    if (ret != XMA_SUCCESS)
        goto error;

    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Load decoder plugins\n");
    ret = xma_dec_plugins_load(&g_xma_singleton->systemcfg,
                               g_xma_singleton->decodercfg);

    if (ret != XMA_SUCCESS)
        goto error;

    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Load filter plugins\n");
    ret = xma_filter_plugins_load(&g_xma_singleton->systemcfg,
                                 g_xma_singleton->filtercfg);

    if (ret != XMA_SUCCESS)
        goto error;

    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Load kernel plugins\n");
    ret = xma_kernel_plugins_load(&g_xma_singleton->systemcfg,
                                 g_xma_singleton->kernelcfg);

    if (ret != XMA_SUCCESS)
        goto error;
    */

    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Init signal and exit handlers\n");
    ret = std::atexit(xma_exit);
    if (ret) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Error initalizing XMA\n");
        //Sarab: Remove xmares stuff
        //xma_res_shm_unmap(g_xma_singleton->shm_res_cfg);

        //Release singleton lock
        g_xma_singleton->locked = false;
        for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
            hw_device.kernels.clear();
        }
        g_xma_singleton->hwcfg.devices.clear();
        g_xma_singleton->hwcfg.num_devices = -1;

        return XMA_ERROR;
    }

    //std::thread threadObjSystem(xma_thread1);
    g_xma_singleton->xma_thread1 = std::thread(xma_thread1);
    //Detach threads to let them run independently
    //threadObjSystem.detach();
    g_xma_singleton->xma_thread1.detach();

    xma_init_sighandlers();
    //xma_res_mark_xma_ready(g_xma_singleton->shm_res_cfg);

    g_xma_singleton->locked = false;
    g_xma_singleton->xma_initialized = true;
    return XMA_SUCCESS;
}

void xma_exit(void)
{
    if (g_xma_singleton) {
        g_xma_singleton->xma_exit = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
