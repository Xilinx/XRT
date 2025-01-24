/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef TYPES_DOT_H
#define TYPES_DOT_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  struct DeviceEventInfo
  {
    VTFEventType type;
    uint64_t eventID;
    double hostTimestamp;
    uint64_t deviceTimestamp;
  };

  struct UserRangeInfo
  {
    const char* label;
    const char* tooltip;
    uint64_t startTimestamp;
  };

  // Used for keeping track of a pair of events in the database that
  // correspond to a single event on the XRT side.  For example, when
  // keeping track of Native XRT sync calls that we want to display as
  // both an API event and a data transfer event.
  struct EventPair
  {
    uint64_t APIEventId;
    uint64_t transferEventId;
  };

} // end namespace xdp

namespace xdp::counters {

  // All plugins that use counters share the same type
  struct Sample
  {
    double timestamp;
    std::vector<uint64_t> values;
  };

  // Different container to handle two timestamps
  struct DoubleSample
  {
    unsigned long timestamp1;
    unsigned long timestamp2;
    std::vector<uint64_t> values;
  };
} // end namespace xdp::counters

namespace xdp::aie {

  struct TraceDataType
  {
    std::vector<unsigned char *> buffer;
    std::vector<uint64_t> bufferSz;
    bool owner;
  };

  typedef std::vector<TraceDataType*> TraceDataVector;

  struct AIEDebugDataType
  {
    uint8_t col;
    uint8_t row;
    uint32_t value;
    uint64_t offset;
    std::string name;
  };

} // end namespace xdp::aie

#endif
