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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Alias XmaLogLevelType to XRT standard xrtLogMsgLevel
 * Redfine XMA msg log literals to map to XRT literals
 */

typedef enum XmaLogLevelType {
     XMA_EMERGENCY_LOG = 0,
     XMA_ALERT_LOG = 1,
     XMA_CRITICAL_LOG = 2,
     XMA_ERROR_LOG = 3,
     XMA_WARNING_LOG = 4,
     XMA_NOTICE_LOG = 5,
     XMA_INFO_LOG = 6,
     XMA_DEBUG_LOG = 7
} XmaLogLevelType;

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
