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
#ifndef _XMA_API_
#define _XMA_API_

#include "xmaplugin.h"
#include "lib/xmacfg.h"
#include "lib/xmaconnect.h"
#include "lib/xmahw.h"
#include "lib/xmares.h"
#include "lib/xmalogger.h"

typedef struct XmaSingleton
{
    XmaSystemCfg      systemcfg;
    XmaHwCfg          hwcfg;
    XmaConnect        connections[MAX_CONNECTION_ENTRIES];
    XmaLogger         logger;
    XmaDecoderPlugin  decodercfg[MAX_PLUGINS];
    XmaEncoderPlugin  encodercfg[MAX_PLUGINS];
    XmaScalerPlugin   scalercfg[MAX_PLUGINS];
    XmaFilterPlugin   filtercfg[MAX_PLUGINS];
    XmaKernelPlugin   kernelcfg[MAX_PLUGINS];
    XmaResources      shm_res_cfg;
    bool              shm_freed;
} XmaSingleton;

void xma_exit(void);

/**
 */
int32_t xma_scaler_plugins_load(XmaSystemCfg      *systemcfg,
                                XmaScalerPlugin   *scalers);

/**
 */
int32_t xma_enc_plugins_load(XmaSystemCfg      *systemcfg,
                             XmaEncoderPlugin  *encoders);
/**
 */
int32_t xma_dec_plugins_load(XmaSystemCfg      *systemcfg,
                             XmaDecoderPlugin  *decoders);

/**
 */
int32_t xma_filter_plugins_load(XmaSystemCfg      *systemcfg,
                             XmaFilterPlugin      *filters);
/**
 */
int32_t xma_kernel_plugins_load(XmaSystemCfg      *systemcfg,
                                XmaKernelPlugin   *kernels);
#endif
