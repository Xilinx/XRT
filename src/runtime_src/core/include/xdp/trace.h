/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc
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

// This file is common amongst all the shims.  The declarations in this
// file are necessary to read trace data from the FIFO in the PL.

#ifndef XDP_TRACE_DOT_H
#define XDP_TRACE_DOT_H

namespace xdp {

// This is the width of the word coming out of the trace FIFO, which is
// different on Edge and Alveo.  It is used in the various shims and in
// the XDP code base.
#ifdef XRT_EDGE
constexpr int TRACE_FIFO_WORD_WIDTH = 32;
#else
constexpr int TRACE_FIFO_WORD_WIDTH = 64;
#endif

constexpr int MAX_TRACE_NUMBER_SAMPLES_FIFO = 16384;

} // end namespace xdp

#ifdef __cplusplus
extern "C" {
#endif

enum xclPerfMonEventID {
  XCL_PERF_MON_HW_EVENT = 0
};

enum xclPerfMonEventType {
  XCL_PERF_MON_START_EVENT = 0x4,
  XCL_PERF_MON_END_EVENT   = 0x5
};

typedef struct {
  enum xclPerfMonEventID   EventID;
  enum xclPerfMonEventType EventType;
  unsigned long long int   Timestamp;
  unsigned char            Overflow;
  unsigned int             TraceID;
  unsigned char            Error;
  unsigned char            Reserved;
  int isClockTrain;
  // Used in HW Emulation
  unsigned long long int   HostTimestamp;
  unsigned char            EventFlags;
  unsigned char            WriteAddrLen;
  unsigned char            ReadAddrLen;
  unsigned short int       WriteBytes;
  unsigned short int       ReadBytes;
} xclTraceResults;

typedef struct {
  unsigned int mLength;
  xclTraceResults mArray[xdp::MAX_TRACE_NUMBER_SAMPLES_FIFO];
} xclTraceResultsVector;

#ifdef __cplusplus
}
#endif

#endif
