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

#ifndef BINARY_WRITER_XRT_BINARYDATAHEADER_H
#define BINARY_WRITER_XRT_BINARYDATAHEADER_H

#include "xdp/config.h"
#include <string>
#include "IBinaryDataEvent.h"

namespace xdp::AIEBinaryData
{

constexpr size_t AIE_HEADER_STR_LEN =  32;

//---------------------------------------------------------------------------------------------------------------------
struct BinaryDataHeader
{
  //Note: The struct has to multiple of 8 in order to get 32 and 64 bit machine working
  char m_header[AIE_HEADER_STR_LEN]       {0}; ///< identifies the format of the AIE DATA file
  char m_targetDevice[AIE_HEADER_STR_LEN] {0}; ///< identifies the format of the AIE DATA file
  uint32_t m_hwGeneration = 1;
  uint32_t m_fileType     = 0;
  uint32_t m_dataVersion  = 0;
  double   m_frequency    = 1250.0;
  uint32_t m_packageSize  = 1024;
  uint32_t m_dateStamp    = 0;         ///< the time at which the file was created

public:
  BinaryDataHeader();
  [[nodiscard]] XDP_CORE_EXPORT bool isHeaderMatched() const;
  XDP_CORE_EXPORT void print() const;
  XDP_CORE_EXPORT void setTargetDevice(const std::string& targetDevice);

public:
  XDP_CORE_EXPORT static void copyString(const std::string& stdString, char charString[], size_t charLength );
};
//---------------------------------------------------------------------------------------------------------------------
struct PacketHeader
{
  uint32_t m_magic            = 0;
  uint32_t m_version          = 1;
  uint32_t m_content_size     = 0;
  IBinaryDataEvent::Time m_timestamp_begin = 0;
  IBinaryDataEvent::Time m_timestamp_end   = 0;

public:
  XDP_CORE_EXPORT PacketHeader();
  [[nodiscard]] XDP_CORE_EXPORT bool isMagicNumberMatched() const;
  XDP_CORE_EXPORT void print() const;
  XDP_CORE_EXPORT static uint32_t getPacketHeaderSize();
};

} // AIEBinaryData

#endif //BINARY_WRITER_XRT_BINARYDATAHEADER_H
