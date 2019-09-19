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
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Alias XmaLogLevelType to XRT standard xrtLogMsgLevel
 * Redfine XMA msg log literals to map to XRT literals
 */

typedef enum xrtLogMsgLevel XmaLogLevelType;

#define XMA_CRITICAL_LOG XRT_CRITICAL
#define XMA_ERROR_LOG XRT_ERROR
#define XMA_WARNING_LOG XRT_WARNING
#define XMA_INFO_LOG XRT_INFO
#define XMA_DEBUG_LOG XRT_DEBUG

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



#ifdef __cplusplus
}
#endif

#endif
