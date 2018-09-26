/**
 * Copyright (C) 2018 Xilinx, Inc
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

#include "SectionMCS.h"

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionMCS::_init SectionMCS::_initializer;

SectionMCS::SectionMCS() {
  // Empty
}

SectionMCS::~SectionMCS() {
  // Empty
}

const std::string
SectionMCS::getMCSTypeStr(enum MCS_TYPE _mcsType) const {
  switch (_mcsType) {
    case MCS_PRIMARY:
      return "MCS_PRIMARY";
    case MCS_SECONDARY:
      return "MCS_SECONDARY";
    case MCS_UNKNOWN:
    default:
      return XUtil::format("UNKNOWN (%d)", (unsigned int)_mcsType);
  }
}


void
SectionMCS::marshalToJSON(char* _pDataSegment,
                          unsigned int _segmentSize,
                          boost::property_tree::ptree& _ptree) const {
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: MCS");

  // Do we have enough room to overlay the header structure
  if (_segmentSize < sizeof(mcs)) {
    throw std::runtime_error(XUtil::format("ERROR: Segment size (%d) is smaller than the size of the mcs structure (%d)",
                                           _segmentSize, sizeof(mcs)));
  }

  mcs* pHdr = (mcs*)_pDataSegment;

  boost::property_tree::ptree pt_mcs;

  XUtil::TRACE(XUtil::format("m_count: %d", (uint32_t)pHdr->m_count));
  XUtil::TRACE_BUF("mcs", reinterpret_cast<const char*>(pHdr), (unsigned long)&(pHdr->m_chunk[0]) - (unsigned long)pHdr);

  // Do we have something to extract.  Note: This should never happen.
  if (pHdr->m_count == 0) {
    XUtil::TRACE("m_count is zero, nothing to extract");
    return;
  }

  pt_mcs.put("count", XUtil::format("%d", (unsigned int)pHdr->m_count).c_str());

  // Check to make sure that the array did not exceed its bounds
  unsigned int arraySize = ((unsigned long)&(pHdr->m_chunk[0]) - (unsigned long)pHdr) + (sizeof(mcs_chunk) * pHdr->m_count);

  if (arraySize > _segmentSize) {
    throw std::runtime_error(XUtil::format("ERROR: m_chunk array size (0x%lx) exceeds segment size (0x%lx).",
                                           arraySize, _segmentSize));
  }

  // Examine and extract the data
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree pt_mcs_chunk;
    XUtil::TRACE(XUtil::format("[%d]: m_type: %s, m_offset: 0x%lx, m_size: 0x%lx",
                               index,
                               getMCSTypeStr((enum MCS_TYPE)pHdr->m_chunk[index].m_type).c_str(),
                               pHdr->m_chunk[index].m_offset,
                               pHdr->m_chunk[index].m_size));

    XUtil::TRACE_BUF("m_chunk", reinterpret_cast<const char*>(&(pHdr->m_chunk[index])), sizeof(mcs_chunk));

    // Do some error checking
    char* ptrImageBase = _pDataSegment + pHdr->m_chunk[index].m_offset;

    // Check to make sure that the MCS image is partially looking good
    if ((unsigned long)ptrImageBase > ((unsigned long)_pDataSegment) + _segmentSize) {
      throw std::runtime_error(XUtil::format("ERROR: MCS image %d start offset exceeds MCS segment size.", index));
    }

    if (((unsigned long)ptrImageBase) + pHdr->m_chunk[index].m_size > ((unsigned long)_pDataSegment) + _segmentSize) {
      throw std::runtime_error(XUtil::format("ERROR: MCS image %d size exceeds the MCS segment size.", index));
    }

    pt_mcs_chunk.put("m_type", getMCSTypeStr((enum MCS_TYPE)pHdr->m_chunk[index].m_type).c_str());
    pt_mcs_chunk.put("m_offset", XUtil::format("0x%ld", pHdr->m_chunk[index].m_offset).c_str());
    pt_mcs_chunk.put("m_size", XUtil::format("0x%ld", pHdr->m_chunk[index].m_size).c_str());
  }
}


