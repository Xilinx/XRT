/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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
#include <iostream>
#include "AIEEventTimeStamp.h"
#include "xdp/profile/writer/vp_base/IBinaryDataWriter.h"

namespace xdp::AIEBinaryData
{
//---------------------------------------------------------------------------------------------------------------------
AIEEventTimeStamp::AIEEventTimeStamp(): IBinaryDataEvent(EventTypeID())
{
}

//---------------------------------------------------------------------------------------------------------------------
AIEEventTimeStamp::~AIEEventTimeStamp()= default;

//---------------------------------------------------------------------------------------------------------------------
void AIEEventTimeStamp::setData(IBinaryDataEvent::Time timeStamp1, IBinaryDataEvent::Time timeStamp2,
                                uint32_t column, uint32_t row, IBinaryDataEvent::Time timer)
{
  m_timeStamp1 = timeStamp1;
  m_timeStamp2 = timeStamp2;
  m_column     = column;
  m_row        = row;
  m_timer      = timer;
}

//---------------------------------------------------------------------------------------------------------------------
void AIEEventTimeStamp::clear()
{
  m_timeStamp1 = 0;
  m_timeStamp2 = 0;
  m_timer      = 0;
  m_column     = 0;
  m_row        = 0;
}

//---------------------------------------------------------------------------------------------------------------------
void AIEEventTimeStamp::print() const
{
  std::cout << m_timeStamp1 << "," << m_timeStamp2 << ","  ;
  std::cout << m_column     << "," << m_row << "," ;
  std::cout << m_timer      << "," << std::endl;
}

//---------------------------------------------------------------------------------------------------------------------
uint32_t AIEEventTimeStamp::getSize() const
{
  uint32_t eventSize  = IBinaryDataEvent::getTypeIDSize();
  eventSize += sizeof(m_timeStamp1);
  eventSize += sizeof(m_timeStamp2);
  eventSize += sizeof(m_timer);
  eventSize += sizeof(m_column);
  eventSize += sizeof(m_row);
  return eventSize;
}

//---------------------------------------------------------------------------------------------------------------------
void AIEEventTimeStamp::writeFields(IBinaryDataWriter &writer) const
{
  IBinaryDataEvent::writeTypeID(writer);
  writer.writeField((const char*) &m_timeStamp1, sizeof(IBinaryDataEvent::Time));
  writer.writeField((const char*) &m_timeStamp2, sizeof(IBinaryDataEvent::Time));
  writer.writeField((const char*) &m_timer,      sizeof(IBinaryDataEvent::Time));
  writer.writeField((const char*) &m_column,     sizeof(uint32_t));
  writer.writeField((const char*) &m_row,        sizeof(uint32_t));
}

//---------------------------------------------------------------------------------------------------------------------
uint32_t AIEEventTimeStamp::EventTypeID()
{
  return 777;
};

} // AIEBinaryData