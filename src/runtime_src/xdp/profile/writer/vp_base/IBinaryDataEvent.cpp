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

#define XDP_CORE_SOURCE

#include "IBinaryDataEvent.h"
#include "IBinaryDataWriter.h"

namespace xdp::AIEBinaryData
{
//---------------------------------------------------------------------------------------------------------------------
IBinaryDataEvent::IBinaryDataEvent(uint32_t id): m_typeID(id)
{
}
//---------------------------------------------------------------------------------------------------------------------
IBinaryDataEvent::~IBinaryDataEvent() = default;
//---------------------------------------------------------------------------------------------------------------------
uint32_t IBinaryDataEvent::getTypeID() const
{
  return m_typeID;
}
//---------------------------------------------------------------------------------------------------------------------
void IBinaryDataEvent::writeTypeID(IBinaryDataWriter &writer) const
{
  const uint32_t id = getTypeID();
  writer.writeField((const char*) &id, sizeof(uint32_t));
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t IBinaryDataEvent::getTypeIDSize() const
{
  return sizeof(m_typeID); // TypeID
}

} // AIEBinaryData