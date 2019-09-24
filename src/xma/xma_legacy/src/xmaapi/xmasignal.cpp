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

#include <sys/types.h>

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "lib/xmaapi.h"
#include "lib/xmasignal.h"

/**
 *
*/
void xma_signal_hdlr(int signum)
{
    pid_t proc_id = getpid();

    switch(signum) {
        case SIGHUP:
        case SIGINT:
        case SIGABRT:
        case SIGTERM:
        case SIGQUIT:
        case SIGFPE:
        case SIGSEGV:
        case SIGBUS:
            xma_exit();
            signal(signum, SIG_DFL);
            kill(proc_id, signum);
    }
}


void xma_init_sighandlers(void)
{
    signal(SIGHUP, xma_signal_hdlr);
    signal(SIGINT, xma_signal_hdlr);
    signal(SIGABRT, xma_signal_hdlr);
    signal(SIGTERM, xma_signal_hdlr);
    signal(SIGQUIT, xma_signal_hdlr);
    signal(SIGFPE, xma_signal_hdlr);
    signal(SIGSEGV, xma_signal_hdlr);
    signal(SIGBUS, xma_signal_hdlr);
}
