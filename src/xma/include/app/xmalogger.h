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
#ifndef _XMA_LOGGER_H_
#define _XMA_LOGGER_H_

#include <stdint.h>
#include <stddef.h>
#include "lib/xmalogger.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * enum XmaLogLevelType - Describes the logging level associated with a log message.
*/
typedef enum XmaLogLevelType
{
    XMA_CRITICAL_LOG = 0, /**< 0 */
    XMA_ERROR_LOG,        /**< 1 */
    XMA_INFO_LOG,         /**< 2 */
    XMA_DEBUG_LOG,        /**< 3 */
} XmaLogLevelType;

/**
 * typedef XmaLoggerCallback - Describes the function signature for an XMA logger callback.
*/
typedef void (*XmaLoggerCallback)(char *msg); 

/**
 * xma_logmsg() - This function logs a message to stdout, a file, or both depending on
 * how the logger is configured in the system configuration YAML file.
 * The log message includes the current time, logging level, a unique
 * name, and a message.
 *
 * @level: Logging level associated with the message
 * @name:  Pointer to a C string indicating the name of the entity
 *              that is generating the log message
 * @msg:   Pointer to a compatible C format string
 * @...:   Variable arguments used in conjunction with the
 * C format string contained in the msg parameter
 *
*/
void
xma_logmsg(XmaLogLevelType level, const char *name, const char *msg, ...);

/**
 * xma_logger_callback() - This function allows an XMA client to register a callback for all XMA log
 * messages that match the logging levels of equal or greater severity.  For
 * example, a level of XMA_INFO_LOG will match INFO, ERROR, and CRITICAL log
 * messages while a level of XMA_ERROR_LOG will match only ERROR and
 * CRITICAL messages.  
 * The supplied callback is given a copy of the log message buffer.  This
 * means the client must free the buffer once the message has been consumed.
 * Failure to free the message will result in a memory-leak.  The log
 * message is a standard C-style NULL terminated string and should be
 * consumed by the callback as quickly as possible since the callback is
 * invoked in the thread context of the log message invoker.  This means
 * that the callback runs in the context of the datapath and blocking calls
 * will impact performance.  To avoid performance impact, perform any output
 * in a separate thread or limit the log level to ERROR.
 *
 * @callback: Pointer to a callback function to receive log messages
 * @level:    The level of log messages to be sent to the callback
 * function.  All messages of equal or greater severity are 
 * forwarded to the callback function
*/
void
xma_logger_callback(XmaLoggerCallback callback, XmaLogLevelType level);


#ifdef __cplusplus
}
#endif

#endif
