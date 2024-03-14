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

#ifndef BINARY_WRITER_XRT_AIEEVENTTIMESTAMP_H
#define BINARY_WRITER_XRT_AIEEVENTTIMESTAMP_H

#include <string>
#include "xdp/profile/writer/vp_base/IBinaryDataEvent.h"

namespace xdp::AIEBinaryData
{

class AIEEventTimeStamp  : public IBinaryDataEvent
{
public:
  AIEEventTimeStamp() ;
  ~AIEEventTimeStamp() override ;

public:
  void setData(IBinaryDataEvent::Time timeStamp1, IBinaryDataEvent::Time timeStamp2,
               uint32_t column, uint32_t row, IBinaryDataEvent::Time timer);

public:
  void clear() override ;
  void print() const override ;
  [[nodiscard]] uint32_t getSize() const override;
  void writeFields(IBinaryDataWriter& writer) const override ;

public:
  static uint32_t EventTypeID() ;

public:
  IBinaryDataEvent::Time m_timeStamp1 = 0;
  IBinaryDataEvent::Time m_timeStamp2 = 0;
  IBinaryDataEvent::Time m_timer = 0;
  uint32_t m_column = 0;
  uint32_t m_row = 0;
};

} // AIEBinaryData

#endif //BINARY_WRITER_XRT_AIEEVENTTIMESTAMP_H
