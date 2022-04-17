/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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

#include "SectionAIEPartition.h"

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;
#include <iostream>

// Static Variables / Classes
SectionAIEPartition::_init SectionAIEPartition::_initializer;

SectionAIEPartition::SectionAIEPartition()
{
  // Empty
}

SectionAIEPartition::~SectionAIEPartition()
{
  // Empty
}

void
SectionAIEPartition::marshalToJSON(char* _pDataSection,
                                   unsigned int _sectionSize,
                                   boost::property_tree::ptree& _ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: AIE_PARTITION");
  XUtil::TRACE_BUF("Section Buffer", reinterpret_cast<const char*>(_pDataSection), _sectionSize);

  // Do we have enough room to overlay the header structure
  if (_sectionSize < sizeof(aie_partition)) {
    auto errMsg = boost::format("ERROR: Section size (%d) is smaller than the size of the aie_partition structure (%d)")
                                 % _sectionSize % sizeof(aie_partition);
    throw std::runtime_error(errMsg.str());
  }

  aie_partition* pHdr = (aie_partition*)_pDataSection;
  boost::property_tree::ptree ptAIEPartition;

  // -- schema version
  XUtil::TRACE(boost::format("schema_version: %d") % static_cast<uint32_t>(pHdr->schema_version));
  ptAIEPartition.put("schema_version", std::to_string(pHdr->schema_version));

  // -- mpo_name
  XUtil::TRACE(boost::format("mpo_name (0x%lx): '%s'") % pHdr->mpo_name % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name));
  ptAIEPartition.put("name", reinterpret_cast<char*>(pHdr) + pHdr->mpo_name);

  // -- partition_info
  boost::property_tree::ptree ptPartitionInfo;
  {
    // -- column width
    XUtil::TRACE(boost::format("column_width: %d") % pHdr->info.column_width);
    ptPartitionInfo.put("column_width", std::to_string(pHdr->info.column_width));

    // -- start_column array
    XUtil::TRACE(boost::format("columns count: %d, offset: 0x%0x")
                               % pHdr->info.start_columns_count
                               % pHdr->info.mpo_auint16_start_columns);

    if (pHdr->info.start_columns_count) {
      const uint16_t* startColumns = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(pHdr) + pHdr->info.mpo_auint16_start_columns);

      boost::property_tree::ptree ptArray;
      for (size_t index = 0; index < pHdr->info.start_columns_count; index++) {
        boost::property_tree::ptree ptEntry;
        ptEntry.put("", std::to_string(startColumns[index]));
        ptArray.push_back(std::make_pair("", ptEntry));   // Used to make an array of objects
      }
      ptPartitionInfo.add_child("start_columns", ptArray);
    }
  }
  ptAIEPartition.add_child("partition_info", ptPartitionInfo);

  _ptree.add_child("aie_partition", ptAIEPartition);
  XUtil::TRACE("-----------------------------");
}

void
SectionAIEPartition::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                     std::ostringstream& _buf) const
{
  const boost::property_tree::ptree ptEmpty;
  std::ostringstream stringBlock;              // Contains variable length strings

  const boost::property_tree::ptree& ptAIEPartition = _ptSection.get_child("aie_partition");
  XUtil::TRACE_PrintTree("AIE_PARTITION", ptAIEPartition);
  aie_partition aie_partitionHdr = aie_partition{0};

  // -- Schema Version
  aie_partitionHdr.schema_version = ptAIEPartition.get<uint8_t>("schema_version");
  XUtil::TRACE(boost::format("schema_version: %d") % static_cast<uint32_t>(aie_partitionHdr.schema_version));

  // -- Offsets
  // -- Start Column
  const uint32_t startColumnsOffset = sizeof(aie_partition);
  std::vector<uint16_t> startColumns = XUtil::as_vector_simple<uint16_t>(ptAIEPartition, "partition_info.start_columns");
  size_t startColumnsRawSize = sizeof(uint16_t) * startColumns.size();
  XUtil::TRACE(boost::format("start_column_offset: 0x%x, raw size: 0x%x, aligned_size: 0x%x")
               % startColumnsOffset % startColumnsRawSize
               % (startColumnsRawSize + XUtil::bytesToAlign(startColumnsRawSize)));

  // -- String Block
  const uint32_t stringBlockOffset = startColumnsOffset + startColumnsRawSize + XUtil::bytesToAlign(startColumnsRawSize);
  XUtil::TRACE(boost::format("staring_block_offset: 0x%x") % stringBlockOffset);

  // -- Name
  auto partitionName = ptAIEPartition.get<std::string>("name");
  aie_partitionHdr.mpo_name = stringBlockOffset + stringBlock.tellp();
  stringBlock << partitionName << '\0';

  // -- Partition Info
  const boost::property_tree::ptree& ptParitionInfo = ptAIEPartition.get_child("partition_info");
  aie_partitionHdr.info.column_width = ptParitionInfo.get<uint16_t>("column_width");
  aie_partitionHdr.info.start_columns_count = startColumns.size();
  if (startColumns.size())
    aie_partitionHdr.info.mpo_auint16_start_columns = startColumnsOffset;

  // -- Copy the output to the output buffer.
  // Header
  _buf.write(reinterpret_cast<const char*>(&aie_partitionHdr), sizeof(aie_partitionHdr));

  // Start columns
  auto bufferSize = sizeof(uint16_t) * startColumns.size();
  if (bufferSize) {
    _buf.write(reinterpret_cast<const char*>(startColumns.data()), bufferSize);

    // Align the buffer on the 64 bit boundary
    auto numAlignBytes = XUtil::alignBytes(_buf, sizeof(uint64_t));
    if (numAlignBytes != XUtil::bytesToAlign(startColumnsRawSize))
      throw std::runtime_error("ERROR: Buffer alignment mismatch. Number of padding bytes written does not match expectation");
  }

  // String block
  std::string sStringBlock = stringBlock.str();
  _buf.write(sStringBlock.c_str(), sStringBlock.size());
}

bool
SectionAIEPartition::doesSupportAddFormatType(FormatType _eFormatType) const
{
  if (_eFormatType == FT_JSON) {
    return true;
  }
  return false;
}

bool
SectionAIEPartition::doesSupportDumpFormatType(FormatType _eFormatType) const
{
  if ((_eFormatType == FT_JSON) ||
      (_eFormatType == FT_HTML) ||
      (_eFormatType == FT_RAW)) {
    return true;
  }

  return false;
}
