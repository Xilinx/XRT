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
#include <list>
#include <unordered_map>

typedef struct XmaLogMsg
{
    XmaLogLevelType level;
    std::string msg;

  XmaLogMsg() {
    level = XMA_DEBUG_LOG;
  }
} XmaLogMsg;

typedef struct XmaSingleton
{
    //XmaSystemCfg      systemcfg;
    XmaHwCfg          hwcfg;
    bool              xma_initialized;
    //XmaConnect        connections[MAX_CONNECTION_ENTRIES];
    //Sarab: Remove logger stuff
    //XmaLogger         logger;
    //XmaDecoderPlugin  decodercfg[MAX_PLUGINS];
    //XmaEncoderPlugin  encodercfg[MAX_PLUGINS];
    //XmaScalerPlugin   scalercfg[MAX_PLUGINS];
    //XmaFilterPlugin   filtercfg[MAX_PLUGINS];
    //XmaKernelPlugin   kernelcfg[MAX_PLUGINS];
    std::atomic<bool> locked;
    std::atomic<uint32_t> num_decoders;
    std::atomic<uint32_t> num_encoders;
    std::atomic<uint32_t> num_scalers;
    std::atomic<uint32_t> num_filters;
    std::atomic<uint32_t> num_kernels;
    std::atomic<uint32_t> num_non_kernels;
    std::atomic<uint32_t> num_of_sessions;
    std::unordered_map<uint32_t, XmaSession> all_sessions;// XMASessions
    std::list<XmaLogMsg>   log_msg_list;
    std::atomic<bool> log_msg_list_locked;

    std::atomic<bool> xma_exit;

    uint32_t          reserved[4];

  XmaSingleton() {
    locked = false;
    xma_initialized = false;
    num_decoders = 0;
    num_encoders = 0;
    num_scalers = 0;
    num_filters = 0;
    num_kernels = 0;
    num_non_kernels = 0;
    num_of_sessions = 0;
    log_msg_list_locked = false;
    xma_exit = false;
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
