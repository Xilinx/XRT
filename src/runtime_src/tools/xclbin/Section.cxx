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

#include "Section.h"

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>


#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

// Static Variables Initialization
std::map<enum axlf_section_kind, std::string> Section::m_mapIdToName;
std::map<std::string, enum axlf_section_kind> Section::m_mapNameToId;
std::map<enum axlf_section_kind, Section::Section_factory> Section::m_mapIdToCtor;
std::map<std::string, enum axlf_section_kind> Section::m_mapJSONNameToKind;

Section::Section()
    : m_eKind(BITSTREAM)
    , m_sKindName("")
    , m_pBuffer(nullptr)
    , m_bufferSize(0)
    , m_name("") {
  // Empty
}

Section::~Section() {
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
Section::setName(const std::string &_sSectionName)
{
   m_name = _sSectionName;
}

void
Section::getKinds(std::vector< std::string > & kinds) {
  for (auto & item : m_mapNameToId) {
    kinds.push_back(item.first);
  }
}

void
Section::registerSectionCtor(enum axlf_section_kind _eKind,
                             const std::string& _sKindStr,
                             const std::string& _sHeaderJSONName,
                             Section_factory _Section_factory) {
  // Some error checking
  if (_sKindStr.empty()) {
    std::string errMsg = XUtil::format("Error: Kind (%d) pretty print name is missing.", _eKind);
    throw std::runtime_error(errMsg);
  }

  if (m_mapIdToName.find(_eKind) != m_mapIdToName.end()) {
    std::string errMsg = XUtil::format("Error: Attempting to register (%d : %s). Constructor enum of kind (%d) already registered.",
                                       (unsigned int)_eKind, _sKindStr.c_str(), (unsigned int)_eKind);
    throw std::runtime_error(errMsg);
  }

  if (m_mapNameToId.find(_sKindStr) != m_mapNameToId.end()) {
    std::string errMsg = XUtil::format("Error: Attempting to register: (%d : %s). Constructor name '%s' already registered to eKind (%d).",
                                       (unsigned int)_eKind, _sKindStr.c_str(),
                                       _sKindStr.c_str(), (unsigned int)m_mapNameToId[_sKindStr]);
    throw std::runtime_error(errMsg);
  }

  if (!_sHeaderJSONName.empty()) {
    if (m_mapJSONNameToKind.find(_sHeaderJSONName) != m_mapJSONNameToKind.end()) {
      std::string errMsg = XUtil::format("Error: Attempting to register: (%d : %s). JSON mapping name '%s' already registered to eKind (%d).",
                                         (unsigned int)_eKind, _sKindStr.c_str(),
                                         _sHeaderJSONName.c_str(), (unsigned int)m_mapJSONNameToKind[_sHeaderJSONName]);
      throw std::runtime_error(errMsg);
    }
    m_mapJSONNameToKind[_sHeaderJSONName] = _eKind;
  }

  
  // At this point we know we are good, lets initialize the arrays
  m_mapIdToName[_eKind] = _sKindStr;
  m_mapNameToId[_sKindStr] = _eKind;
  m_mapIdToCtor[_eKind] = _Section_factory;
  
  //std::cout << "Kind(" << _eKind << "): " << _sKindStr << std::endl;
}

bool
Section::translateSectionKindStrToKind(const std::string &_sKindStr, enum axlf_section_kind &_eKind)
{
  if (m_mapNameToId.find(_sKindStr) == m_mapNameToId.end()) {
    return false;   
  }
  _eKind = m_mapNameToId[_sKindStr];
  return true;
}


enum Section::FormatType 
Section::getFormatType(const std::string _sFormatType)
{
  std::string sFormatType = _sFormatType;

  boost::to_upper(sFormatType);

  if (sFormatType == "") { return FT_UNDEFINED; }
  if (sFormatType == "RAW") { return FT_RAW; }
  if (sFormatType == "JSON") { return FT_JSON; }
  if (sFormatType == "HTML") { return FT_HTML; }
  if (sFormatType == "TXT") { return FT_TXT; }
  
  return FT_UNKNOWN;
}

bool 
Section::getKindOfJSON(const std::string &_sJSONStr, enum axlf_section_kind &_eKind) {
  if (_sJSONStr.empty() ||
     (m_mapJSONNameToKind.find(_sJSONStr) == m_mapJSONNameToKind.end()) ) {
    return false;
  }

  _eKind = m_mapJSONNameToKind[_sJSONStr];
  return true;
}


Section*
Section::createSectionObjectOfKind(enum axlf_section_kind _eKind) {
  Section* pSection = nullptr;

  if (m_mapIdToCtor.find(_eKind) == m_mapIdToCtor.end()) {
    std::string errMsg = XUtil::format("Error: Constructor for enum (%d) is missing.", (unsigned int)_eKind);
    throw std::runtime_error(errMsg);
  }

  pSection = m_mapIdToCtor[_eKind]();
  pSection->m_eKind = _eKind;
  pSection->m_sKindName = m_mapIdToName[_eKind];

  XUtil::TRACE(XUtil::format("Created segment: %s (%d)",
                             pSection->getSectionKindAsString().c_str(),
                             (unsigned int)pSection->getSectionKind()));
  return pSection;
}


enum axlf_section_kind
Section::getSectionKind() const {
  return m_eKind;
}

const std::string&
Section::getSectionKindAsString() const {
  return m_sKindName;
}

std::string
Section::getName() const {
  return m_name;
}

unsigned int
Section::getSize() const {
  return m_bufferSize;
}

void
Section::initXclBinSectionHeader(axlf_section_header& _sectionHeader) {
  _sectionHeader.m_sectionKind = m_eKind;
  _sectionHeader.m_sectionSize = m_bufferSize;
  XUtil::safeStringCopy((char*)&_sectionHeader.m_sectionName, m_name, sizeof(axlf_section_header::m_sectionName));
}

void
Section::writeXclBinSectionBuffer(std::fstream& _ostream) const
{
  if ((m_pBuffer == nullptr) ||
      (m_bufferSize == 0)) {
    return;
  }

  _ostream.write(m_pBuffer, m_bufferSize);
}

void
Section::readXclBinBinary(std::fstream& _istream, const axlf_section_header& _sectionHeader) {
  // Some error checking
  if ((enum axlf_section_kind)_sectionHeader.m_sectionKind != getSectionKind()) {
    std::string errMsg = XUtil::format("Error: Unexpected section kind.  Expected: %d, Read: %d", getSectionKind(), _sectionHeader.m_sectionKind);
    throw std::runtime_error(errMsg);
  }

  if (m_pBuffer != nullptr) {
    std::string errMsg = "Error: Binary buffer already exists.";
    throw std::runtime_error(errMsg);
  }

  m_name = (char*)&_sectionHeader.m_sectionName;
  m_bufferSize = _sectionHeader.m_sectionSize;
  m_pBuffer = new char[m_bufferSize];

  _istream.seekg(_sectionHeader.m_sectionOffset);

  _istream.read(m_pBuffer, m_bufferSize);

  if (_istream.gcount() != m_bufferSize) {
    std::string errMsg = "ERROR: Input stream for the binary buffer is smaller then the expected size.";
    throw std::runtime_error(errMsg);
  }

  XUtil::TRACE(XUtil::format("Section: %s (%d)", getSectionKindAsString().c_str(), (unsigned int)getSectionKind()));
  XUtil::TRACE(XUtil::format("  m_name: %s", m_name.c_str()));
  XUtil::TRACE(XUtil::format("  m_size: %ld", m_bufferSize));
}


void 
Section::readJSONSectionImage(const boost::property_tree::ptree& _ptSection)
{
  std::ostringstream buffer;
  marshalFromJSON(_ptSection, buffer);

  // -- Read contents into memory buffer --
  m_bufferSize = buffer.tellp();
  m_pBuffer = new char[m_bufferSize];
  memcpy(m_pBuffer, buffer.str().c_str(), m_bufferSize);
}

void
Section::readXclBinBinary(std::fstream& _istream,
                          const boost::property_tree::ptree& _ptSection) {
  // Some error checking
  enum axlf_section_kind eKind = (enum axlf_section_kind)_ptSection.get<unsigned int>("Kind");

  if (eKind != getSectionKind()) {
    std::string errMsg = XUtil::format("Error: Unexpected section kind.  Expected: %d, Read: %d", getSectionKind(), eKind);
  }

  if (m_pBuffer != nullptr) {
    std::string errMsg = "Error: Binary buffer already exists.";
    throw std::runtime_error(errMsg);
  }

  m_name = _ptSection.get<std::string>("Name");


  boost::optional<const boost::property_tree::ptree&> ptPayload = _ptSection.get_child_optional("payload");

  if (ptPayload.is_initialized()) {
    XUtil::TRACE(XUtil::format("Reading in the section '%s' (%d) via metadata.", getSectionKindAsString().c_str(), (unsigned int)getSectionKind()));
    readJSONSectionImage(ptPayload.get());
  } else {
    // We don't initialize the buffer via any metadata.  Just read in the section as is
    XUtil::TRACE(XUtil::format("Reading in the section '%s' (%d) as a image.", getSectionKindAsString().c_str(), (unsigned int)getSectionKind()));
    m_bufferSize =  XUtil::stringToUInt64(_ptSection.get<std::string>("Size"));
    m_pBuffer = new char[m_bufferSize];

    unsigned int offset = XUtil::stringToUInt64(_ptSection.get<std::string>("Offset"));

    _istream.seekg(offset);
    _istream.read(m_pBuffer, m_bufferSize);

    if (_istream.gcount() != m_bufferSize) {
      std::string errMsg = "ERROR: Input stream for the binary buffer is smaller then the expected size.";
      throw std::runtime_error(errMsg);
    }
  }

  XUtil::TRACE(XUtil::format("Adding Section: %s (%d)", getSectionKindAsString().c_str(), (unsigned int)getSectionKind()));
  XUtil::TRACE(XUtil::format("  m_name: %s", m_name.c_str()));
  XUtil::TRACE(XUtil::format("  m_size: %ld", m_bufferSize));
}


void
Section::addMirrorPayload(boost::property_tree::ptree& _pt) const {
  marshalToJSON(m_pBuffer, m_bufferSize, _pt);
}

void
Section::marshalToJSON(char* _pDataSegment,
                       unsigned int _segmentSize,
                       boost::property_tree::ptree& _ptree) const {
  // Do nothing
}


void
Section::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                         std::ostringstream& _buf) const {
  XUtil::TRACE_PrintTree("Payload", _ptSection);
  std::string errMsg = XUtil::format("Error: Section '%s' (%d) missing payload parser.", getSectionKindAsString().c_str(), (unsigned int)getSectionKind());
  throw std::runtime_error(errMsg);
}


void 
Section::readXclBinBinary(std::fstream& _istream, enum FormatType _eFormatType)
{
  switch (_eFormatType) {
  case FT_RAW:
    {
      axlf_section_header sectionHeader = (axlf_section_header){ 0 };
      sectionHeader.m_sectionKind = getSectionKind();
      sectionHeader.m_sectionOffset = 0;
      _istream.seekg(0, _istream.end);
      sectionHeader.m_sectionSize = _istream.tellg();

      readXclBinBinary(_istream, sectionHeader);
      break;
    }
  case FT_JSON:
    {
      // Bring the file into memory
      _istream.seekg(0, _istream.end);
      unsigned int fileSize = _istream.tellg();

      std::unique_ptr<unsigned char> memBuffer(new unsigned char[fileSize]);
      _istream.clear();
      _istream.seekg(0);
      _istream.read((char*)memBuffer.get(), fileSize);

      XUtil::TRACE_BUF("Buffer", (char*)memBuffer.get(), fileSize);

      // Convert the JSON file to a boost property tree
      std::stringstream ss((char*)memBuffer.get());

      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      readXclBinBinary(_istream, pt);
      break;
    }
  case FT_HTML:
    // Do nothing
    break;
  case FT_TXT:
    // Do nothing
    break;
  case FT_UNKNOWN:
    // Do nothing
    break;
  case FT_UNDEFINED:
    // Do nothing
    break;
  }
}


void 
Section::dumpContents(std::fstream& _ostream, enum FormatType _eFormatType) const
{
  switch (_eFormatType) {
  case FT_RAW:
    {
      writeXclBinSectionBuffer(_ostream);
      break;
    }
  case FT_JSON:
    {
      boost::property_tree::ptree pt;
      marshalToJSON(m_pBuffer, m_bufferSize, pt);

      boost::property_tree::write_json(_ostream, pt, true /*Pretty print*/);
      break;
    }
  case FT_HTML:
    {
      boost::property_tree::ptree pt;
      marshalToJSON(m_pBuffer, m_bufferSize, pt);

      _ostream << XUtil::format("<!DOCTYPE html><html><body><h1>Section: %s (%d)</h1><pre>", getSectionKindAsString().c_str(), getSectionKind()) << std::endl;
      boost::property_tree::write_json(_ostream, pt, true /*Pretty print*/);
      _ostream << "</pre></body></html>" << std::endl;
      break;
    }
  case FT_UNKNOWN:
    // Do nothing;
    break;
  case FT_TXT:
    // Do nothing;
    break;
  case FT_UNDEFINED:
    // Do nothing;
    break;
  }
}
