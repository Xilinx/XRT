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
//#include "lib/xmacfg.h"
//#include "lib/xmaconnect.h"
#include "lib/xmahw_lib.h"
//#include "lib/xmares.h"
#include "lib/xmalogger.h"
#include <atomic>
#include <vector>

typedef struct XmaSingleton
{
    //XmaSystemCfg      systemcfg;
    XmaHwCfg          hwcfg;
    //XmaConnect        connections[MAX_CONNECTION_ENTRIES];
    //Sarab: Remove logger stuff
    //XmaLogger         logger;
    XmaDecoderPlugin  decodercfg[MAX_PLUGINS];
    XmaEncoderPlugin  encodercfg[MAX_PLUGINS];
    XmaScalerPlugin   scalercfg[MAX_PLUGINS];
    XmaFilterPlugin   filtercfg[MAX_PLUGINS];
    XmaKernelPlugin   kernelcfg[MAX_PLUGINS];
    std::atomic<bool> locked;
    std::vector<XmaDecoderPlugin> decoders;
    std::vector<XmaEncoderPlugin> encoders;
    std::vector<XmaScalerPlugin> scalers;
    std::vector<XmaFilterPlugin> filters;
    std::vector<XmaKernelPlugin> kernels;
    //XmaResources      shm_res_cfg;
    //bool              shm_freed;
    uint32_t          reserved[4];

  XmaSingleton() {
    locked = false;
  }
} XmaSingleton;

#ifdef __cplusplus
extern "C" {
#endif

void xma_exit(void);


/** @} */
#ifdef __cplusplus
}
#endif

#endif
