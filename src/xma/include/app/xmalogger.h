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
 * @ingroup xma_app_intf
 * @file app/xmalogger.h
 * Logging facility used by XMA applications and plugins. 
*/

/**
 * @ingroup xma
 * @addtogroup xmalog xmalogger.h
 * @{
*/

/**
 * @enum XmLogLevelType
 * Describes the logging level associated with a log message.
*/
typedef enum XmaLogLevelType
{
    XMA_CRITICAL_LOG = 0, /**< 0 */
    XMA_ERROR_LOG,        /**< 1 */
    XMA_INFO_LOG,         /**< 2 */
    XMA_DEBUG_LOG,        /**< 3 */
} XmaLogLevelType;

/**
 * @brief Log a message
 *
 * This function logs a message to stdout, a file, or both depending on
 * how the logger is configured in the system configuration YAML file.
 * The log message includes the current time, logging level, a unique
 * name, and a message.
 *
 * @param level Logging level associated with the message
 * @param name  Pointer to a C string indicating the name of the entity
 *              that is generating the log message
 * @param msg   Pointer to a compatible C format string
 * @param ...   Variable arguments used in conjunction with the
 *              C format string contained in the msg parameter
 *
*/
void
xma_logmsg(XmaLogLevelType level, const char *name, const char *msg, ...);

/** @} */
#ifdef __cplusplus
}
#endif

#endif
