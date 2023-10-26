/**
 * Copyright (C) 2019 - 2022 Xilinx, Inc
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

#include "SectionFlash.h"

#include "XclBinUtilities.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace XUtil = XclBinUtilities;

// Disable windows compiler warnings
#ifdef _WIN32
#pragma warning( disable : 4100 4267 4244)
#endif

// Static Variables / Classes
SectionFlash::init SectionFlash::initializer;

SectionFlash::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(ASK_FLASH, "FLASH", boost::factory<SectionFlash*>());
  sectionInfo->supportsSubSections = true;
  sectionInfo->subSections.push_back(getSubSectionName(SubSection::data));
  sectionInfo->subSections.push_back(getSubSectionName(SubSection::metadata));

  sectionInfo->supportsIndexing = true;

  // Add format support empty (no support)
  // The top-level section doesn't support any add syntax.
  // Must use sub-sections

  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// -------------------------------------------------------------------------
using SubSectionTableCollection = std::vector<std::pair<std::string, SectionFlash::SubSection>>;
static const SubSectionTableCollection&
getSubSectionTable()
{
  static SubSectionTableCollection subSectionTable = {
    { "UNKNOWN", SectionFlash::SubSection::unknown },
    { "DATA", SectionFlash::SubSection::data },
    { "METADATA", SectionFlash::SubSection::metadata }
  };
  return subSectionTable;
}


SectionFlash::SubSection
SectionFlash::getSubSectionEnum(const std::string& sSubSectionName)
{
  auto subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return boost::iequals(entry.first, sSubSectionName);});

  if (iter == subSectionTable.end())
    return SubSection::unknown;

  return iter->second;
}

// -------------------------------------------------------------------------

const std::string&
SectionFlash::getSubSectionName(SectionFlash::SubSection eSubSection)
{
  const auto& subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return entry.second == eSubSection;});

  if (iter == subSectionTable.end())
    return getSubSectionName(SubSection::unknown);

  return iter->first;
}



// -------------------------------------------------------------------------

bool
SectionFlash::subSectionExists(const std::string& _sSubSectionName) const
{

  // No buffer no subsections
  if (m_pBuffer == nullptr)
    return false;

  // There is a sub-system

  // Determine if the metadata section has been initialized by the user.
  // If not then it doesn't really exist

  // Extract the sub-section entry type
  const SubSection eSS = getSubSectionEnum(_sSubSectionName);

  if (eSS == SubSection::metadata) {
    // Extract the binary data as a JSON string
    std::ostringstream buffer;
    writeMetadata(buffer);

    std::stringstream ss;
    const std::string& sBuffer = buffer.str();
    XUtil::TRACE_BUF("String Image", sBuffer.c_str(), sBuffer.size());

    ss.write((char*)sBuffer.c_str(), sBuffer.size());

    // Create a property tree and determine if the variables are all default values
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);

    boost::property_tree::ptree& ptFlash = pt.get_child("flash_metadata");

    XUtil::TRACE_PrintTree("Current FLASH contents", ptFlash);
    if ((ptFlash.get<std::string>("version") == "") &&
        (ptFlash.get<std::string>("md5") == "") &&
        (ptFlash.get<std::string>("name") == "")) {
      // All default values, metadata sub-section has yet to be added
      return false;
    }
  }

  return true;
}

// -------------------------------------------------------------------------

static std::string
getFlashTypeAsString(FLASH_TYPE _eFT)
{
  switch (_eFT) {
    case FLT_BIN_PRIMARY:
      return "BIN";
    case FLT_UNKNOWN:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}


static FLASH_TYPE
getFlashType(const std::string& _sFlashType)
{
  if (_sFlashType == "BIN") {
    return FLT_BIN_PRIMARY;
  }

  return FLT_UNKNOWN;
}


// -------------------------------------------------------------------------
void
SectionFlash::copyBufferUpdateMetadata(const char* _pOrigDataSection,
                                       unsigned int _origSectionSize,
                                       std::istream& _istream,
                                       std::ostringstream& _buffer) const
{
  XUtil::TRACE("SectionFlash::CopyBufferUpdateMetadata");

  // Do we have enough room to overlay the header structure
  if (_origSectionSize < sizeof(flash)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the flash structure (%d)") % _origSectionSize % sizeof(flash);
    throw std::runtime_error(errMsg.str());
  }

  // Prepare our destination header buffer
  flash flashHdr = { 0 };              // Header buffer
  std::ostringstream stringBlock;      // String block (stored immediately after the header)

  auto pHdr = reinterpret_cast<const flash*>(_pOrigDataSection);

  XUtil::TRACE_BUF("flash-original", reinterpret_cast<const char*>(pHdr), sizeof(flash));
  XUtil::TRACE(boost::format("Original: \n"
                             "  m_flash_type (%d) : '%s' \n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  mpo_md5_value (0x%lx): '%s'\n")
               % pHdr->m_flash_type % getFlashTypeAsString((FLASH_TYPE)pHdr->m_flash_type)
               % pHdr->m_image_offset % pHdr->m_image_size
               % pHdr->mpo_name % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name)
               % pHdr->mpo_version % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_version)
               % pHdr->mpo_md5_value % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_md5_value));

  // Get the JSON metadata
  _istream.seekg(0, _istream.end);             // Go to the beginning
  std::streampos fileSize = _istream.tellg();  // Go to the end

  // Reserve buffer memory
  std::vector<uint8_t> memBuffer;
  memBuffer.resize(fileSize, 0);

  _istream.clear();                                // Clear any previous errors
  _istream.seekg(0);                               // Go to the beginning
  _istream.read((char*)memBuffer.data(), memBuffer.size()); // Read in the buffer into memory

  XUtil::TRACE_BUF("Buffer", (char*)memBuffer.data(), memBuffer.size());

  // Convert JSON memory image into a boost property tree
  std::stringstream ss;
  ss.write((char*)memBuffer.data(), memBuffer.size());

  boost::property_tree::ptree pt;
  boost::property_tree::read_json(ss, pt);

  // ----------------------

  // Extract and update the data
  boost::property_tree::ptree& ptFlash = pt.get_child("flash_metadata");

  // Update and record the variables

  // m_flash_type
  {
    auto sFlashType = ptFlash.get<std::string>("flash_type", getFlashTypeAsString((FLASH_TYPE)pHdr->m_flash_type).c_str());

    if (sFlashType.compare(getSectionIndexName()) != 0) {
      auto errMsg = boost::format("ERROR: Metadata data mpo_flash_type '%s' does not match expected section type '%s'") % sFlashType % getSectionIndexName();
      throw std::runtime_error(errMsg.str());
    }
  }

  // m_flash_type
  {
    auto sFlashType = ptFlash.get<std::string>("flash_type", getFlashTypeAsString((FLASH_TYPE)pHdr->m_flash_type).c_str());
    FLASH_TYPE eFlashType = getFlashType(sFlashType);

    flashHdr.m_flash_type = eFlashType;
    XUtil::TRACE(boost::format("  m_flash_type: %d") % flashHdr.m_flash_type);
  }

  // mpo_name
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(flash) + pHdr->mpo_name;
    auto sValue = ptFlash.get<std::string>("name", sDefault);
    flashHdr.mpo_name = sizeof(flash) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  mpo_name (0x%lx): '%s'") % flashHdr.mpo_name % sValue);
  }


  // mpo_version
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(flash) + pHdr->mpo_version;
    auto sValue = ptFlash.get<std::string>("version", sDefault);
    flashHdr.mpo_version = sizeof(flash) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  mpo_version (0x%lx): '%s'") % flashHdr.mpo_version % sValue);
  }

  // mpo_md5_value
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(flash) + pHdr->mpo_md5_value;
    auto sValue = ptFlash.get<std::string>("md5", sDefault);
    flashHdr.mpo_md5_value = sizeof(flash) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  mpo_md5_value (0x%lx): '%s'") % flashHdr.mpo_md5_value % sValue);
  }

  // Last item to be initialized
  {
    flashHdr.m_image_offset = sizeof(flash) + stringBlock.tellp();
    flashHdr.m_image_size = pHdr->m_image_size;
    XUtil::TRACE(boost::format("  m_image_offset: 0x%lx") % flashHdr.m_image_offset);
    XUtil::TRACE(boost::format("    m_image_size: 0x%lx") % flashHdr.m_image_size);
  }

  // Copy the output to the output buffer.
  // Header
  _buffer.write(reinterpret_cast<const char*>(&flashHdr), sizeof(flash));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Image
  _buffer.write(reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionFlash::createDefaultImage(std::istream& _istream, std::ostringstream& _buffer) const
{
  XUtil::TRACE("FLASH-DATA");

  flash flashHdr = flash{};
  std::ostringstream stringBlock;       // String block (stored immediately after the header)

  // Initialize type
  {
    std::string sSectionIndex = getSectionIndexName();
    FLASH_TYPE eFT = getFlashType(sSectionIndex);
    if (eFT == FLT_UNKNOWN) {
      auto errMsg = boost::format("ERROR: Unknown flash type index: '%s'") % sSectionIndex;
      throw std::runtime_error(errMsg.str());
    }
    flashHdr.m_flash_type = (uint16_t)eFT;
  }

  // Initialize default values
  {
    // Have all of the mpo (member, point, offset) values point to the zero length terminate string
    uint32_t mpo_emptyChar = sizeof(flash) + stringBlock.tellp();
    stringBlock << '\0';

    flashHdr.mpo_name = mpo_emptyChar;
    flashHdr.mpo_version = mpo_emptyChar;
    flashHdr.mpo_md5_value = mpo_emptyChar;
  }

  // Initialize the object image values (last)
  {
    _istream.seekg(0, _istream.end);
    flashHdr.m_image_size = _istream.tellg();
    flashHdr.m_image_offset = sizeof(flash) + stringBlock.tellp();
  }

  XUtil::TRACE_BUF("flash", reinterpret_cast<const char*>(&flashHdr), sizeof(flash));

  // Write the header information
  _buffer.write(reinterpret_cast<const char*>(&flashHdr), sizeof(flash));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Write Data
  {
    std::vector<uint8_t> memBuffer;
    memBuffer.resize(flashHdr.m_image_size, 0);

    _istream.seekg(0);
    _istream.clear();
    _istream.read(reinterpret_cast<char*>(memBuffer.data()), memBuffer.size());

    _buffer.write(reinterpret_cast<const char*>(memBuffer.data()), memBuffer.size());
  }
}

// -------------------------------------------------------------------------

void
SectionFlash::readSubPayload(const char* _pOrigDataSection,
                             unsigned int _origSectionSize,
                             std::istream& _istream,
                             const std::string& _sSubSectionName,
                             Section::FormatType _eFormatType,
                             std::ostringstream& _buffer) const
{
  // Determine the sub-section of interest
  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SubSection::data:
      // Some basic DRC checks
      if (_pOrigDataSection != nullptr) {
        std::string errMsg = "ERROR: Flash DATA image already exists.";
        throw std::runtime_error(errMsg);
      }

      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: Flash DATA image only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      createDefaultImage(_istream, _buffer);
      break;

    case SubSection::metadata: {
        // Some basic DRC checks
        if (_pOrigDataSection == nullptr) {
          std::string errMsg = "ERROR: Missing FLASH data image.  Add the FLASH[]-DATA image prior to changing its metadata.";
          throw std::runtime_error(errMsg);
        }

        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: FLASH[]-METADATA only supports the JSON format.";
          throw std::runtime_error(errMsg);
        }

        copyBufferUpdateMetadata(_pOrigDataSection, _origSectionSize, _istream, _buffer);
      }
      break;

    case SubSection::unknown:
    default: {
        auto errMsg = boost::format("ERROR: Subsection '%s' not support by section '%s") % _sSubSectionName % getSectionKindAsString();
        throw std::runtime_error(errMsg.str());
      }
      break;
  }
}

// -------------------------------------------------------------------------

void
SectionFlash::writeObjImage(std::ostream& _oStream) const
{
  XUtil::TRACE("SectionFlash::writeObjImage");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(flash)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the flash structure (%d)") % m_bufferSize % sizeof(flash);
    throw std::runtime_error(errMsg.str());
  }

  // No look at the data
  auto pHdr = reinterpret_cast<flash*>(m_pBuffer);

  auto pFWBuffer = reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset;
  _oStream.write(pFWBuffer, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionFlash::writeMetadata(std::ostream& _oStream) const
{
  XUtil::TRACE("FLASH-METADATA");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(flash)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the flash structure (%d)") % m_bufferSize % sizeof(flash);
    throw std::runtime_error(errMsg.str());
  }

  auto pHdr = reinterpret_cast<flash*>(m_pBuffer);

  XUtil::TRACE(boost::format("Original: \n"
                             "  m_flash_type (%d) : '%s' \n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  mpo_md5_value (0x%lx): '%s'\n")
               % pHdr->m_flash_type % getFlashTypeAsString((FLASH_TYPE)pHdr->m_flash_type)
               % pHdr->m_image_offset % pHdr->m_image_size
               % pHdr->mpo_name % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name)
               % pHdr->mpo_version % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_version)
               % pHdr->mpo_md5_value % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_md5_value));

  // Convert the data from the binary format to JSON
  boost::property_tree::ptree ptFlash;

  std::string sFlashType = getFlashTypeAsString((FLASH_TYPE)pHdr->m_flash_type);
  ptFlash.put("flash_type", sFlashType.c_str());
  ptFlash.put("name", reinterpret_cast<char*>(pHdr) + pHdr->mpo_name);
  ptFlash.put("version", reinterpret_cast<char*>(pHdr) + pHdr->mpo_version);
  ptFlash.put("md5", reinterpret_cast<char*>(pHdr) + pHdr->mpo_md5_value);

  boost::property_tree::ptree root;
  root.put_child("flash_metadata", ptFlash);

  boost::property_tree::write_json(_oStream, root);
}

// -------------------------------------------------------------------------

void
SectionFlash::writeSubPayload(const std::string& _sSubSectionName,
                              FormatType _eFormatType,
                              std::fstream&  _oStream) const
{
  // Some basic DRC checks
  if (m_pBuffer == nullptr) {
    std::string errMsg = "ERROR: Flash section does not exist.";
    throw std::runtime_error(errMsg);
  }

  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SubSection::data:
      // Some basic DRC checks
      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: FLASH[]-DATA only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      writeObjImage(_oStream);
      break;

    case SubSection::metadata: {
        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: FLASH[]-METADATA only supports the JSON format.";
          throw std::runtime_error(errMsg);
        }

        writeMetadata(_oStream);
      }
      break;

    case SubSection::unknown:
    default: {
        auto errMsg = boost::format("ERROR: Subsection '%s' not support by section '%s") % _sSubSectionName % getSectionKindAsString();
        throw std::runtime_error(errMsg.str());
      }
      break;
  }
}

void
SectionFlash::readXclBinBinary(std::istream& _istream, const axlf_section_header& _sectionHeader)
{
  Section::readXclBinBinary(_istream, _sectionHeader);

  // Extract the binary data as a JSON string
  std::ostringstream buffer;
  writeMetadata(buffer);

  std::stringstream ss;
  const std::string& sBuffer = buffer.str();
  XUtil::TRACE_BUF("String Image", sBuffer.c_str(), sBuffer.size());

  ss.write((char*)sBuffer.c_str(), sBuffer.size());

  // Create a property tree and determine if the variables are all default values
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(ss, pt);

  boost::property_tree::ptree& ptFlash = pt.get_child("flash_metadata");

  XUtil::TRACE_PrintTree("Current FLASH contents", ptFlash);

  auto sFlashType = ptFlash.get<std::string>("flash_type", "");

  if (getFlashType(sFlashType) == FLT_UNKNOWN) {
    auto errMsg = boost::format("Error: Unknown flash type: %s") % sFlashType;
    throw std::runtime_error(errMsg.str());
  }

  Section::m_sIndexName = sFlashType;
}
