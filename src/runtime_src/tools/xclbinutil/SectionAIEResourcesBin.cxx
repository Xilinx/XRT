
/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
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

#include "SectionAIEResourcesBin.h"

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
SectionAIEResourcesBin::init SectionAIEResourcesBin::initializer;

SectionAIEResourcesBin::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(AIE_RESOURCES_BIN, "AIE_RESOURCES_BIN", boost::factory<SectionAIEResourcesBin*>());
  sectionInfo->supportsSubSections = true;
  sectionInfo->subSections.push_back(getSubSectionName(SubSection::obj));
  sectionInfo->subSections.push_back(getSubSectionName(SubSection::metadata));

  sectionInfo->supportsIndexing = true;

  // Add format support empty (no support)
  // The top-level section doesn't support any add syntax.
  // Must use sub-sections

  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// -------------------------------------------------------------------------

using SubSectionTableCollection = std::vector<std::pair<std::string, SectionAIEResourcesBin::SubSection>>;

static const SubSectionTableCollection&
getSubSectionTable()
{
  static const SubSectionTableCollection subSectionTable = {
    { "UNKNOWN", SectionAIEResourcesBin::SubSection::unknown },
    { "OBJ", SectionAIEResourcesBin::SubSection::obj },
    { "METADATA", SectionAIEResourcesBin::SubSection::metadata }
  };
  return subSectionTable;
}

SectionAIEResourcesBin::SubSection
SectionAIEResourcesBin::getSubSectionEnum(const std::string& sSubSectionName)
{
  auto subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return boost::iequals(entry.first, sSubSectionName);});

  if (iter == subSectionTable.end())
    return SubSection::unknown;

  return iter->second;
}

// -------------------------------------------------------------------------

const std::string&
SectionAIEResourcesBin::getSubSectionName(SectionAIEResourcesBin::SubSection eSubSection)
{
  auto subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return entry.second == eSubSection;});

  if (iter == subSectionTable.end())
    return getSubSectionName(SubSection::unknown);

  return iter->first;
}

// -------------------------------------------------------------------------

bool
SectionAIEResourcesBin::subSectionExists(const std::string& _sSubSectionName) const
{

  // No buffer no subsections
  if (m_pBuffer == nullptr) {
    return false;
  }

  // There is a sub-system

  // Determine if the metadata section has been initialized by the user.
  // If not then it doesn't really exist

  // Extract the sub-section entry type
  SubSection eSS = getSubSectionEnum(_sSubSectionName);

  if (eSS == SubSection::metadata) {
    // Extract the binary data as a JSON string
    std::ostringstream buffer;
    writeMetadata(buffer);

    std::stringstream ss;
    const std::string& sBuffer = buffer.str();
    XUtil::TRACE_BUF("String Image", sBuffer.c_str(), sBuffer.size());

    ss.write((char*)sBuffer.c_str(), sBuffer.size());
    boost::property_tree::ptree pt;
    // Create a property tree and determine if the variables are all default values
    try{
      boost::property_tree::read_json(ss, pt);
    }
    catch (const boost::property_tree::json_parser_error& e) {
      (void)e;
      auto errMsg = boost::format("ERROR: Unable to parse  metadata file of section '%s'") % getSectionIndexName();
      throw std::runtime_error(errMsg.str());
    }
    boost::property_tree::ptree& ptAieResourcesBin = pt.get_child("aie_resources_bin_metadata");

    XUtil::TRACE_PrintTree("Current AIE_RESOURCES_BIN contents", pt);
    if ((ptAieResourcesBin.get<std::string>("version") == "") &&
        (ptAieResourcesBin.get<std::string>("start_column") == "") &&
        (ptAieResourcesBin.get<std::string>("num_columns") == "")) {
      // All default values, metadata sub-section has yet to be added
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------------------------

void
SectionAIEResourcesBin::copyBufferUpdateMetadata(const char* _pOrigDataSection,
                                            unsigned int _origSectionSize,
                                            std::istream& _istream,
                                            std::ostringstream& _buffer) const
{
  XUtil::TRACE("SectionAIEResourcesBin::CopyBufferUpdateMetadata");

  // Do we have enough room to overlay the header structure
  if (_origSectionSize < sizeof(aie_resources_bin)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the aie_resources_bin structure (%d)") % _origSectionSize % sizeof(aie_resources_bin);
    throw std::runtime_error(errMsg.str());
  }

  // Prepare our destination header buffer
  aie_resources_bin aieResourcesBinHdr = {};      // Header buffer
  std::ostringstream stringBlock;         // String block (stored immediately after the header)

  auto pHdr = reinterpret_cast<const aie_resources_bin*>(_pOrigDataSection);

  XUtil::TRACE_BUF("aie_resources_bin-original", reinterpret_cast<const char*>(pHdr), sizeof(aie_resources_bin));
  XUtil::TRACE(boost::format("Original: \n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  m_start_column (0x%lx): '%s'\n"
                             "  m_num_columns (0x%lx): '%s'")
               % pHdr->mpo_name % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name)
               % pHdr->m_image_offset % pHdr->m_image_size
               % pHdr->mpo_version % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_version)
               % pHdr->m_start_column % (reinterpret_cast<const char*>(pHdr) + pHdr->m_start_column)
               % pHdr->m_num_columns % (reinterpret_cast<const char*>(pHdr) + pHdr->m_num_columns));

  // Get the JSON metadata
  _istream.seekg(0, _istream.end);             // Go to the beginning
  std::streampos fileSize = _istream.tellg();  // Go to the end

  // Copy the buffer into memory
  std::unique_ptr<unsigned char[]> memBuffer(new unsigned char[fileSize]);
  _istream.clear();                                // Clear any previous errors
  _istream.seekg(0);                               // Go to the beginning
  _istream.read((char*)memBuffer.get(), fileSize); // Read in the buffer into memory

  XUtil::TRACE_BUF("Buffer", (char*)memBuffer.get(), fileSize);

  // Convert JSON memory image into a boost property tree
  std::stringstream ss;
  ss.write((char*)memBuffer.get(), fileSize);

  boost::property_tree::ptree pt;

  try{
    boost::property_tree::read_json(ss, pt);
  }

  catch (const boost::property_tree::json_parser_error& e) {
    (void)e;
    auto errMsg = boost::format("ERROR: Unable to parse  metadata file of section '%s'") % getSectionIndexName();
    throw std::runtime_error(errMsg.str());
  }
  // ----------------------

  // Extract and update the data
  boost::property_tree::ptree& ptSK = pt.get_child("aie_resources_bin_metadata");

  // Update and record the variables

  // mpo_name
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(aie_resources_bin) + pHdr->mpo_name;
    auto sValue = ptSK.get<std::string>("name", sDefault);

    if (sValue.compare(getSectionIndexName()) != 0) {
      auto errMsg = boost::format("ERROR: Metadata data name '%s' does not match expected section name '%s'") % sValue % getSectionIndexName();
      throw std::runtime_error(errMsg.str());
    }

    aieResourcesBinHdr.mpo_name = sizeof(aie_resources_bin) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  name (0x%lx): '%s'") % aieResourcesBinHdr.mpo_name % sValue);
  }

  // mpo_version
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(aie_resources_bin) + pHdr->mpo_version;
    auto sValue = ptSK.get<std::string>("version", sDefault);
    aieResourcesBinHdr.mpo_version = sizeof(aie_resources_bin) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  version (0x%lx): '%s'") % aieResourcesBinHdr.mpo_version % sValue);
  }

  // m_start_column
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(aie_resources_bin) + pHdr->m_start_column;
    auto sValue = ptSK.get<std::string>("start_column", sDefault);
    aieResourcesBinHdr.m_start_column = sizeof(aie_resources_bin) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  start_column (0x%lx): '%s'") % aieResourcesBinHdr.m_start_column % sValue);
  }

  // m_num_columns
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(aie_resources_bin) + pHdr->m_num_columns;
    auto sValue = ptSK.get<std::string>("num_columns", sDefault);
    aieResourcesBinHdr.m_num_columns = sizeof(aie_resources_bin) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  num_columns (0x%lx): '%s'") % aieResourcesBinHdr.m_num_columns % sValue);
  }


  // Last item to be initialized
  {
    aieResourcesBinHdr.m_image_offset = sizeof(aie_resources_bin) + stringBlock.tellp();
    aieResourcesBinHdr.m_image_size = pHdr->m_image_size;
    XUtil::TRACE(boost::format("  m_image_offset: 0x%lx") % aieResourcesBinHdr.m_image_offset);
    XUtil::TRACE(boost::format("    m_image_size: 0x%lx") % aieResourcesBinHdr.m_image_size);
  }

  // Copy the output to the output buffer.
  // Header
  _buffer.write(reinterpret_cast<const char*>(&aieResourcesBinHdr), sizeof(aie_resources_bin));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Image
  _buffer.write(reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionAIEResourcesBin::createDefaultImage(std::istream& _istream, std::ostringstream& _buffer) const
{
  XUtil::TRACE("AIE_RESOURCES_BIN-OBJ");

  aie_resources_bin aieResourcesBinHdr = aie_resources_bin{};
  std::ostringstream stringBlock;       // String block (stored immediately after the header)

  // Initialize default values
  {
    // Have all of the mpo (member, point, offset) values point to the zero length terminate string
    aieResourcesBinHdr.mpo_name = sizeof(aie_resources_bin) + stringBlock.tellp();
    stringBlock << getSectionIndexName() << '\0';

    uint32_t mpo_emptyChar = sizeof(aie_resources_bin) + stringBlock.tellp();
    stringBlock << '\0';

    aieResourcesBinHdr.mpo_version = mpo_emptyChar;
    aieResourcesBinHdr.m_start_column = mpo_emptyChar;
    aieResourcesBinHdr.m_num_columns = mpo_emptyChar;
  }

  // Initialize the object image values (last)
  {
    _istream.seekg(0, _istream.end);
    aieResourcesBinHdr.m_image_size = _istream.tellg();
    aieResourcesBinHdr.m_image_offset = sizeof(aie_resources_bin) + stringBlock.tellp();
  }

  XUtil::TRACE_BUF("aie_resources_bin", reinterpret_cast<const char*>(&aieResourcesBinHdr), sizeof(aie_resources_bin));

  // Write the header information
  _buffer.write(reinterpret_cast<const char*>(&aieResourcesBinHdr), sizeof(aie_resources_bin));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Write Data
  {
    std::unique_ptr<unsigned char[]> memBuffer(new unsigned char[aieResourcesBinHdr.m_image_size]);
    _istream.seekg(0);
    _istream.clear();
    _istream.read(reinterpret_cast<char*>(memBuffer.get()), aieResourcesBinHdr.m_image_size);

    _buffer.write(reinterpret_cast<const char*>(memBuffer.get()), aieResourcesBinHdr.m_image_size);
  }
}

// -------------------------------------------------------------------------

void
SectionAIEResourcesBin::readSubPayload(const char* _pOrigDataSection,
                                       unsigned int _origSectionSize,
                                  std::istream& _istream,
                                  const std::string& _sSubSectionName,
                                  Section::FormatType _eFormatType,
                                  std::ostringstream& _buffer) const
{
  // Determine the sub-section of interest
  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SubSection::obj:
      // Some basic DRC checks
      if (_pOrigDataSection != nullptr) {
        std::string errMsg = "ERROR: aie_resources_bin object image already exists.";
        throw std::runtime_error(errMsg);
      }

      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: aie_resources_bin object only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      createDefaultImage(_istream, _buffer);
      break;

    case SubSection::metadata: {
        // Some basic DRC checks
        if (_pOrigDataSection == nullptr) {
          std::string errMsg = "ERROR: Missing aie_resources_bin object image.  Add the AIE_RESOURCES_BIN-OBJ image prior to changing its metadata.";
          throw std::runtime_error(errMsg);
        }

        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: AIE_RESOURCES_BIN-METADATA only supports the JSON format.";
          throw std::runtime_error(errMsg);
        }

        copyBufferUpdateMetadata(_pOrigDataSection, _origSectionSize, _istream, _buffer);
      }
      break;

    case SubSection::unknown:
    default: {
        auto errMsg = boost::format("ERROR: Subsection '%s' not supported by section '%s") % _sSubSectionName % getSectionKindAsString();
        throw std::runtime_error(errMsg.str());
      }
      break;
  }
}

// -------------------------------------------------------------------------

void
SectionAIEResourcesBin::writeObjImage(std::ostream& _oStream) const
{
  XUtil::TRACE("SectionAIEResourcesBin::writeObjImage");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(aie_resources_bin)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the bmc structure (%d)") % m_bufferSize % sizeof(aie_resources_bin);
    throw std::runtime_error(errMsg.str());
  }

  // No look at the data
  auto pHdr = reinterpret_cast<aie_resources_bin*>(m_pBuffer);

  auto pFWBuffer = reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset;
  _oStream.write(pFWBuffer, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionAIEResourcesBin::writeMetadata(std::ostream& _oStream) const
{
  XUtil::TRACE("AIE_RESOURCES_BIN-METADATA");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(aie_resources_bin)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the aie_resources_bin structure (%d)") % m_bufferSize % sizeof(aie_resources_bin);
    throw std::runtime_error(errMsg.str());
  }

  auto pHdr = reinterpret_cast<aie_resources_bin*>(m_pBuffer);

  XUtil::TRACE(boost::format("Original: \n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  m_start_column (0x%lx): '%s'\n"
                             "  m_num_columns (0x%lx): '%s'")
               % pHdr->mpo_name % (reinterpret_cast<char*>(pHdr) + pHdr->mpo_name)
               % pHdr->m_image_offset % pHdr->m_image_size
               % pHdr->mpo_version % (reinterpret_cast<char*>(pHdr) + pHdr->mpo_version)
               % pHdr->m_start_column % (reinterpret_cast<char*>(pHdr) + pHdr->m_start_column)
               % pHdr->m_num_columns % (reinterpret_cast<char*>(pHdr) + pHdr->m_num_columns));

  // Convert the data from the binary format to JSON
  boost::property_tree::ptree ptAieResourcesBin;

  ptAieResourcesBin.put("name", reinterpret_cast<char*>(pHdr) + pHdr->mpo_name);
  ptAieResourcesBin.put("version", reinterpret_cast<char*>(pHdr) + pHdr->mpo_version);
  ptAieResourcesBin.put("start_column", reinterpret_cast<char*>(pHdr) + pHdr->m_start_column);
  ptAieResourcesBin.put("num_columns", reinterpret_cast<char*>(pHdr) + pHdr->m_num_columns);

  boost::property_tree::ptree root;
  root.put_child("aie_resources_bin_metadata", ptAieResourcesBin);

  boost::property_tree::write_json(_oStream, root);
}

// -------------------------------------------------------------------------

void
SectionAIEResourcesBin::writeSubPayload(const std::string& _sSubSectionName,
                                        FormatType _eFormatType,
                                        std::fstream&  _oStream) const
{
  // Some basic DRC checks
  if (m_pBuffer == nullptr) {
    std::string errMsg = "ERROR: aie_resources_bin section does not exist.";
    throw std::runtime_error(errMsg);
  }

  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SubSection::obj:
      // Some basic DRC checks
      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: AIE_RESOURCES_BIN-OBJ only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      writeObjImage(_oStream);
      break;

    case SubSection::metadata: {
        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: AIE_RESOURCES_BIN-METADATA only supports the JSON format.";
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
SectionAIEResourcesBin::readXclBinBinary(std::istream& _istream, const axlf_section_header& _sectionHeader)
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

  try {
    boost::property_tree::read_json(ss, pt);
  }

  catch (const boost::property_tree::json_parser_error& e) {
      (void)e;
      auto errMsg = boost::format("ERROR: Unable to parse  metadata file of section '%s'") % getSectionIndexName();
      throw std::runtime_error(errMsg.str());
  }

  boost::property_tree::ptree& ptAieResourcesBin = pt.get_child("aie_resources_bin_metadata");

  XUtil::TRACE_PrintTree("Current AIE_RESOURCES_BIN contents", pt);
  std::string sName = ptAieResourcesBin.get<std::string>("name");
  Section::m_sIndexName = sName;
}
