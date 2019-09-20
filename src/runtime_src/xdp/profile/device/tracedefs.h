/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef _XDP_DEVICE_TRACEDEFS_H_
#define _XDP_DEVICE_TRACEDEFS_H_

#define TRACE_PACKET_SIZE       8

// Offsets
#define TS2MM_COUNT_LOW         0x10
#define TS2MM_COUNT_HIGH        0x14
#define TS2MM_RST               0x1c
#define TS2MM_WRITE_OFFSET_LOW  0x2c
#define TS2MM_WRITE_OFFSET_HIGH 0x30
#define TS2MM_WRITTEN_LOW       0x38
#define TS2MM_WRITTEN_HIGH      0x3c
#define TS2MM_AP_CTRL           0x0

// Commands
#define TS2MM_AP_START          0x1

// little less than 4GB
#define TS2MM_MAX_BUF_SIZE      0xffffefff
//8KB
#define TS2MM_MIN_BUF_SIZE      0x2000

#define FIFO_WARN_MSG "Trace FIFO is full because of too many events. Timeline trace could be incomplete. \
Please use 'coarse' option for data transfer trace or turn off Stall profiling"

#define TS2MM_WARN_MSG_BUFSIZE_BIG    "Trace Buffer size is too big. The maximum size of 4095M will be used."
#define TS2MM_WARN_MSG_BUFSIZE_SMALL  "Trace Buffer size is too small. The minimum size of 8K will be used."
#define TS2MM_WARN_MSG_BUFSIZE_DEF    "Trace Buffer size could not be parsed. The default size of 1M will be used."
#define TS2MM_WARN_MSG_ALLOC_FAIL     "Trace Buffer could not be allocated on device. Device trace will be missing."

#endif
