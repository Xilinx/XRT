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
#ifndef _XMA_LOGGER_LIB_H_
#define _XMA_LOGGER_LIB_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include "xrt.h"


#if !defined (PATH_MAX) || !defined (NAME_MAX)
#include <linux/limits.h>
#endif

#define XMA_MAX_LOGMSG_SIZE          512
//#define XMA_MAX_LOGMSG_Q_ENTRIES     128

/*
typedef enum xrtLogMsgLevel XmaLogLevelType;
#define XMA_CRITICAL_LOG XRT_CRITICAL
#define XMA_ERROR_LOG XRT_ERROR
#define XMA_WARNING_LOG XRT_WARNING
#define XMA_INFO_LOG XRT_INFO
#define XMA_DEBUG_LOG XRT_DEBUG
*/

//struct XmaActor;

/* Data structure for XmaActor *--/
typedef struct XmaActor
{
    XmaThread          *thread;

    std::unique_ptr<std::mutex> logger_queue_mutex;//Using only for waiting for queue to be not empty
    std::unique_ptr<std::condition_variable> logger_queue_cv;
    std::unique_ptr<std::atomic<bool>> logger_queue_locked;
    std::unique_ptr<std::queue<std::string>> logger_queue;

  XmaActor(): logger_queue_mutex(new std::mutex), logger_queue_cv(new std::condition_variable), 
		logger_queue_locked(new std::atomic<bool>), logger_queue(new std::queue<std::string>) {
    *logger_queue_locked = false;
    thread = NULL;
  }
} XmaActor;
*/


#endif
