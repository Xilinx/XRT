/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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

#include <cstdint>

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

enum TraceEventType : unsigned int {
  start = 0x4,
  end   = 0x5
};

// Used when reading the events via shim functions
struct TraceEvent {
  TraceEventType EventType;
  uint64_t       Timestamp;
  uint8_t        Overflow;
  uint32_t       TraceID;
  uint8_t        Error;
  uint8_t        Reserved;
  int32_t        isClockTrain;

  // Used in HW Emulation
  uint64_t HostTimestamp;
  uint8_t  EventFlags;
  uint8_t  WriteAddrLen;
  uint8_t  ReadAddrLen;
  uint16_t WriteBytes;
  uint16_t ReadBytes;
};

struct TraceEventsVector {
  uint32_t mLength;
  TraceEvent mArray[MAX_TRACE_NUMBER_SAMPLES_FIFO];
};

} // end namespace xdp

#endif
