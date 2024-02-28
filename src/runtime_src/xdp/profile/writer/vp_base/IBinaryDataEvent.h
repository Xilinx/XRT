/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef BINARY_WRITER_XRT_IBINARYDATAEVENT_H
#define BINARY_WRITER_XRT_IBINARYDATAEVENT_H

#include "xdp/config.h"
#include <cstdint>

namespace xdp::AIEBinaryData
{

class IBinaryDataWriter; //class forward

class IBinaryDataEvent
{
public:
  typedef uint64_t Time;

public:
  XDP_CORE_EXPORT explicit IBinaryDataEvent(uint32_t TypeID);
  XDP_CORE_EXPORT virtual ~IBinaryDataEvent();

public:
  [[nodiscard]] XDP_CORE_EXPORT uint32_t getTypeID() const;
  [[nodiscard]] XDP_CORE_EXPORT uint32_t getTypeIDSize() const;
  XDP_CORE_EXPORT void writeTypeID(IBinaryDataWriter &writer) const;

public:
  [[nodiscard]] XDP_CORE_EXPORT virtual uint32_t getSize() const =0;
  XDP_CORE_EXPORT virtual void writeFields(IBinaryDataWriter &writer) const =0;
  XDP_CORE_EXPORT virtual void clear() = 0;
  XDP_CORE_EXPORT virtual void print() const = 0;

private:
  uint32_t m_typeID;
};

} // xdp::AIEBinaryData

#endif //BINARY_WRITER_XRT_IBINARYDATAEVENT_H
