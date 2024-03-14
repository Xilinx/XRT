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

#ifndef BINARY_WRITER_XRT_BINARYDATAWRITER_H
#define BINARY_WRITER_XRT_BINARYDATAWRITER_H

#include "xdp/config.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "BinaryDataHeader.h"
#include "IBinaryDataEvent.h"
#include "IBinaryDataWriter.h"

namespace xdp::AIEBinaryData
{

//---------------------------------------------------------------------------------------------------------------------
class BinaryDataWriter : public IBinaryDataWriter
{
public:
  XDP_CORE_EXPORT BinaryDataWriter(std::fstream& channel, const std::string& targetDevice, uint32_t hwGeneration,
                   double frequency, uint32_t packet_size);
  XDP_CORE_EXPORT ~BinaryDataWriter() override;

public:
  XDP_CORE_EXPORT void writeField(const char* data, uint32_t size) override;
  XDP_CORE_EXPORT void writeField(const std::string& str) override;
  XDP_CORE_EXPORT void writeEvent(IBinaryDataEvent::Time current_time, const IBinaryDataEvent& event) override;
  XDP_CORE_EXPORT void flush();

private:
  XDP_CORE_EXPORT void writeHeader();
  XDP_CORE_EXPORT void writePacket(const char* content, uint32_t content_size,
                   IBinaryDataEvent::Time timestamp_begin, IBinaryDataEvent::Time timestamp_end );

private:
  std::fstream& m_stream;
  std::stringstream m_buffer;
  BinaryDataHeader m_header;
  const uint32_t m_packageSize = 1024;
  uint32_t m_totalEventSize =0;
  IBinaryDataEvent::Time m_packet_time_begin =0;
  IBinaryDataEvent::Time m_packet_time_end   =0;
};

} // AIEBinaryData

#endif //BINARY_WRITER_XRT_BINARYDATAWRITER_H
