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

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory.h>
#include <string>
#include "BinaryDataWriter.h"
#include "IBinaryDataEvent.h"

namespace xdp::AIEBinaryData
{
//---------------------------------------------------------------------------------------------------------------------
BinaryDataWriter::BinaryDataWriter(std::fstream& stream,  const std::string& targetDevice, uint32_t hwGeneration,
                                   double frequency, uint32_t packet_size)
    : m_stream(stream), m_buffer(std::ios_base::in | std::ios_base::out | std::ios_base::binary),
      m_packageSize(packet_size)
{
  time_t currentTime;
  m_header.setTargetDevice(targetDevice);
  m_header.m_hwGeneration = hwGeneration;
  m_header.m_fileType    = 1;
  m_header.m_dataVersion = 1;
  m_header.m_frequency   = frequency;
  m_header.m_packageSize = packet_size;
  m_header.m_dateStamp   = static_cast<uint8_t>(time(&currentTime));

  writeHeader();
}
//---------------------------------------------------------------------------------------------------------------------
BinaryDataWriter::~BinaryDataWriter()
{
  flush();
}
//---------------------------------------------------------------------------------------------------------------------
void BinaryDataWriter::writeHeader()
{
  m_stream.seekg(0, std::ios::beg);
  m_stream.write( reinterpret_cast<const char *>(&m_header), sizeof(m_header));
  m_totalEventSize = PacketHeader::getPacketHeaderSize();;
}
//---------------------------------------------------------------------------------------------------------------------
void BinaryDataWriter::writePacket(const char* content, uint32_t content_size, IBinaryDataEvent::Time timestamp_begin,
                                   IBinaryDataEvent::Time timestamp_end)
{
  PacketHeader aPacketHeader;
  aPacketHeader.m_content_size    = content_size;
  aPacketHeader.m_timestamp_begin = timestamp_begin;
  aPacketHeader.m_timestamp_end   = timestamp_end;
  m_stream.write( reinterpret_cast<const char *>(&aPacketHeader), sizeof(aPacketHeader));
  if (content_size > 0)
    m_stream.write(content, content_size);

  uint32_t padding_size = m_packageSize - sizeof(aPacketHeader) - content_size;
  if (padding_size > 0) {
    void* pVoid = malloc(padding_size);
    memset(pVoid, 0, padding_size);
    m_stream.write((const char*)pVoid, padding_size);
    free(pVoid);
  }
  m_totalEventSize = PacketHeader::getPacketHeaderSize();;
}

//---------------------------------------------------------------------------------------------------------------------
void BinaryDataWriter::writeEvent(const IBinaryDataEvent::Time current_time, const IBinaryDataEvent& dataEvent)
{
  uint32_t eventSize = 0;
  eventSize += dataEvent.getSize();
  if ( (m_totalEventSize + eventSize) > m_packageSize ) {
    writePacket(m_buffer.str().c_str(), static_cast<uint32_t>(m_buffer.tellp()),
                m_packet_time_begin, m_packet_time_end);
    m_buffer.str("");
    m_buffer.clear(); //clear state flags.
    m_packet_time_begin = m_packet_time_end;
  }
  m_packet_time_end = current_time;
  m_totalEventSize += eventSize;
  dataEvent.writeFields(*this);
}

//---------------------------------------------------------------------------------------------------------------------
void BinaryDataWriter::writeField(const char* data, uint32_t size)
{
  m_buffer.write(data, size);
}
//---------------------------------------------------------------------------------------------------------------------
void BinaryDataWriter::writeField(const std::string& str)
{
  m_buffer.write(str.c_str(), static_cast<std::streamsize>(str.length()));
  static const char endChar = 0;
  m_buffer.write((const char*) &endChar, sizeof(char));
}
//---------------------------------------------------------------------------------------------------------------------
void BinaryDataWriter::flush()
{
  if ( !m_buffer.str().empty() ) {
    writePacket(m_buffer.str().c_str(), static_cast<uint32_t>(m_buffer.tellp()),
                m_packet_time_begin, m_packet_time_end);
    m_buffer.str("");
    m_buffer.clear(); //clear state flags.
    m_packet_time_begin = m_packet_time_end;
  }
  m_stream.flush();
}

} // AIEBinaryData