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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <syslog.h>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>

#include "lib/xmaapi.h"
#include "app/xmalogger.h"
#include "lib/xmalogger.h"
#include "core/common/config_reader.h"

#ifdef XMA_DEBUG
#define XMA_DBG_PRINTF(format, ...) \
   printf(format, __VA_ARGS__);
#else
#define XMA_DBG_PRINTF(format, ...)
#endif

extern XmaSingleton *g_xma_singleton;


void
xma_logmsg(XmaLogLevelType level, const char *name, const char *msg, ...)
{
    static auto verbosity = xrt_core::config::get_verbosity();
    if (level <= verbosity) {
        /* Handle variable arguments */
        va_list ap;

        /* Create message buffer on the stack */
        char            msg_buff[XMA_MAX_LOGMSG_SIZE];
        char            log_name[40] = {0};
        int32_t         hdr_offset;

        memset(msg_buff, 0, sizeof(msg_buff));

        /* Set component name */
        if (name == NULL)
            strncpy(log_name, "XMA-default", sizeof(log_name));
        else
            strncpy(log_name, name, sizeof(log_name)-1);


        sprintf(msg_buff, "%s %s ", program_invocation_short_name, log_name);
        hdr_offset = strlen(msg_buff);
        va_start(ap, msg);
        vsnprintf(&msg_buff[hdr_offset], (XMA_MAX_LOGMSG_SIZE - hdr_offset), msg, ap);
        va_end(ap);
        if (g_xma_singleton) {
            bool expected = false;
            bool desired = true;
            while (!g_xma_singleton->log_msg_list_locked.compare_exchange_weak(expected, desired)) {
                expected = false;
            }
            //log msg list lock acquired

            g_xma_singleton->log_msg_list.emplace_back(XmaLogMsg{});
            auto& tmp1 = g_xma_singleton->log_msg_list.back();
            tmp1.level = level;
            tmp1.msg = std::string(msg_buff);

            if (level <= XMA_ERROR_LOG) {
                //Flush log msg queue for all error
                //Else application may exit/crash early
                while (!g_xma_singleton->log_msg_list.empty()) {
                    auto itr1 = g_xma_singleton->log_msg_list.begin();
                    xclLogMsg(NULL, (xrtLogMsgLevel)itr1->level, "XMA", itr1->msg.c_str());
                    g_xma_singleton->log_msg_list.pop_front();
                }
            }

            //Release log msg list lock
            g_xma_singleton->log_msg_list_locked = false;
        } else {
            xclLogMsg(NULL, (xrtLogMsgLevel)level, "XMA", msg_buff);
        }
    }
}

