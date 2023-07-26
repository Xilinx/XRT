/**
 * Copyright (C) 2018-2023 Xilinx, Inc
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
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>

// Disable windows compiler warnings
#ifdef _WIN32
  #pragma warning( disable : 4100)      // 4100 - Unreferenced formal parameter
#endif


namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionMCS::init SectionMCS::initializer;

SectionMCS::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(MCS, "MCS", boost::factory<SectionMCS*>());
  sectionInfo->supportsSubSections = true;
  sectionInfo->subSections.push_back(getSubSectionName(MCS_PRIMARY));
  sectionInfo->subSections.push_back(getSubSectionName(MCS_SECONDARY));

  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// --------------------------------------------------------------------------

using SubSectionTableCollection = std::vector<std::pair<std::string, MCS_TYPE>>;
static const SubSectionTableCollection&
getSubSectionTable()
{
  static const SubSectionTableCollection subSectionTable = {
    { "UNKNOWN", MCS_UNKNOWN },
    { "PRIMARY", MCS_PRIMARY },
    { "SECONDARY", MCS_SECONDARY }
  };

  return subSectionTable;
}


MCS_TYPE
SectionMCS::getSubSectionEnum(const std::string& sSubSectionName)
{
  auto subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return boost::iequals(entry.first, sSubSectionName);});

  if (iter == subSectionTable.end())
    return MCS_UNKNOWN;

  return iter->second;
}

// -------------------------------------------------------------------------

const std::string&
SectionMCS::getSubSectionName(MCS_TYPE eSubSection)
{
  auto subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return entry.second == eSubSection;});

  if (iter == subSectionTable.end())
    return getSubSectionName(MCS_UNKNOWN);
  return iter->first;
}

// --------------------------------------------------------------------------

void
SectionMCS::marshalToJSON(char* _pDataSegment,
                          unsigned int _segmentSize,
                          boost::property_tree::ptree& _ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: MCS");

  // Do we have enough room to overlay the header structure
  if (_segmentSize < sizeof(mcs)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the mcs structure (%d)") % _segmentSize % sizeof(mcs);
    throw std::runtime_error(errMsg.str());
  }

  mcs* pHdr = (mcs*)_pDataSegment;

  boost::property_tree::ptree pt_mcs;

  XUtil::TRACE(boost::format("m_count: %d") % (uint32_t)pHdr->m_count);
  XUtil::TRACE_BUF("mcs", reinterpret_cast<const char*>(pHdr), ((uint64_t)&(pHdr->m_chunk[0]) - (uint64_t)pHdr));

  // Do we have something to extract.  Note: This should never happen.
  if (pHdr->m_count == 0) {
    XUtil::TRACE("m_count is zero, nothing to extract");
    return;
  }

  pt_mcs.put("count", (boost::format("%d") % (unsigned int)pHdr->m_count).str());

  // Check to make sure that the array did not exceed its bounds
  uint64_t arraySize = ((uint64_t)&(pHdr->m_chunk[0]) - (uint64_t)pHdr) + (sizeof(mcs_chunk) * pHdr->m_count);

  if (arraySize > _segmentSize) {
    auto errMsg = boost::format("ERROR: m_chunk array size (0x%lx) exceeds segment size (0x%lx).") % arraySize % _segmentSize;
    throw std::runtime_error(errMsg.str());
  }

  // Examine and extract the data
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree pt_mcs_chunk;
    XUtil::TRACE(boost::format("[%d]: m_type: %s, m_offset: 0x%lx, m_size: 0x%lx")
                 % index
                 % getSubSectionName((MCS_TYPE)pHdr->m_chunk[index].m_type)
                 % pHdr->m_chunk[index].m_offset
                 % pHdr->m_chunk[index].m_size);

    XUtil::TRACE_BUF("m_chunk", reinterpret_cast<const char*>(&(pHdr->m_chunk[index])), sizeof(mcs_chunk));

    // Do some error checking
    char* ptrImageBase = _pDataSegment + pHdr->m_chunk[index].m_offset;

    // Check to make sure that the MCS image is partially looking good
    if ((uint64_t)ptrImageBase > ((uint64_t)_pDataSegment) + _segmentSize) {
      auto errMsg = boost::format("ERROR: MCS image %d start offset exceeds MCS segment size.") % index;
      throw std::runtime_error(errMsg.str());
    }

    if (((uint64_t)ptrImageBase) + pHdr->m_chunk[index].m_size > ((uint64_t)_pDataSegment) + _segmentSize) {
      auto errMsg = boost::format("ERROR: MCS image %d size exceeds the MCS segment size.") % index;
      throw std::runtime_error(errMsg.str());
    }

    pt_mcs_chunk.put("m_type", getSubSectionName((MCS_TYPE)pHdr->m_chunk[index].m_type).c_str());
    pt_mcs_chunk.put("m_offset", (boost::format("0x%ld") % pHdr->m_chunk[index].m_offset).str());
    pt_mcs_chunk.put("m_size", (boost::format("0x%ld") % pHdr->m_chunk[index].m_size).str());
  }

  // TODO: Add support to write out this data
}

// --------------------------------------------------------------------------

void
SectionMCS::getSubPayload(char* _pDataSection,
                          unsigned int _sectionSize,
                          std::ostringstream& _buf,
                          const std::string& _sSubSectionName,
                          Section::FormatType _eFormatType) const
{
  // Make sure we support the subsystem
  if (Section::supportsSubSectionName(m_eKind, _sSubSectionName) == false) {
    auto errMsg = boost::format("ERROR: For section '%s' the subsystem '%s' is not supported.") % getSectionKindAsString() % _sSubSectionName;
    throw std::runtime_error(errMsg.str());
  }

  // Make sure we support the format type
  if (_eFormatType != FormatType::raw) {
    auto errMsg = boost::format("ERROR: For section '%s' the format type (%d) is not supported.") % getSectionKindAsString() % (unsigned int)_eFormatType;
    throw std::runtime_error(errMsg.str());
  }

  // Get the payload
  std::vector<mcsBufferPair> mcsBuffers;

  if (m_pBuffer != nullptr) {
    extractBuffers(m_pBuffer, m_bufferSize, mcsBuffers);
  }

  MCS_TYPE eMCSType = getSubSectionEnum(_sSubSectionName);

  for (auto mcsBuffer : mcsBuffers) {
    if (mcsBuffer.first == eMCSType) {
      const std::string& sBuffer = mcsBuffer.second->str();
      _buf.write(sBuffer.c_str(), sBuffer.size());
    }
  }
}

// --------------------------------------------------------------------------

void
SectionMCS::extractBuffers(const char* _pDataSection,
                           unsigned int _sectionSize,
                           std::vector<mcsBufferPair>& _mcsBuffers) const
{
  XUtil::TRACE("Extracting: MCS buffers");

  // Do we have enough room to overlay the header structure
  if (_sectionSize < sizeof(mcs)) {
    auto errMsg = boost::format("ERROR: Section size (%d) is smaller than the size of the mcs structure (%d)") % _sectionSize % sizeof(mcs);
    throw std::runtime_error(errMsg.str());
  }

  mcs* pHdr = (mcs*)_pDataSection;

  XUtil::TRACE(boost::format("m_count: %d") % (uint32_t)pHdr->m_count);
  XUtil::TRACE_BUF("mcs", reinterpret_cast<const char*>(pHdr), ((uint64_t)&(pHdr->m_chunk[0]) - (uint64_t)pHdr));

  // Do we have something to extract.  Note: This should never happen.
  if (pHdr->m_count == 0) {
    XUtil::TRACE("m_count is zero, nothing to extract");
    return;
  }

  // Check to make sure that the array did not exceed its bounds
  uint64_t arraySize = ((uint64_t)&(pHdr->m_chunk[0]) - (uint64_t)pHdr) + (sizeof(mcs_chunk) * pHdr->m_count);

  if (arraySize > _sectionSize) {
    auto errMsg = boost::format("ERROR: m_chunk array size (0x%lx) exceeds segment size (0x%lx).") % arraySize % _sectionSize;
    throw std::runtime_error(errMsg.str());
  }

  // Examine and extract the data
  for (int index = 0; index < pHdr->m_count; ++index) {
    XUtil::TRACE(boost::format("[%d]: m_type: %s, m_offset: 0x%lx, m_size: 0x%lx")
                 % index
                 % getSubSectionName((MCS_TYPE)pHdr->m_chunk[index].m_type)
                 % pHdr->m_chunk[index].m_offset
                 % pHdr->m_chunk[index].m_size);

    XUtil::TRACE_BUF("m_chunk", reinterpret_cast<const char*>(&(pHdr->m_chunk[index])), sizeof(mcs_chunk));

    const char* ptrImageBase = _pDataSection + pHdr->m_chunk[index].m_offset;

    // Check to make sure that the MCS image is partially looking good
    if ((uint64_t)ptrImageBase > ((uint64_t)_pDataSection) + _sectionSize) {
      auto errMsg = boost::format("ERROR: MCS image %d start offset exceeds MCS segment size.") % index;
      throw std::runtime_error(errMsg.str());
    }

    if (((uint64_t)ptrImageBase) + pHdr->m_chunk[index].m_size > ((uint64_t)_pDataSection) + _sectionSize) {
      auto errMsg = boost::format("ERROR: MCS image %d size exceeds the MCS segment size.") % index;
      throw std::runtime_error(errMsg.str());
    }

    std::ostringstream* pBuffer = new std::ostringstream;
    pBuffer->write(ptrImageBase, pHdr->m_chunk[index].m_size);

    _mcsBuffers.emplace_back((MCS_TYPE)pHdr->m_chunk[index].m_type, pBuffer);
  }
}

// --------------------------------------------------------------------------

void
SectionMCS::buildBuffer(const std::vector<mcsBufferPair>& _mcsBuffers,
                        std::ostringstream& _buffer) const
{
  XUtil::TRACE("Building: MCS buffers");

  // Must have something to work with
  int count = (int)_mcsBuffers.size();
  if (count == 0)
    return;

  mcs mcsHdr = mcs{};
  mcsHdr.m_count = (int8_t)count;

  XUtil::TRACE(boost::format("m_count: %d") % (int)mcsHdr.m_count);

  // Write out the entire structure except for the mcs structure
  XUtil::TRACE_BUF("mcs - minus mcs_chunk", reinterpret_cast<const char*>(&mcsHdr), (sizeof(mcs) - sizeof(mcs_chunk)));
  _buffer.write(reinterpret_cast<const char*>(&mcsHdr), (sizeof(mcs) - sizeof(mcs_chunk)));


  // Calculate The mcs_chunks data
  std::vector<mcs_chunk> mcsChunks;
  {
    uint64_t currentOffset = ((sizeof(mcs) - sizeof(mcs_chunk)) +
                              (sizeof(mcs_chunk) * count));

    for (auto mcsEntry : _mcsBuffers) {
      mcs_chunk mcsChunk = mcs_chunk{};
      mcsChunk.m_type = (uint8_t)mcsEntry.first;   // Record the MCS type

      mcsEntry.second->seekp(0, std::ios_base::end);
      mcsChunk.m_size = mcsEntry.second->tellp();
      mcsChunk.m_offset = currentOffset;
      currentOffset += mcsChunk.m_size;

      mcsChunks.push_back(mcsChunk);
    }
  }

  // Finish building the buffer
  // First the array
  {
    int index = 0;
    for (auto mcsChunk : mcsChunks) {
      XUtil::TRACE(boost::format("[%d]: m_type: %d, m_offset: 0x%lx, m_size: 0x%lx")
                   % index++
                   % mcsChunk.m_type
                   % mcsChunk.m_offset
                   % mcsChunk.m_size);
      XUtil::TRACE_BUF("mcs_chunk", reinterpret_cast<const char*>(&mcsChunk), sizeof(mcs_chunk));
      _buffer.write(reinterpret_cast<const char*>(&mcsChunk), sizeof(mcs_chunk));
    }
  }

  // Second the data
  {
    for (auto mcsEntry : _mcsBuffers) {
      const std::string& stringBuffer = mcsEntry.second->str();
      _buffer.write(stringBuffer.c_str(), stringBuffer.size());
    }
  }
}

// --------------------------------------------------------------------------

void
SectionMCS::readSubPayload(const char* _pOrigDataSection,
                           unsigned int _origSectionSize,
                           std::istream& _istream,
                           const std::string& _sSubSection,
                           Section::FormatType _eFormatType,
                           std::ostringstream& _buffer) const
{
  // Determine subsection name
  MCS_TYPE eMCSType = getSubSectionEnum(_sSubSection);

  if (eMCSType == MCS_UNKNOWN) {
    auto errMsg = boost::format("ERROR: Not support subsection '%s' for section '%s',") % _sSubSection % getSectionKindAsString();
    throw std::runtime_error(errMsg.str());
  }

  // Validate format type
  if (_eFormatType != Section::FormatType::raw) {
    auto errMsg = boost::format("ERROR: Section '%s' only supports 'RAW' subsections.") % getSectionKindAsString();
    throw std::runtime_error(errMsg.str());
  }

  // Get any previous sections
  std::vector<mcsBufferPair> mcsBuffers;

  if (_pOrigDataSection != nullptr) {
    extractBuffers(_pOrigDataSection, _origSectionSize, mcsBuffers);
  }

  // Check to see if subsection already exists
  for (auto mcsEntry : mcsBuffers) {
    if (mcsEntry.first == eMCSType) {
      auto errMsg = boost::format("ERROR: Subsection '%s' already exists for section '%s',") % _sSubSection % getSectionKindAsString();
      throw std::runtime_error(errMsg.str());
    }
  }

  // Things are good, now get this new buffer
  {
    _istream.seekg(0, _istream.end);
    std::streamsize mcsSize = _istream.tellg();

    // -- Read contents into memory buffer --
    std::unique_ptr<unsigned char[]> memBuffer(new unsigned char[mcsSize]);
    _istream.clear();
    _istream.seekg(0, _istream.beg);
    _istream.read((char*)memBuffer.get(), mcsSize);

    std::ostringstream* buffer = new std::ostringstream;
    buffer->write(reinterpret_cast<const char*>(memBuffer.get()), mcsSize);
    mcsBuffers.emplace_back(eMCSType, buffer);
  }

  // Now create a new buffer stream
  buildBuffer(mcsBuffers, _buffer);

  // Clean up the memory
  for (auto mcsEntry : mcsBuffers) {
    delete mcsEntry.second;
    mcsEntry.second = nullptr;
  }
}


// --------------------------------------------------------------------------

bool
SectionMCS::subSectionExists(const std::string& _sSubSectionName) const
{
  // Get a list of the sections
  std::vector<mcsBufferPair> mcsBuffers;
  if (m_pBuffer != nullptr) {
    extractBuffers(m_pBuffer, m_bufferSize, mcsBuffers);
  }

  // Search for the given section
  MCS_TYPE eMCSType = getSubSectionEnum(_sSubSectionName);
  for (auto mcsBuffer : mcsBuffers) {
    if (mcsBuffer.first == eMCSType) {
      return true;
    }
  }

  // If we get here, then the section of interest doesn't exist
  return false;
}

// --------------------------------------------------------------------------

void
SectionMCS::writeSubPayload(const std::string& _sSubSectionName,
                            FormatType _eFormatType,
                            std::fstream&  _oStream) const
{
  // Validate format type
  if (_eFormatType != Section::FormatType::raw) {
    auto errMsg = boost::format("ERROR: Section '%s' only supports 'RAW' subsections.") % getSectionKindAsString();
    throw std::runtime_error(errMsg.str());
  }

  // Obtain the collection of MCS buffers
  std::vector<mcsBufferPair> mcsBuffers;
  if (m_pBuffer != nullptr) {
    extractBuffers(m_pBuffer, m_bufferSize, mcsBuffers);
  }

  // Search for the collection of interest
  MCS_TYPE eMCSType = getSubSectionEnum(_sSubSectionName);
  for (auto mcsBuffer : mcsBuffers) {
    if (mcsBuffer.first == eMCSType) {
      const std::string& buffer = mcsBuffer.second->str();
      _oStream.write(buffer.c_str(), buffer.size());
      return;
    }
  }

  // No collection entry
  auto errMsg = boost::format("ERROR: Subsection '%s' of section '%s' does not exist") % _sSubSectionName % getSectionKindAsString();
  throw std::runtime_error(errMsg.str());
}


