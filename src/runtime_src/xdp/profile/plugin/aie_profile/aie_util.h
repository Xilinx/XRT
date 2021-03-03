/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef XDP_AIE_UTIL_DOT_H_
#define XDP_AIE_UTIL_DOT_H_

// AIE event IDs
#define AIE_EVENT_COMBO0               9
#define AIE_EVENT_GROUP_DMA           20
#define AIE_EVENT_CORE_STALL          22
#define AIE_EVENT_MEMORY_STALL        23
#define AIE_EVENT_STREAM_STALL        24
#define AIE_EVENT_LOCK_STALL          25
#define AIE_EVENT_CASCADE_STALL       26
#define AIE_EVENT_ACTIVE              28
#define AIE_EVENT_DISABLED            29
#define AIE_EVENT_CALL_INSTRUCTION    35
#define AIE_EVENT_RETURN_INSTRUCTION  36
#define AIE_EVENT_VECTOR_INSTRUCTION  37
#define AIE_EVENT_LOAD_INSTRUCTION    38
#define AIE_EVENT_STORE_INSTRUCTION   39
#define AIE_EVENT_GROUP_LOCKS         43
#define AIE_EVENT_GROUP_CONFLICTS     76
#define AIE_EVENT_GROUP_ERRORS        86

#endif /* XDP_AIE_UTIL_DOT_H_ */
