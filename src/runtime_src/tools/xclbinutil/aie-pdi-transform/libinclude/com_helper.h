/******************************************************************************
* Copyright (C) 2019 - 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

#ifndef __COM_HELPER_H__
#define __COM_HELPER_H__

#ifdef _ENABLE_IPU_LX6_
#include "com_io_ipu_lx6.h"
#endif
#ifdef _ENABLE_FREERTOS_
#include <stdbool.h>
#include <FreeRTOS.h>
#include "com_io_generic.h"
#include "task.h"
#include "semphr.h"
#endif /* _ENABLE_IPU_LX6_ */

#ifdef _ENABLE_IPU_LX6_
typedef uint32_t SemaphoreHandle_t;
typedef uint32_t TaskHandle_t;
#endif

enum ipu_resources {
    ERT_QUEUE_TAIL_H2C_DOORBELL = 0,
    ERT_QUEUE_TAIL_C2H_DOORBELL = 0,
    DPU_EVENT,
    AIE_EVENT,
    ADMA,
    IPU_RES_NUM
};

struct event_resources {
    enum ipu_resources res;
    SemaphoreHandle_t sem;
};

struct com_helper_arg {
    TaskHandle_t tHdl;
};

void wait_event(enum ipu_resources res);
int com_usleep(unsigned int usec);
int com_helper_init(struct com_helper_arg *arg);
void com_enable_interrupts(void);

#endif
