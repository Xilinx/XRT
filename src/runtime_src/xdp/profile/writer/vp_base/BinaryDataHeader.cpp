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

#include <cstring>
#include <iostream>
#include "BinaryDataHeader.h"

namespace xdp::AIEBinaryData
{
//---------------------------------------------------------------------------------------------------------------------
void BinaryDataHeader::copyString(const std::string& stdString, char charString[], size_t charLength)
{
  std::memset(charString, 0, charLength);
  auto length = static_cast<size_t>(stdString.length());
  if (length > charLength)
    length = charLength;
  std::memcpy(charString, stdString.c_str(), length);
}
//---------------------------------------------------------------------------------------------------------------------
constexpr const char* AIE_VERSION_STR =  "AMD AIE DATA 01";
BinaryDataHeader::BinaryDataHeader()
{
  copyString(AIE_VERSION_STR, m_header, AIE_HEADER_STR_LEN);
}
//---------------------------------------------------------------------------------------------------------------------
bool BinaryDataHeader::isHeaderMatched() const
{
  char str[AIE_HEADER_STR_LEN];
  copyString(AIE_VERSION_STR, str, AIE_HEADER_STR_LEN);
  return std::memcmp(m_header, str, AIE_HEADER_STR_LEN) == 0;
}
//---------------------------------------------------------------------------------------------------------------------
void BinaryDataHeader::setTargetDevice(const std::string& targetDevice)
{
  copyString(targetDevice, m_targetDevice, AIE_HEADER_STR_LEN);
}
//---------------------------------------------------------------------------------------------------------------------
void BinaryDataHeader::print() const
{
  std::string targetDevice(m_targetDevice);
  std::cout << "Binary File Header" << std::endl;
  std::cout << "targetDevice = "     << targetDevice    << std::endl;
  std::cout << "m_hwGeneration = "   << m_hwGeneration  << std::endl;
  std::cout << "fileType = "         << m_fileType      << std::endl;
  std::cout << "dataVersion = "      << m_dataVersion   << std::endl;
  std::cout << "frequency = "        << m_frequency     << std::endl;
  std::cout << "packageSize = "      << m_packageSize   << std::endl;
  std::cout << "dateStamp = "        << m_dateStamp     << std::endl;
}
//---------------------------------------------------------------------------------------------------------------------
constexpr uint32_t MAGIC =  0xc1fc1fc1;
PacketHeader::PacketHeader():m_magic(MAGIC)
{
}
//---------------------------------------------------------------------------------------------------------------------
bool PacketHeader::isMagicNumberMatched() const
{
  return m_magic == MAGIC;
}
//---------------------------------------------------------------------------------------------------------------------
void PacketHeader::print() const
{
  std::cout << "Binary Packet Header" << std::endl;
  std::cout << "m_magic = "            << std::hex << m_magic << std::dec << std::endl;
  std::cout << "m_version = "          << m_version           << std::endl;
  std::cout << "m_content_size = "     << m_content_size      << std::endl;
  std::cout << "m_timestamp_begin = "  << m_timestamp_begin   << std::endl;
  std::cout << "m_timestamp_end = "    << m_timestamp_end     << std::endl;
}

//---------------------------------------------------------------------------------------------------------------------
uint32_t PacketHeader::getPacketHeaderSize()
{
  static uint32_t packageHeaderSize = sizeof(uint32_t)  + sizeof(uint32_t) + sizeof(uint32_t) +
                                      sizeof(IBinaryDataEvent::Time) + sizeof(IBinaryDataEvent::Time);
  return packageHeaderSize;
}
} // AIEBinaryData