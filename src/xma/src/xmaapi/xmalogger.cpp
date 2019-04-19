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
#include <xclhal2.h>

#ifdef XMA_DEBUG
#define XMA_DBG_PRINTF(format, ...) \
   printf(format, __VA_ARGS__);
#else
#define XMA_DBG_PRINTF(format, ...)
#endif

typedef struct XmaLoggerCbData
{
    XmaLoggerCallback callback;
    XmaLogLevelType   level;
} XmaLoggerCbData;

XmaLoggerCbData *g_xma_loggercb_singleton;

extern XmaSingleton *g_xma_singleton;

typedef struct XmaLogLevel2Str
{
    XmaLogLevelType level;
    const char      *lvl_str;
} XmaLogLevel2Str;

XmaLogLevel2Str g_loglevel_tbl[] = {
    {XMA_CRITICAL_LOG, "CRITICAL"},
    {XMA_ERROR_LOG,    "ERROR   "},
    {XMA_INFO_LOG,     "INFO    "},
    {XMA_DEBUG_LOG,    "DEBUG   "}
};

/* Prototype for the logger actor thread */
//void* xma_logger_actor(void *data);

void xma_logger_callback(XmaLoggerCallback callback, XmaLogLevelType level)
{
    // Allocate singleton if it doesn't exist
    if (g_xma_loggercb_singleton == NULL)
        g_xma_loggercb_singleton = (XmaLoggerCbData*) malloc(sizeof(XmaLoggerCbData));

    g_xma_loggercb_singleton->callback = callback;
    g_xma_loggercb_singleton->level = level;

}

int xma_logger_init(XmaLogger *logger)
{
    /* Verify parameters */
    assert(logger);

    if (!g_xma_singleton->systemcfg.logger_initialized)
    {
        printf("XMA Logger: defaulting to stdout, loglevel INFO\n");
        logger->use_stdout = true;
        logger->use_fileout = false;
        logger->use_syslog = false;
        logger->log_level = XMA_INFO_LOG;
    }
    else if (strcmp(g_xma_singleton->systemcfg.logfile,"syslog") >= 0)
    {
        printf("XMA Logger: using syslog\n");
        logger->use_stdout = false;
        logger->use_fileout = false;
        logger->use_syslog = true;
        strcpy(logger->filename, g_xma_singleton->systemcfg.logfile);
        logger->log_level = g_xma_singleton->systemcfg.loglevel;
        openlog("xma: ", LOG_PID|LOG_CONS, LOG_USER);
    }
    else
    {
        printf("XMA Logger: using configuration file settings\n");
        logger->use_stdout = false;
        logger->use_fileout = true;
        logger->use_syslog = false;
        strcpy(logger->filename, g_xma_singleton->systemcfg.logfile);
        logger->log_level = g_xma_singleton->systemcfg.loglevel;
    }

    /* Save FD of output file */
    if (logger->use_fileout)
    {
        logger->fd = open((const char*)logger->filename,
                          O_APPEND | O_CREAT | O_WRONLY, 00666);
        if (logger->fd == -1)
        {
            perror("XMA Logger open failed: ");
            return -1;
        }
    }
    else
        logger->fd = -1;

    /* Create logger actor */
    /*
    //std::cout << "Sarab: " << __func__ << " , " << std::dec << __LINE__ << std::endl;
    logger->actor = xma_actor_create(xma_logger_actor,
                                     XMA_MAX_LOGMSG_SIZE,
                                     XMA_MAX_LOGMSG_Q_ENTRIES);
    */
    logger->actor = xma_actor_create();

    //std::cout << "Sarab: " << __func__ << " , " << std::dec << __LINE__ << std::endl;
    xma_actor_start(logger->actor);
    //std::cout << "Sarab: " << __func__ << " , " << std::dec << __LINE__ << std::endl;

    return 0;
}

int xma_logger_close(XmaLogger *logger)
{
    /* Verify parameters */
    assert(logger);
    if(logger->use_syslog){
        closelog();
    }
    xma_actor_destroy(logger->actor);

    return 0;
}

void
xma_logmsg(XmaLogLevelType level, const char *name, const char *msg, ...)
{
    /* Handle variable arguments */
    va_list ap;

    /* Create message buffer on the stack */
    char            msg_buff[XMA_MAX_LOGMSG_SIZE];
    struct tm      *tm_info;
    struct timeval  tv;
    int32_t         millisec;
    char            log_time[40] = {0};
    char            log_name[40] = {0};
    const char     *log_level;
    int32_t         hdr_offset;
    bool            send2callback = false;
    bool            send2actor = false;
    char           *buffer;

    /* Get XMA logger */
    XmaLogger *logger = &g_xma_singleton->logger;
    XmaLoggerCbData *cbdata = g_xma_loggercb_singleton;

    memset(msg_buff, 0, sizeof(msg_buff));

    if (cbdata)
    {
        if (level <= cbdata->level)
            send2callback = true;
    }

    if (level <= logger->log_level)
        send2actor = true;

    if (!(send2callback || send2actor))
        return;

    /* Get time */
    gettimeofday(&tv, NULL);
    millisec = lrint(tv.tv_usec/1000.0);
    if (millisec >= 1000)
    {
        millisec -= 1000;
        tv.tv_sec++;
    }
    tm_info = localtime(&tv.tv_sec);
    strftime(log_time, sizeof(log_time), "%Y-%m-%d %H:%M:%S", tm_info);

    /* Set component name */
    if (name == NULL)
        strncpy(log_name, "XMA-default", sizeof(log_name));
    else
        strncpy(log_name, name, sizeof(log_name)-1);

    log_level = g_loglevel_tbl[level].lvl_str;

    /* Format log message */
    //NOTE: Usage of program_invocation_short_name may hinder portability
    if(logger->use_syslog){
        sprintf(msg_buff, "%s %s %s ", program_invocation_short_name, log_level, log_name);
    }
    else{
        sprintf(msg_buff, "%s.%03d %d %s %s %s ", log_time, millisec, getpid(), program_invocation_short_name, log_level, log_name);
    }
    hdr_offset = strlen(msg_buff);
    va_start(ap, msg);
    vsnprintf(&msg_buff[hdr_offset], (XMA_MAX_LOGMSG_SIZE - hdr_offset), msg, ap);
    va_end(ap);

    /* Send message buffer to logger Actor -
       will be copied to loggers message buffer */
    if (send2actor)
        xma_actor_sendmsg(logger->actor, msg_buff, sizeof(msg_buff));

    if (send2callback)
    {
        buffer = (char*) malloc(sizeof(msg_buff));
        strcpy(buffer, msg_buff);
        cbdata->callback(buffer);
    }
}

//void* xma_logger_actor(void *data)
void xma_logger_actor(XmaActor *actor)
{
    int32_t rc;
    char logmsg[XMA_MAX_LOGMSG_SIZE];
    XmaLogger *logger = &g_xma_singleton->logger;
    //XmaActor  *actor = (XmaActor*)data;

    if (!actor)
    {
        printf("XMA ERROR: XmaActor does not exist\n");
        exit(-1);
    }

    char buf[1024] = {0};
    auto len = ::readlink("/proc/self/exe", buf, 1024);
    std::string curr_dir =  std::string(buf, (len>0) ? len : 0);
    std::string ini_file1 = "";
    if (!curr_dir.empty()) {
        if (curr_dir.back() != '/') {
            size_t pos = curr_dir.find_last_of("/");
            if (pos != std::string::npos) {
                ini_file1 = curr_dir.substr(0, pos);
                ini_file1.append("/sdaccel.ini");
            } else {
                ini_file1 = "./sdaccel.ini";
            }
        } else {
            ini_file1 = curr_dir;
            ini_file1.append("sdaccel.ini");
        }
    }

    char* ini_path = std::getenv("SDACCEL_INI_PATH");
    //char* ini_file = getenv("SDACCEL_INI_PATH");
    std::string ini_file2 = "";
    if (ini_path != NULL) {
        ini_file2 = std::string(ini_path);
        if (!ini_file2.empty()) {
            if (ini_file2.find("sdaccel.ini") == std::string::npos) {
                if (ini_file2.back() != '/') {
                    ini_file2.append("/sdaccel.ini");
                } else {
                    ini_file2.append("sdaccel.ini");
                }
            }
        }
    }

    //std::cout << "ERROR: " << __func__ << " , " << std::dec << __LINE__ << std::endl;
    //std::cout << "ERROR: ini_file1: " << ini_file1 << std::endl;
    //std::cout << "ERROR: ini_file2: " << ini_file2 << std::endl;
    bool found_sdaccel_ini_file = false;
    std::ifstream infile;
    if (!ini_file2.empty()) {
        infile.open(ini_file2, std::ios::ate);
        if (infile.is_open()) {
            size_t size = infile.tellg();
            infile.close();
            if (size > 0) {
                found_sdaccel_ini_file = true;
                std::cout << "XMA Logger: Using log destination settings from sdaccel.ini instead of yaml file" << std::endl;
                std::cout << "XMA Logger: sdaccel.ini file: " << ini_file2 << std::endl;
            }
        }
    }
    if (!found_sdaccel_ini_file) {
        if (!ini_file1.empty()) {
            infile.open(ini_file1, std::ios::ate);
            if (infile.is_open()) {
                size_t size = infile.tellg();
                infile.close();
                if (size > 0) {
                    found_sdaccel_ini_file = true;
                    std::cout << "XMA Logger: Using log destination settings from sdaccel.ini instead of yaml file" << std::endl;
                    std::cout << "XMA Logger: sdaccel.ini file: " << ini_file1 << std::endl;
                }
            }
        }
    }




    //std::cout << "ERROR: found ini file: " << std::boolalpha << found_sdaccel_ini_file << std::endl;
    printf("XMA Logger: Logging thread started\n");
    while (1)
    {
        memset(logmsg, 0, sizeof(logmsg));
        rc = xma_actor_recvmsg(actor, logmsg, sizeof(logmsg));
        if (rc == 0)
        {
            if (strncmp(logmsg, "shutdown", 8) == 0)
            {
                printf("XMA logger: received shutdown\n");
                break;
            }

            if (found_sdaccel_ini_file) {
                /* Format log message
                sprintf(msg_buff, "%s.%03d %d %s %s ", log_time, millisec, getpid(), log_level, log_name);
                */
                //rc = xclLogMsg(xclDeviceHandle handle, xclLogMsgLevel level, logmsg);
                xclLogMsg(NULL, xclLogMsgLevel::INFO, "XMA",logmsg);
            } else {
                if (logger->fd != -1)
                {
                    rc = write(logger->fd, logmsg, strlen(logmsg));
                    if (rc < 0)
                    {
                        perror("XMA Logger: could not write to file: ");
                        break;
                    }
                }
                if (logger->use_syslog){
                    uint8_t syslog_level = LOG_DEBUG;
                    switch(logger->log_level){
                        case XMA_CRITICAL_LOG: syslog_level = LOG_CRIT ; break;
                        case XMA_ERROR_LOG   : syslog_level = LOG_ERR  ; break;
                        case XMA_INFO_LOG    : syslog_level = LOG_INFO ; break;
                        case XMA_DEBUG_LOG   : syslog_level = LOG_DEBUG; break;
                    }
                    syslog(syslog_level,"%s", logmsg);
                }
                if (logger->use_stdout)
                    printf("%s", logmsg);
            }
        }
        else
            /* Logger has been shutdown - so return from thread */
            break;
    }
    printf("XMA Logger: shutting down\n");
    if (logger->fd != -1)
        close(logger->fd);

    //return NULL;
}

/* XmaThread APIs */
/*
XmaThread *xma_thread_create(XmaThreadFunc func, void *data)
{
    XmaThread *thread = new XmaThread();
    //XmaThread *thread = (XmaThread*) malloc(sizeof(XmaThread));
    thread->thread_func = func;
    thread->data = data;
    thread->is_running = false;

    return thread;
}


void xma_thread_destroy(XmaThread *thread)
{
    free(thread);
}

void *xma_thread_entry_func(void *data)
{
    XmaThread   *thread = (XmaThread*)data;
    thread->is_running = true;
    thread->thread_func(thread->data);

    return 0;
};

void xma_thread_start(XmaThread *thread)
{
    pthread_create(&thread->tid, NULL, xma_thread_entry_func, thread);
}

bool xma_thread_is_running(XmaThread *thread)
{
    return thread->is_running;
}

void xma_thread_join(XmaThread *thread)
{
    //pthread_join(thread->tid, NULL);
    if (thread->thread_obj.joinable()) {
        thread->thread_obj.join();
    }
}
*/
/* XmaMsgQ APIs *--/
Sarab: Remove this and use C++ std::queue

XmaMsgQ *xma_msgq_create(size_t msg_size, size_t max_msg_entries)
{
    XmaMsgQ *msgq = (XmaMsgQ*) malloc(sizeof(XmaMsgQ));
    msgq->msg_size = msg_size;
    msgq->max_msg_entries = max_msg_entries;
    msgq->msg_array = (uint8_t*) malloc(msg_size * max_msg_entries);
    msgq->num_entries = 0;
    msgq->front = 0;
    msgq->back = 0;

    return msgq;
}

void xma_msgq_destroy(XmaMsgQ *msgq)
{
    free(msgq->msg_array);
    free(msgq);
}

bool xma_msgq_isfull(XmaMsgQ *msgq)
{
    return ((uint32_t)msgq->num_entries == (uint32_t)msgq->max_msg_entries);
}

bool xma_msgq_isempty(XmaMsgQ *msgq)
{
    return (msgq->num_entries == 0);
}

int32_t xma_msgq_enqueue(XmaMsgQ *msgq, void *msg, size_t size)
{
    if (xma_msgq_isfull(msgq))
    {
        XMA_DBG_PRINTF("%s", "XMA msgq enqueue: full\n");
        return XMA_MSGQ_FULL;
    }

    if (size > msgq->msg_size)
    {
        XMA_DBG_PRINTF("%s", "XMA msgq enqueue: too Large\n");
        return XMA_MSGQ_MSG_TOO_LARGE;
    }

    uint8_t *msgdst = msgq->msg_array + (msgq->msg_size * msgq->back);
    memcpy(msgdst, msg, size);

    msgq->back = (msgq->back + 1) % (msgq->max_msg_entries);
    msgq->num_entries++;

    return 0;
}

int32_t xma_msgq_dequeue(XmaMsgQ *msgq, void *msg, size_t size)
{
    if (xma_msgq_isempty(msgq))
    {
        XMA_DBG_PRINTF("%s", "XMA msgq dequeue: empty\n");
        return XMA_MSGQ_EMPTY;
    }

    if (size < msgq->msg_size)
    {
        XMA_DBG_PRINTF("%s", "XMA msgq dequeue: too small\n");
        return XMA_MSGQ_MSG_TOO_SMALL;
    }

    uint8_t *msgsrc = msgq->msg_array + (msgq->msg_size * msgq->front);
    memcpy(msg, msgsrc, msgq->msg_size);

    msgq->front = (msgq->front + 1) % (msgq->max_msg_entries);
    msgq->num_entries--;

    return 0;
}
*/

/* XmaActor APIs */
/*
XmaActor *xma_actor_create(XmaThreadFunc    func,
                           size_t           msg_size,
                           size_t           max_msg_entries)
*/
XmaActor *xma_actor_create()
{
    //std::cout << "Sarab: " << __func__ << " , " << std::dec << __LINE__ << std::endl;
    /*
    XmaActor *actor = (XmaActor*) malloc(sizeof(XmaActor));
    pthread_mutex_init(&actor->lock, NULL);
    pthread_cond_init(&actor->queued_cond, NULL);
    pthread_cond_init(&actor->dequeued_cond, NULL);
    actor->msg_q = xma_msgq_create(msg_size, max_msg_entries);
    */
    XmaActor *actor =  new XmaActor();
    actor->thread = new XmaThread();
    actor->thread->is_running = false;
    
    /*
    std::cout << "Sarab: " << __func__ << " , " << std::dec << __LINE__ << std::endl;
    actor->logger_queue_mutex.reset(new std::mutex());
    exit(0);



    actor->logger_queue_mutex = std::unique_ptr<std::mutex>(new std::mutex());
    actor->logger_queue_cv = std::unique_ptr<std::condition_variable>(new std::condition_variable());
    actor->logger_queue_locked = std::unique_ptr<std::atomic<bool>>(new std::atomic<bool>());
    *(actor->logger_queue_locked) = false;

    std::cout << "Sarab: " << __func__ << " , " << std::dec << __LINE__ << std::endl;
    actor->logger_queue = std::unique_ptr<std::queue<std::string>>(new std::queue<std::string>());
    */

    return actor;
}

void xma_actor_start(XmaActor *actor)
{
    //xma_thread_start(actor->thread);
    actor->thread->thread_obj = std::thread(xma_logger_actor, actor);
    actor->thread->is_running = true;
}

void xma_actor_destroy(XmaActor *actor)
{
    char *shutdown = (char*) "shutdown\0";

    /* Send shutdown message to Actor */
    XMA_DBG_PRINTF("%s", "XMA sending shutdown message\n");
    xma_actor_sendmsg(actor, shutdown, strlen(shutdown));
    if (actor->thread->thread_obj.joinable()) {
        actor->thread->thread_obj.join();
    }
    actor->thread->is_running = false;
    //xma_thread_join(actor->thread);
    //xma_msgq_destroy(actor->msg_q);
    //xma_thread_destroy(actor->thread);
    free(actor->thread);

    free(actor);
}

int32_t xma_actor_sendmsg(XmaActor *actor, void *msg, size_t msg_size)
{
    /*
    int32_t rc;
    bool    was_empty;

    pthread_mutex_lock(&actor->lock);
    XMA_DBG_PRINTF("XMA actor sendmsg on entry depth=%d, front=%d, back=%d\n",
            actor->msg_q->num_entries,
            actor->msg_q->front,
            actor->msg_q->back);
    if (xma_msgq_isfull(actor->msg_q))
    {
        XMA_DBG_PRINTF("%s", "Waiting: msgq_isfull\n");
        pthread_cond_wait(&actor->dequeued_cond, &actor->lock);
        XMA_DBG_PRINTF("%s", "Waiting: msgq_isfull done\n");
    }
    was_empty = xma_msgq_isempty(actor->msg_q);
    rc = xma_msgq_enqueue(actor->msg_q, msg, msg_size);
    if (rc == 0 && was_empty)
    {
        XMA_DBG_PRINTF("%s", "Sending queued_cond for previously empty queue\n");
        pthread_cond_broadcast(&actor->queued_cond);
    }

    XMA_DBG_PRINTF("XMA actor sendmsg on exit depth=%d, front=%d, back=%d\n",
            actor->msg_q->num_entries,
            actor->msg_q->front,
            actor->msg_q->back);

    pthread_mutex_unlock(&actor->lock);
    */
    // First acquire queue lock
    bool expected = false;
    bool desired = true;
    while (!(*(actor->logger_queue_locked)).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Queue lock acquired
    bool was_empty = actor->logger_queue->empty();

    actor->logger_queue->emplace(std::string((char*)msg, ((char*)msg)+msg_size));
    if (was_empty) {
        (*(actor->logger_queue_cv)).notify_all();
    }
    *(actor->logger_queue_locked) = false;

    return 0;
}

int32_t xma_actor_recvmsg(XmaActor *actor, void *msg, size_t msg_size)
{
    /*
    int32_t rc;
    bool    was_full;

    pthread_mutex_lock(&actor->lock);

    XMA_DBG_PRINTF("XMA actor recvmsg on entry depth=%d, front=%d, back=%d\n",
            actor->msg_q->num_entries,
            actor->msg_q->front,
            actor->msg_q->back);


    if (xma_msgq_isempty(actor->msg_q))
        pthread_cond_wait(&actor->queued_cond, &actor->lock);

    was_full = xma_msgq_isfull(actor->msg_q);
    rc = xma_msgq_dequeue(actor->msg_q, msg, msg_size);
    if (was_full)
    {
        XMA_DBG_PRINTF("%s", "sending dequeued_cond\n");
        pthread_cond_broadcast(&actor->dequeued_cond);
    }

    XMA_DBG_PRINTF("XMA actor recvmsg on exit depth=%d, front=%d, back=%d\n",
            actor->msg_q->num_entries,
            actor->msg_q->front,
            actor->msg_q->back);

    pthread_mutex_unlock(&actor->lock);
    */

    // First acquire queue lock
    bool lock_acquired = false;
    bool expected = false;
    bool desired = true;
    while (!(*(actor->logger_queue_locked)).compare_exchange_weak(expected, desired)) {
        expected = false;
    }
    //Queue lock acquired
    lock_acquired = true;
    if (actor->logger_queue->empty()) {
        *(actor->logger_queue_locked) = false;
        lock_acquired = false;
        std::unique_lock<std::mutex> lk(*(actor->logger_queue_mutex));
        (*(actor->logger_queue_cv)).wait(lk, [&]{return (!(actor->logger_queue->empty()));});
    }
    if (!lock_acquired) {
        expected = false;
        desired = true;
        while (!(*(actor->logger_queue_locked)).compare_exchange_weak(expected, desired)) {
            expected = false;
        }
        lock_acquired = true;
    }
    
    std::string log_msg = actor->logger_queue->front();
    uint32_t max = log_msg.size();
    if (max > msg_size-1) {
        max = msg_size-1;
        XMA_DBG_PRINTF("%s", "XMA msgq dequeue: too small\n");
    }
    std::copy(log_msg.begin(), log_msg.begin()+max, (char*)msg);

    actor->logger_queue->pop();
    *(actor->logger_queue_locked) = false;

    return 0;
}
