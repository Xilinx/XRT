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

#if !defined (PATH_MAX) || !defined (NAME_MAX)
#include <linux/limits.h>
#endif

#define XMA_MAX_LOGMSG_SIZE          255
#define XMA_MAX_LOGMSG_Q_ENTRIES     128

#ifdef __cplusplus
extern "C" {
#endif

/* Callback function for an XmaThread */
typedef void* (*XmaThreadFunc)(void *data);

/* Data structure for an XmaThread */
typedef struct XmaThread
{
    pthread_t           tid;
    XmaThreadFunc       thread_func;
    void               *data;
    bool                is_running;
} XmaThread;

/* XmaThread APIs */
XmaThread *xma_thread_create(XmaThreadFunc func, void *data);
void xma_thread_destroy(XmaThread *thread);
void xma_thread_start(XmaThread *thread);
bool xma_thread_is_running(XmaThread *thread);
void xma_thread_join(XmaThread *thread);

/* Data structure for XmaMsgQ */
typedef struct XmaMsgQ
{
    uint8_t     *msg_array;
    size_t       msg_size;
    size_t       max_msg_entries;
    int32_t      num_entries;
    int32_t      front;
    int32_t      back;
} XmaMsgQ;

/* XmaMsgQ APIs */
#define XMA_MSGQ_FULL          -1
#define XMA_MSGQ_MSG_TOO_LARGE -2
#define XMA_MSGQ_MSG_TOO_SMALL -3
#define XMA_MSGQ_EMPTY         -4

XmaMsgQ *xma_msgq_create(size_t msg_size, size_t max_msg_entries);
void xma_msgq_destroy(XmaMsgQ *msgq);
bool xma_msgq_isfull(XmaMsgQ *msgq);
bool xma_msgq_isempty(XmaMsgQ *msgq);
int32_t xma_msgq_enqueue(XmaMsgQ *msgq, void *msg, size_t size);
int32_t xma_msgq_dequeue(XmaMsgQ *msgq, void *msg, size_t size);

struct XmaActor;

/* Data structure for XmaActor */
typedef struct XmaActor
{
    XmaThread          *thread;
    XmaMsgQ            *msg_q;
    pthread_mutex_t     lock;
    pthread_cond_t      queued_cond;
    pthread_cond_t      dequeued_cond;
} XmaActor;

/* XmaActor APIs */
XmaActor *xma_actor_create(XmaThreadFunc      func,
                           size_t             msg_size, 
                           size_t             max_msg_entries);
void xma_actor_start(XmaActor *actor);
void xma_actor_destroy(XmaActor *actor);
int xma_actor_sendmsg(XmaActor *actor, void *msg, size_t msg_size);
int xma_actor_recvmsg(XmaActor *actor, void *msg, size_t msg_size);

/* Data structure for XmaLogger */
typedef struct XmaLogger
{
    bool      use_stdout;
    bool      use_fileout;
    bool      use_syslog;
    char      filename[PATH_MAX];
    int32_t   fd;
    int32_t   log_level;
    XmaActor *actor;
} XmaLogger;

int32_t xma_logger_init(XmaLogger *logger);
int32_t xma_logger_close(XmaLogger *logger);

/** @} */
#ifdef __cplusplus
}
#endif

#endif
