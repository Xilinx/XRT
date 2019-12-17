/**
 * Copyright (C) 2019 Xilinx, Inc
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
namespace XUtil = XclBinUtilities;

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>

// Disable windows compiler warnings
#ifdef _WIN32
#pragma warning( disable : 4100 4267 4244)
#endif

// Static Variables / Classes
SectionFlash::_init SectionFlash::_initializer;

// -------------------------------------------------------------------------

SectionFlash::SectionFlash() {
  // Empty
}

// -------------------------------------------------------------------------

SectionFlash::~SectionFlash() {
  // Empty
}

// -------------------------------------------------------------------------

bool
SectionFlash::doesSupportAddFormatType(FormatType _eFormatType) const {
  // The FLASH top-level section does support any add syntax.
  // Must use sub-sections
  return false;
}

// -------------------------------------------------------------------------

bool
SectionFlash::subSectionExists(const std::string& _sSubSectionName) const {

  // No buffer no subsections
  if (m_pBuffer == nullptr) 
    return false;

  // There is a sub-system
  
  // Determine if the metadata section has been initialized by the user.
  // If not then it doesn't really exist
  
  // Extract the sub-section entry type
  const SubSection eSS = getSubSectionEnum(_sSubSectionName);

  if (eSS == SS_METADATA) {
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
    if ((ptFlash.get<std::string>("mpo_version") == "") &&
        (ptFlash.get<std::string>("mpo_md5_value") == "") &&
        (ptFlash.get<std::string>("mpo_name") == "")) {
      // All default values, metadata sub-section has yet to be added
      return false;
    }
  }

  return true;
}

// -------------------------------------------------------------------------

bool
SectionFlash::supportsSubSection(const std::string& _sSubSectionName) const {
  if (getSubSectionEnum(_sSubSectionName) == SS_UNKNOWN) {
    return false;
  }

  return true;
}

// -------------------------------------------------------------------------

enum SectionFlash::SubSection
SectionFlash::getSubSectionEnum(const std::string _sSubSectionName) const {

  // Case-insensitive
  std::string sSubSection = _sSubSectionName;
  boost::to_upper(sSubSection);

  // Convert string to the enumeration value
  if (sSubSection == "DATA") {
    return SS_DATA;
  }

  if (sSubSection == "METADATA") {
    return SS_METADATA;
  }

  return SS_UNKNOWN;
}

static std::string
getFlashTypeAsString(enum FLASH_TYPE _eFT) {
  switch (_eFT) {
    case FLT_BIN_PRIMARY: return "BIN";
    case FLT_UNKNOWN:     return "UNKNOWN";
  }
  return "UNKNOWN";
}


static enum FLASH_TYPE
getFlashType(const std::string &_sFlashType) {
  if (_sFlashType == "BIN") {
    return FLT_BIN_PRIMARY;
  }

  return FLT_UNKNOWN;
}


// -------------------------------------------------------------------------
void
SectionFlash::copyBufferUpdateMetadata(const char* _pOrigDataSection,
                                       unsigned int _origSectionSize,
                                       std::fstream& _istream,
                                       std::ostringstream& _buffer) const {
  XUtil::TRACE("SectionFlash::CopyBufferUpdateMetadata");

  // Do we have enough room to overlay the header structure
  if (_origSectionSize < sizeof(flash)) {
    std::string errMsg = XUtil::format("ERROR: Segment size (%d) is smaller than the size of the flash structure (%d)", _origSectionSize, sizeof(flash));
    throw std::runtime_error(errMsg);
  }

  // Prepare our destination header buffer
  flash flashHdr = { 0 };              // Header buffer
  std::ostringstream stringBlock;      // String block (stored immediately after the header)

  const flash* pHdr = reinterpret_cast<const flash*>(_pOrigDataSection);

  XUtil::TRACE_BUF("flash-original", reinterpret_cast<const char*>(pHdr), sizeof(flash));
  XUtil::TRACE(XUtil::format("Original: \n"
                             "  m_flash_type (%d) : '%s' \n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  mpo_md5_value (0x%lx): '%s'\n",
                             pHdr->m_flash_type, getFlashTypeAsString((FLASH_TYPE) pHdr->m_flash_type).c_str(),
                             pHdr->m_image_offset, pHdr->m_image_size,
                             pHdr->mpo_name, reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name,
                             pHdr->mpo_version, reinterpret_cast<const char*>(pHdr) + pHdr->mpo_version,
                             pHdr->mpo_md5_value, reinterpret_cast<const char*>(pHdr) + pHdr->mpo_md5_value));

  // Get the JSON metadata
  _istream.seekg(0, _istream.end);             // Go to the beginning
  std::streampos fileSize = _istream.tellg();  // Go to the end

  // Reserve buffer memory
  std::vector<uint8_t> memBuffer;
  memBuffer.resize(fileSize, 0);

  _istream.clear();                                // Clear any previous errors
  _istream.seekg(0);                               // Go to the beginning
  _istream.read((char*) memBuffer.data(), memBuffer.size()); // Read in the buffer into memory

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
    uint16_t flash_type = ptFlash.get<uint16_t>("m_flash_type", pHdr->m_flash_type);
    std::string sValue = getFlashTypeAsString((FLASH_TYPE) flash_type);

    if (sValue.compare(getSectionIndexName()) != 0) {
      std::string errMsg = XUtil::format("ERROR: Metadata data mpo_flash_type '%s' does not match expected section type '%s'", sValue.c_str(), getSectionIndexName().c_str());
      throw std::runtime_error(errMsg);
    }
  }

  // m_flash_type
  {
    uint16_t value = ptFlash.get<uint16_t>("m_flash_type", 0);
    flashHdr.m_flash_type = value;
    XUtil::TRACE(XUtil::format("  m_flash_type: %d", flashHdr.m_flash_type).c_str());
  }

  // mpo_name
  {
    std::string sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(flash) + pHdr->mpo_name;
    std::string sValue = ptFlash.get<std::string>("mpo_name", sDefault);
    flashHdr.mpo_name = sizeof(flash) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(XUtil::format("  mpo_name (0x%lx): '%s'", flashHdr.mpo_name, sValue.c_str()).c_str());
  }


  // mpo_version
  {
    std::string sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(flash) + pHdr->mpo_version;
    std::string sValue = ptFlash.get<std::string>("mpo_version", sDefault);
    flashHdr.mpo_version = sizeof(flash) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(XUtil::format("  mpo_version (0x%lx): '%s'", flashHdr.mpo_version, sValue.c_str()).c_str());
  }

  // mpo_md5_value
  {
    std::string sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(flash) + pHdr->mpo_md5_value;
    std::string sValue = ptFlash.get<std::string>("mpo_md5_value", sDefault);
    flashHdr.mpo_md5_value = sizeof(flash) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(XUtil::format("  mpo_md5_value (0x%lx): '%s'", flashHdr.mpo_md5_value, sValue.c_str()).c_str());
  }

  // Last item to be initialized
  {
    flashHdr.m_image_offset = sizeof(flash) + stringBlock.tellp();
    flashHdr.m_image_size = pHdr->m_image_size;
    XUtil::TRACE(XUtil::format("  m_image_offset: 0x%lx", flashHdr.m_image_offset).c_str());
    XUtil::TRACE(XUtil::format("    m_image_size: 0x%lx", flashHdr.m_image_size).c_str());
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
SectionFlash::createDefaultImage(std::fstream& _istream, std::ostringstream& _buffer) const {
  XUtil::TRACE("FLASH-DATA");

  flash flashHdr = flash{0};
  std::ostringstream stringBlock;       // String block (stored immediately after the header)

  // Initialize type
  {
    std::string sSectionIndex = getSectionIndexName();
    FLASH_TYPE eFT = getFlashType(sSectionIndex);
    if (eFT == FLT_UNKNOWN) {
      std::string errMsg = "ERROR: Unknown flash type index: '" + sSectionIndex + "'";
      throw std::runtime_error(errMsg);
    }
    flashHdr.m_flash_type = (uint16_t) eFT;
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
    _istream.read(reinterpret_cast<char *>(memBuffer.data()), memBuffer.size());

    _buffer.write(reinterpret_cast<const char*>(memBuffer.data()), memBuffer.size());
  }
}

// -------------------------------------------------------------------------

void
SectionFlash::readSubPayload(const char* _pOrigDataSection,
                             unsigned int _origSectionSize,
                             std::fstream& _istream,
                             const std::string& _sSubSectionName,
                             enum Section::FormatType _eFormatType,
                             std::ostringstream& _buffer) const {
  // Determine the sub-section of interest
  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SS_DATA:
      // Some basic DRC checks
      if (_pOrigDataSection != nullptr) {
        std::string errMsg = "ERROR: Flash DATA image already exists.";
        throw std::runtime_error(errMsg);
      }

      if (_eFormatType != Section::FT_RAW) {
        std::string errMsg = "ERROR: Flash DATA image only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      createDefaultImage(_istream, _buffer);
      break;

    case SS_METADATA: {
        // Some basic DRC checks
        if (_pOrigDataSection == nullptr) {
          std::string errMsg = "ERROR: Missing FLASH data image.  Add the FLASH[]-DATA image prior to changing its metadata.";
          throw std::runtime_error(errMsg);
        }

        if (_eFormatType != Section::FT_JSON) {
          std::string errMsg = "ERROR: FLASH[]-METADATA only supports the JSON format.";
          throw std::runtime_error(errMsg);
        }

        copyBufferUpdateMetadata(_pOrigDataSection, _origSectionSize, _istream, _buffer);
      }
      break;

    case SS_UNKNOWN:
    default: {
        std::string errMsg = XUtil::format("ERROR: Subsection '%s' not support by section '%s", _sSubSectionName.c_str(), getSectionKindAsString().c_str());
        throw std::runtime_error(errMsg);
      }
      break;
  }
}

// -------------------------------------------------------------------------

void
SectionFlash::writeObjImage(std::ostream& _oStream) const {
  XUtil::TRACE("SectionFlash::writeObjImage");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(flash)) {
    std::string errMsg = XUtil::format("ERROR: Segment size (%d) is smaller than the size of the falsh structure (%d)", m_bufferSize, sizeof(flash));
    throw std::runtime_error(errMsg);
  }

  // No look at the data
  flash* pHdr = reinterpret_cast<flash *>(m_pBuffer);

  const char* pFWBuffer = reinterpret_cast<const char *>(pHdr) + pHdr->m_image_offset;
  _oStream.write(pFWBuffer, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionFlash::writeMetadata(std::ostream& _oStream) const {
  XUtil::TRACE("FLASH-METADATA");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(flash)) {
    std::string errMsg = XUtil::format("ERROR: Segment size (%d) is smaller than the size of the flash structure (%d)", m_bufferSize, sizeof(flash));
    throw std::runtime_error(errMsg);
  }

  flash* pHdr = reinterpret_cast<flash *>(m_pBuffer);

  XUtil::TRACE(XUtil::format("Original: \n"
                             "  m_flash_type (%d) : '%s' \n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  mpo_md5_value (0x%lx): '%s'\n",
                             pHdr->m_flash_type, getFlashTypeAsString((FLASH_TYPE) pHdr->m_flash_type).c_str(),
                             pHdr->m_image_offset, pHdr->m_image_size,
                             pHdr->mpo_name, reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name,
                             pHdr->mpo_version, reinterpret_cast<const char*>(pHdr) + pHdr->mpo_version,
                             pHdr->mpo_md5_value, reinterpret_cast<const char*>(pHdr) + pHdr->mpo_md5_value));

  // Convert the data from the binary format to JSON
  boost::property_tree::ptree ptFlash;

  ptFlash.put("m_flash_type", XUtil::format("%d", pHdr->m_flash_type).c_str());
  ptFlash.put("mpo_name", reinterpret_cast<char *>(pHdr) + pHdr->mpo_name);
  ptFlash.put("mpo_version", reinterpret_cast<char *>(pHdr) + pHdr->mpo_version);
  ptFlash.put("mpo_md5_value", reinterpret_cast<char *>(pHdr) + pHdr->mpo_md5_value);

  boost::property_tree::ptree root;
  root.put_child("flash_metadata", ptFlash);

  boost::property_tree::write_json(_oStream, root);
}

// -------------------------------------------------------------------------

void
SectionFlash::writeSubPayload(const std::string& _sSubSectionName,
                                   FormatType _eFormatType,
                                   std::fstream&  _oStream) const {
  // Some basic DRC checks
  if (m_pBuffer == nullptr) {
    std::string errMsg = "ERROR: Flash section does not exist.";
    throw std::runtime_error(errMsg);
  }

  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SS_DATA:
      // Some basic DRC checks
      if (_eFormatType != Section::FT_RAW) {
        std::string errMsg = "ERROR: FLASH[]-DATA only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      writeObjImage(_oStream);
      break;

    case SS_METADATA: {
        if (_eFormatType != Section::FT_JSON) {
          std::string errMsg = "ERROR: FLASH[]-METADATA only supports the JSON format.";
          throw std::runtime_error(errMsg);
        }

        writeMetadata(_oStream);
      }
      break;

    case SS_UNKNOWN:
    default: {
        std::string errMsg = XUtil::format("ERROR: Subsection '%s' not support by section '%s", _sSubSectionName.c_str(), getSectionKindAsString().c_str());
        throw std::runtime_error(errMsg);
      }
      break;
  }
}

void
SectionFlash::readXclBinBinary(std::fstream& _istream, const axlf_section_header& _sectionHeader) {
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
  uint16_t flash_type = ptFlash.get<uint16_t>("m_flash_type", 0);
  std::string sName = getFlashTypeAsString((FLASH_TYPE) flash_type);

  Section::m_sIndexName = sName;
}
