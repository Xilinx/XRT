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
#include "lib/xmahw_lib.h"
#include "lib/xmalogger.h"
#include <atomic>
#include <list>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <future>
#include <chrono>

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
    XmaHwCfg          hwcfg;
    bool              xma_initialized;
    bool              kds_old;
    uint32_t          cpu_mode;
    std::mutex            m_mutex;
    std::atomic<uint32_t> num_decoders;
    std::atomic<uint32_t> num_encoders;
    std::atomic<uint32_t> num_scalers;
    std::atomic<uint32_t> num_filters;
    std::atomic<uint32_t> num_kernels;
    std::atomic<uint32_t> num_admins;
    std::atomic<uint32_t> num_of_sessions;
    std::vector<XmaSession> all_sessions_vec;// XMASessions
    std::list<XmaLogMsg>   log_msg_list;
    std::atomic<bool> log_msg_list_locked;
    std::atomic<uint32_t> num_execbos;

    std::atomic<bool> xma_exit;
    std::thread       xma_thread1;
    std::vector<std::thread> all_thread2;
    std::future<bool> thread1_future;
    std::vector<std::future<bool>> all_thread2_futures;

    uint32_t          reserved[4];

  XmaSingleton() {
    xma_initialized = false;
    kds_old = false;
    num_decoders = 0;
    num_encoders = 0;
    num_scalers = 0;
    num_filters = 0;
    num_kernels = 0;
    num_admins = 0;
    num_execbos = XMA_NUM_EXECBO_DEFAULT;
    num_of_sessions = 0;
    log_msg_list_locked = false;
    xma_exit = false;
    cpu_mode = 0;
  }

  ~XmaSingleton() {
    xma_exit = true;
    if (!xma_initialized) {
      return;
    }
    try {
      if (thread1_future.valid())
        thread1_future.wait_for(std::chrono::milliseconds(400));
    } catch (...) {}
    try {
      for (const auto& thread2_f: all_thread2_futures) {
        if (thread2_f.valid())
          thread2_f.wait_for(std::chrono::milliseconds(400));
      }
    } catch (...) {}
  }
} XmaSingleton;

#ifdef __cplusplus
extern "C" {
#endif


/** @} */
#ifdef __cplusplus
}
#endif

#endif
