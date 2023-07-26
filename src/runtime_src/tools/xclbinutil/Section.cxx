/**
 * Copyright (C) 2018 - 2022 Xilinx, Inc
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

#include "Section.h"

#include "XclBinUtilities.h"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <sstream>

namespace XUtil = XclBinUtilities;

// Disable windows compiler warnings
#ifdef _WIN32
  #pragma warning( disable : 4100)      // 4100 - Unreferenced formal parameter
#endif


Section::SectionInfo::SectionInfo(axlf_section_kind eKind,
                                  std::string sectionName,
                                  Section_factory sectionCtor)
    : eKind(eKind)
    , name(std::move(sectionName))
    , sectionCtor(sectionCtor)
    , nodeName("")
    , supportsSubSections(false)
    , supportsIndexing(false)
{
  // Empty
}

// Singleton collection of sections
std::vector<std::unique_ptr<Section::SectionInfo>>&
Section::getSectionTypes()
{
  static std::vector<std::unique_ptr<SectionInfo>> sections;
  return sections;
}

Section::Section()
    : m_eKind(BITSTREAM)
    , m_sKindName("")
    , m_sIndexName("")
    , m_pBuffer(nullptr)
    , m_bufferSize(0)
    , m_name("")
{
  // Empty
}

Section::~Section()
{
  purgeBuffers();
}

void
Section::purgeBuffers()
{
  if (m_pBuffer != nullptr) {
    delete m_pBuffer;
    m_pBuffer = nullptr;
  }
  m_bufferSize = 0;
}

void
Section::setName(const std::string& _sSectionName)
{
  m_name = _sSectionName;
}

std::vector<std::string>
Section::getSupportedKinds()
{
  std::vector<std::string> supportedKinds;

  for (auto& item : getSectionTypes())
    supportedKinds.push_back(item->name);

  return supportedKinds;
}


void
Section::addSectionType(std::unique_ptr<SectionInfo> sectionInfo)
{
  // Some error checking
  if (sectionInfo->name.empty()) {
    auto errMsg = boost::format("ERROR: CMD name for the section kind (%d) is empty. This needs to be defined.") % sectionInfo->eKind;
    throw std::runtime_error(errMsg.str());
  }

  // Get the collection of sections
  auto& sections = getSectionTypes();

  // Is the enumeration type already registered
  if (std::any_of(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == sectionInfo->eKind;})) {
    auto errMsg = boost::format("ERROR: Attempting to register (%d : %s). Constructor enum of kind (%d) already registered.")
        % sectionInfo->eKind % sectionInfo->name % sectionInfo->eKind;
    throw std::runtime_error(errMsg.str());
  }

  // Is the cmd name already registered
  {
    auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->name == sectionInfo->name;});
    if (iter != sections.end()) {
      auto errMsg = boost::format("ERROR: Attempting to register: (%d : %s). Constructor name '%s' already registered to eKind (%d).")
          % sectionInfo->eKind % sectionInfo->name
          % sectionInfo->name % iter->get()->eKind;
      throw std::runtime_error(errMsg.str());
    }
  }

  // Is the header name already registered
  if (!sectionInfo->nodeName.empty()) {
    auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->nodeName == sectionInfo->nodeName;});
    if (iter != sections.end()) {
      auto errMsg = boost::format("ERROR: Attempting to register: (%d : %s). JSON mapping name '%s' already registered to eKind (%d).")
          % sectionInfo->eKind % sectionInfo->name
          % sectionInfo->nodeName % iter->get()->eKind;
      throw std::runtime_error(errMsg.str());
    }
  }

  sections.push_back(std::move(sectionInfo));
}

void
Section::translateSectionKindStrToKind(const std::string& sectionName,
                                       axlf_section_kind& eKind)
{
  auto& sections = getSectionTypes();
  auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->name == sectionName;});
  if (iter == sections.end()) {
    auto errMsg = boost::format("ERROR: Section '%s' isn't a valid section name.") % sectionName;
    throw std::runtime_error(errMsg.str());
  }
  eKind = iter->get()->eKind;
}

bool
Section::supportsSubSections(axlf_section_kind& eKind)
{
  auto& sections = getSectionTypes();

  auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == eKind;});

  if (iter == sections.end()) {
    auto errMsg = boost::format("ERROR: The section kind value '%d' does not exist.") % eKind;
    throw std::runtime_error(errMsg.str());
  }

  return iter->get()->supportsSubSections;
}

bool
Section::supportsSectionIndex(axlf_section_kind& eKind)
{
  auto& sections = getSectionTypes();

  auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == eKind;});

  if (iter == sections.end()) {
    auto errMsg = boost::format("ERROR: The section kind value '%d' does not exist.") % eKind;
    throw std::runtime_error(errMsg.str());
  }

  return iter->get()->supportsIndexing;
}

bool
Section::supportsSubSectionName(axlf_section_kind eKind, const std::string& sSubSectionName)
{
  auto& sections = getSectionTypes();
  auto sectionIter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == eKind;});

  if (sectionIter == sections.end())
    return false;

  auto subSections = sectionIter->get()->subSections;

  return std::any_of(subSections.begin(), subSections.end(), [&](const auto& entry) {return boost::iequals(entry, sSubSectionName);});
}


// -------------------------------------------------------------------------

const std::string&
Section::getSectionIndexName() const
{
  return m_sIndexName;
}

static const std::vector<std::pair<std::string, Section::FormatType>> formatTypeTable = {
  { "",    Section::FormatType::undefined },
  { "RAW",  Section::FormatType::raw },
  { "JSON", Section::FormatType::json },
  { "HTML", Section::FormatType::html },
  { "TXT",  Section::FormatType::txt }
};

Section::FormatType
Section::getFormatType(const std::string& sFormatType)
{
  auto iter = std::find_if(formatTypeTable.begin(), formatTypeTable.end(), [&](const auto& entry) {return boost::iequals(entry.first, sFormatType);});
  if (iter == formatTypeTable.end())
    return FormatType::undefined;

  return iter->second;
}

axlf_section_kind
Section::getKindOfJSON(const std::string& nodeName)
{
  auto& sections = getSectionTypes();
  auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->nodeName == nodeName;});

  if (iter == sections.end()) {
    auto errMsg = boost::format("ERROR: Node name '%s' does not map to a given section type.") % nodeName;
    throw std::runtime_error(errMsg.str());
  }

  return iter->get()->eKind;
}

std::string
Section::getJSONOfKind(axlf_section_kind eKind)
{
  auto& sections = getSectionTypes();
  auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == eKind;});

  if (iter == sections.end()) {
    auto errMsg = boost::format("ERROR: The given enum kind (%d) does not exist.") % eKind;
    throw std::runtime_error(errMsg.str());
  }

  return iter->get()->nodeName;
}

Section*
Section::createSectionObjectOfKind(axlf_section_kind eKind,
                                   const std::string sIndexName)
{
  Section* pSection = nullptr;

  auto& sections = getSectionTypes();
  auto iter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == eKind;});

  if (iter == sections.end()) {
    auto errMsg = boost::format("ERROR: Section constructor for the archive section ID '%d' does not exist.  This error is most likely the result of examining a newer version of an archive image than this version of software supports.") % eKind;
    throw std::runtime_error(errMsg.str());
  }

  pSection = iter->get()->sectionCtor();
  pSection->m_eKind = eKind;
  pSection->m_sKindName = iter->get()->name;
  pSection->m_sIndexName = sIndexName;

  XUtil::TRACE(boost::format("Created segment: %s (%d), index: '%s'")
               % pSection->getSectionKindAsString()
               % pSection->getSectionKind()
               % pSection->getSectionIndexName());
  return pSection;
}


axlf_section_kind
Section::getSectionKind() const
{
  return m_eKind;
}

const std::string&
Section::getSectionKindAsString() const
{
  return m_sKindName;
}

std::string
Section::getName() const
{
  return m_name;
}

unsigned int
Section::getSize() const
{
  return m_bufferSize;
}

void
Section::initXclBinSectionHeader(axlf_section_header& _sectionHeader)
{
  _sectionHeader.m_sectionKind = m_eKind;
  _sectionHeader.m_sectionSize = m_bufferSize;
  XUtil::safeStringCopy((char*)&_sectionHeader.m_sectionName, m_name, sizeof(axlf_section_header::m_sectionName));
}

void
Section::writeXclBinSectionBuffer(std::ostream& _ostream) const
{
  if ((m_pBuffer == nullptr) ||
      (m_bufferSize == 0)) {
    return;
  }

  _ostream.write(m_pBuffer, m_bufferSize);
  _ostream.flush();
}

void
Section::readXclBinBinary(std::istream& _istream, const axlf_section_header& _sectionHeader)
{
  // Some error checking
  XUtil::TRACE("ReadXclBinBinary...");
  if ((axlf_section_kind)_sectionHeader.m_sectionKind != getSectionKind()) {
    auto errMsg = boost::format("ERROR: Unexpected section kind.  Expected: %d, Read: %d") % getSectionKind() % _sectionHeader.m_sectionKind;
    throw std::runtime_error(errMsg.str());
  }

  if (m_pBuffer != nullptr) {
    std::string errMsg = "ERROR: Binary buffer already exists.";
    throw std::runtime_error(errMsg);
  }

  m_name = (char*)&_sectionHeader.m_sectionName;

  if (_sectionHeader.m_sectionSize > UINT64_MAX) {
    std::string errMsg("FATAL ERROR: Section header size exceeds internal representation size.");
    throw std::runtime_error(errMsg);
  }

  m_bufferSize = (unsigned int)_sectionHeader.m_sectionSize;

  m_pBuffer = new char[m_bufferSize];

  _istream.seekg(_sectionHeader.m_sectionOffset);

  _istream.read(m_pBuffer, m_bufferSize);

  if (_istream.gcount() != (std::streamsize)m_bufferSize) {
    std::string errMsg = "ERROR: Input stream for the binary buffer is smaller then the expected size.";
    throw std::runtime_error(errMsg);
  }

  XUtil::TRACE(boost::format("Section: %s (%d)") % getSectionKindAsString() % (unsigned int)getSectionKind());
  XUtil::TRACE(boost::format("  m_name: %s") % m_name);
  XUtil::TRACE(boost::format("  m_size: %ld") % m_bufferSize);
}


void
Section::readJSONSectionImage(const boost::property_tree::ptree& _ptSection)
{
  std::ostringstream buffer;
  marshalFromJSON(_ptSection, buffer);

  // Release the buffer memory and reset the size to zero
  purgeBuffers();

  // -- Read contents into memory buffer --
  m_bufferSize = (unsigned int)buffer.tellp();

  if (m_bufferSize == 0) {
    auto errMsg = boost::format("WARNING: Section '%s' content is empty.  No data in the given JSON file.") % getSectionKindAsString();
    std::cout << errMsg << std::endl;
    return;
  }

  m_pBuffer = new char[m_bufferSize];
  memcpy(m_pBuffer, buffer.str().c_str(), m_bufferSize);
}

void
Section::readXclBinBinary(std::istream& _istream,
                          const boost::property_tree::ptree& _ptSection)
{
  // Some error checking
  axlf_section_kind eKind = (axlf_section_kind)_ptSection.get<unsigned int>("Kind");

  if (eKind != getSectionKind()) {
    auto errMsg = boost::format("ERROR: Unexpected section kind.  Expected: %d, Read: %d") % (unsigned int)getSectionKind() % (unsigned int)eKind;
    throw std::runtime_error(errMsg.str());
  }

  if (m_pBuffer != nullptr) {
    auto errMsg = "ERROR: Binary buffer already exists.";
    throw std::runtime_error(errMsg);
  }

  m_name = _ptSection.get<std::string>("Name");


  boost::optional<const boost::property_tree::ptree&> ptPayload = _ptSection.get_child_optional("payload");

  if (ptPayload.is_initialized()) {
    XUtil::TRACE(boost::format("Reading in the section '%s' (%d) via metadata.") % getSectionKindAsString() % (unsigned int)getSectionKind());
    readJSONSectionImage(ptPayload.get());
  } else {
    // We don't initialize the buffer via any metadata.  Just read in the section as is
    XUtil::TRACE(boost::format("Reading in the section '%s' (%d) as a image.") % getSectionKindAsString() % (unsigned int)getSectionKind());

    uint64_t imageSize = XUtil::stringToUInt64(_ptSection.get<std::string>("Size"));
    if (imageSize > UINT64_MAX) {
      std::string errMsg("FATAL ERROR: Image size exceeds internal representation size.");
      throw std::runtime_error(errMsg);
    }

    m_bufferSize = (unsigned int)imageSize;
    m_pBuffer = new char[m_bufferSize];

    uint64_t offset = XUtil::stringToUInt64(_ptSection.get<std::string>("Offset"));

    _istream.seekg(offset);
    _istream.read(m_pBuffer, m_bufferSize);

    if (_istream.gcount() != (std::streamsize)m_bufferSize) {
      std::string errMsg = "ERROR: Input stream for the binary buffer is smaller then the expected size.";
      throw std::runtime_error(errMsg);
    }
  }

  XUtil::TRACE(boost::format("Adding Section: %s (%d)") % getSectionKindAsString() % (unsigned int)getSectionKind());
  XUtil::TRACE(boost::format("  m_name: %s") % m_name);
  XUtil::TRACE(boost::format("  m_size: %ld") % m_bufferSize);
}


void
Section::getPayload(boost::property_tree::ptree& _pt) const
{
  marshalToJSON(m_pBuffer, m_bufferSize, _pt);
}

void
Section::marshalToJSON(char* _pDataSegment,
                       unsigned int _segmentSize,
                       boost::property_tree::ptree& _ptree) const
{
  // Do nothing
}



void
Section::appendToSectionMetadata(const boost::property_tree::ptree& _ptAppendData,
                                 boost::property_tree::ptree& _ptToAppendTo)
{
  std::string errMsg = "ERROR: The Section '" + getSectionKindAsString() + "' does not support appending metadata";
  throw std::runtime_error(errMsg);
}


void
Section::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                         std::ostringstream& _buf) const
{
  XUtil::TRACE_PrintTree("Payload", _ptSection);
  auto errMsg = boost::format("ERROR: Section '%s' (%d) missing payload parser.") % getSectionKindAsString() % (unsigned int)getSectionKind();
  throw std::runtime_error(errMsg.str());
}

void
Section::setPathAndName(const std::string& _pathAndName)
{
  m_pathAndName = _pathAndName;
}

const std::string&
Section::getPathAndName() const
{
  return m_pathAndName;
}


void
Section::readPayload(std::istream& _istream, FormatType _eFormatType)
{
  switch (_eFormatType) {
    case FormatType::raw: {
        axlf_section_header sectionHeader = axlf_section_header{};
        sectionHeader.m_sectionKind = getSectionKind();
        sectionHeader.m_sectionOffset = 0;
        _istream.seekg(0, _istream.end);

        static_assert(sizeof(std::streamsize) <= sizeof(uint64_t), "std::streamsize precision is greater then 64 bits");
        sectionHeader.m_sectionSize = (uint64_t)_istream.tellg();

        readXclBinBinary(_istream, sectionHeader);
        break;
      }
    case FormatType::json: {
        // Bring the file into memory
        _istream.seekg(0, _istream.end);
        std::streamsize fileSize =  _istream.tellg();

        std::unique_ptr<unsigned char[]> memBuffer(new unsigned char[fileSize]);
        _istream.clear();
        _istream.seekg(0);
        _istream.read((char*)memBuffer.get(), fileSize);

        XUtil::TRACE_BUF("Buffer", (char*)memBuffer.get(), fileSize);

        // Convert the JSON file to a boost property tree
        std::stringstream ss;
        ss.write((char*)memBuffer.get(), fileSize);

        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        // O.K. - Lint checking is done and write it to our buffer
        try {
          readJSONSectionImage(pt);
        } catch (const std::exception& e) {
          std::cerr << "\nERROR: An exception was thrown while attempting to add following JSON image to the section: '" << getSectionKindAsString() << "'\n";
          std::cerr << "       Exception Message: " << e.what() << "\n";
          std::ostringstream jsonBuf;
          boost::property_tree::write_json(jsonBuf, pt, true);
          std::cerr << jsonBuf.str() << "\n";
          throw std::runtime_error("Aborting remaining operations");
        }
        break;
      }
    case FormatType::html:
      // Do nothing
      break;
    case FormatType::txt:
      // Do nothing
      break;
    case FormatType::unknown:
      // Do nothing
      break;
    case FormatType::undefined:
      // Do nothing
      break;
  }
}

void
Section::dumpContents(std::ostream& _ostream, FormatType _eFormatType) const
{
  switch (_eFormatType) {
    case FormatType::raw: {
        writeXclBinSectionBuffer(_ostream);
        break;
      }
    case FormatType::json: {
        boost::property_tree::ptree pt;
        marshalToJSON(m_pBuffer, m_bufferSize, pt);

        boost::property_tree::write_json(_ostream, pt, true /*Pretty print*/);
        break;
      }
    case FormatType::html: {
        boost::property_tree::ptree pt;
        marshalToJSON(m_pBuffer, m_bufferSize, pt);

        _ostream << boost::format("<!DOCTYPE html><html><body><h1>Section: %s (%d)</h1><pre>\n") % getSectionKindAsString() % (unsigned int)getSectionKind();
        boost::property_tree::write_json(_ostream, pt, true /*Pretty print*/);
        _ostream << "</pre></body></html>\n";
        break;
      }
    case FormatType::unknown:
      // Do nothing;
      break;
    case FormatType::txt:
      // Do nothing;
      break;
    case FormatType::undefined:
      break;
  }
}

void
Section::dumpSubSection(std::fstream& _ostream,
                        std::string _sSubSection,
                        FormatType _eFormatType) const
{
  writeSubPayload(_sSubSection, _eFormatType, _ostream);
}


void
Section::printHeader(std::ostream& _ostream) const
{
  _ostream << "Section Header\n";
  _ostream << boost::format("  Type    : '%s'\n") % getSectionKindAsString();
  _ostream << boost::format("  Name    : '%s'\n") % getName();
  _ostream << boost::format("  Size    : '%d'\n") % getSize();
}

bool
Section::doesSupportAddFormatType(axlf_section_kind eKind, FormatType eFormatType)
{
  auto& sections = getSectionTypes();

  auto sectionIter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == eKind;});
  if (sectionIter == sections.end())
    return false;

  auto& addFormats = sectionIter->get()->supportedAddFormats;
  return std::any_of(addFormats.begin(), addFormats.end(), [&](const auto& entry) {return entry == eFormatType;});
}

bool
Section::doesSupportDumpFormatType(axlf_section_kind eKind, FormatType eFormatType)
{
  auto& sections = getSectionTypes();

  auto sectionIter = std::find_if(sections.begin(), sections.end(), [&](const auto& entry) {return entry->eKind == eKind;});
  if (sectionIter == sections.end())
    return false;

  auto& addFormats = sectionIter->get()->supportedDumpFormats;
  return std::any_of(addFormats.begin(), addFormats.end(), [&](const auto& entry) {return entry == eFormatType;});
}

bool
Section::getSubPayload(std::ostringstream& _buf,
                       const std::string& _sSubSection,
                       Section::FormatType _eFormatType) const
{
  // Make sure we support this subsection
  if (supportsSubSectionName(m_eKind, _sSubSection) == false) {
    return false;
  }

  // Make sure we support the format type
  if (_eFormatType != FormatType::raw) {
    return false;
  }

  // All is good now get the data from the section
  getSubPayload(m_pBuffer, m_bufferSize, _buf, _sSubSection, _eFormatType);

  if ((long)_buf.tellp() == 0) {
    return false;
  }

  return true;
}

void
Section::getSubPayload(char* _pDataSection,
                       unsigned int _sectionSize,
                       std::ostringstream& _buf,
                       const std::string& _sSubSection,
                       Section::FormatType _eFormatType) const
{
  // Empty
}

void
Section::readSubPayload(std::istream& _istream,
                        const std::string& _sSubSection,
                        Section::FormatType _eFormatType)
{
  // Make sure we support this subsection
  if (supportsSubSectionName(m_eKind, _sSubSection) == false) {
    return;
  }

  // All is good now get the data from the section
  std::ostringstream buffer;
  readSubPayload(m_pBuffer, m_bufferSize, _istream, _sSubSection, _eFormatType, buffer);

  // Now for some how cleaning
  if (m_pBuffer != nullptr) {
    delete m_pBuffer;
    m_pBuffer = nullptr;
    m_bufferSize = 0;
  }

  m_bufferSize = (unsigned int)buffer.tellp();

  if (m_bufferSize == 0) {
    auto errMsg = boost::format("WARNING: Section '%s' content is empty.") % getSectionKindAsString();
    throw std::runtime_error(errMsg.str());
  }

  m_pBuffer = new char[m_bufferSize];
  memcpy(m_pBuffer, buffer.str().c_str(), m_bufferSize);
}

void
Section::readSubPayload(const char* _pOrigDataSection,
                        unsigned int _origSectionSize,
                        std::istream& _istream,
                        const std::string& _sSubSection,
                        Section::FormatType _eFormatType,
                        std::ostringstream& _buffer) const
{
  auto errMsg = boost::format("FATAL ERROR: Section '%s' virtual method readSubPayLoad() not defined.") % getSectionKindAsString();
  throw std::runtime_error(errMsg.str());
}

bool
Section::subSectionExists(const std::string& _sSubSectionName) const
{
  return false;
}

void
Section::writeSubPayload(const std::string& _sSubSectionName,
                         FormatType _eFormatType,
                         std::fstream&  _oStream) const
{
  auto errMsg = boost::format("FATAL ERROR: Section '%s' virtual method writeSubPayload() not defined.") % getSectionKindAsString();
  throw std::runtime_error(errMsg.str());
}

