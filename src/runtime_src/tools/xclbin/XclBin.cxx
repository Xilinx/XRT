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

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "XclBin.h"

#include <stdexcept>
#include <boost/property_tree/json_parser.hpp>

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;


static const std::string MIRROR_DATA_START = "XCLBIN_MIRROR_DATA_START";
static const std::string MIRROR_DATA_END = "XCLBIN_MIRROR_DATA_END";


XclBin::XclBin()
    : m_xclBinHeader({ 0 })
    , m_SchemaVersionMirrorWrite({ 1, 0, 0 }) {
  // Empty
}

XclBin::~XclBin() {
  for (size_t index = 0; index < m_sections.size(); index++) {
    delete m_sections[index];
  }
  m_sections.clear();
}


void
XclBin::addSection(Section* _pSection) {
  // Error check
  if (_pSection == nullptr) {
    return;
  }

  m_sections.push_back(_pSection);
  m_xclBinHeader.m_header.m_numSections = m_sections.size();
}

void
XclBin::readXclBinBinaryHeader(std::fstream& _istream) {
  // Read in the buffer
  const unsigned int expectBufferSize = sizeof(axlf);

  _istream.seekg(0);
  _istream.read((char*)&m_xclBinHeader, sizeof(axlf));

  if (_istream.gcount() != expectBufferSize) {
    std::string errMsg = "ERROR: Input stream is smaller then the expected header size.";
    throw std::runtime_error(errMsg);
  }

  // TODO: Check for magic value being correct
}

void
XclBin::readXclBinBinarySections(std::fstream& _istream) {
  // Read in each section
  unsigned int numberOfSections = m_xclBinHeader.m_header.m_numSections;

  for (unsigned int index = 0; index < numberOfSections; ++index) {
    XUtil::TRACE(XUtil::format("Examining Section: %d of %d", index + 1, m_xclBinHeader.m_header.m_numSections));
    // Find the section header data
    long long sectionOffset = sizeof(axlf) + (index * sizeof(axlf_section_header)) - sizeof(axlf_section_header);
    _istream.seekg(sectionOffset);

    // Read in the section header
    axlf_section_header sectionHeader = (axlf_section_header){ 0 };
    const unsigned int expectBufferSize = sizeof(axlf_section_header);

    _istream.read((char*)&sectionHeader, sizeof(axlf_section_header));

    if (_istream.gcount() != expectBufferSize) {
      std::string errMsg = "ERROR: Input stream is smaller then the expected section header size.";
      throw std::runtime_error(errMsg);
    }

    Section* pSection = Section::createSectionObjectOfKind((enum axlf_section_kind)sectionHeader.m_sectionKind);

    // Here for testing purposes, when all segments are supported it should be removed
    if (pSection != nullptr) {
      pSection->readXclBinBinary(_istream, sectionHeader);
      addSection(pSection);
    }
  }
}

void
XclBin::readXclBinBinary(const std::string _binaryFileName,
                         bool _bMigrate) {
  // Error checks
  if (_binaryFileName.empty()) {
    std::string errMsg = "ERROR: Missing file name to read from.";
    throw std::runtime_error(errMsg);
  }

  // Open the file for consumption
  XUtil::TRACE("Reading xclbin binary file: " + _binaryFileName);
  std::fstream ifXclBin;
  ifXclBin.open(_binaryFileName, std::ifstream::in | std::ifstream::binary);
  if (!ifXclBin.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _binaryFileName;
    throw std::runtime_error(errMsg);
  }

  if (_bMigrate) {
    boost::property_tree::ptree pt_mirrorData;
    findAndReadMirrorData(ifXclBin, pt_mirrorData);

    // Read in the mirror image
    readXclBinaryMirrorImage(ifXclBin, pt_mirrorData);
  } else {
    // Read in the header
    readXclBinBinaryHeader(ifXclBin);

    // Read the sections
    readXclBinBinarySections(ifXclBin);
  }

  ifXclBin.close();
}

void
XclBin::addHeaderMirrorData(boost::property_tree::ptree& _pt_header) {
  struct axlf* pHeader = &m_xclBinHeader;

  XUtil::TRACE("Creating Header Mirror ptree");
  std::string tmpStr;

  // Axlf structure
  {
    _pt_header.put("Magic", XUtil::format("%s", pHeader->m_magic).c_str());
    XUtil::binaryBufferToHexString(pHeader->m_cipher, sizeof(axlf::m_cipher), tmpStr);
    _pt_header.put("Cipher", tmpStr.c_str());
    XUtil::binaryBufferToHexString(pHeader->m_keyBlock, sizeof(axlf::m_keyBlock), tmpStr);
    _pt_header.put("KeyBlock", tmpStr.c_str());
    _pt_header.put("UniqueID", XUtil::format("0x%lx", pHeader->m_uniqueId).c_str());
  }

  // Axlf_header structure
  {
    _pt_header.put("TimeStamp", XUtil::format("%ld", pHeader->m_header.m_timeStamp).c_str());
    _pt_header.put("FeatureRomTimeStamp", XUtil::format("%ld", pHeader->m_header.m_featureRomTimeStamp).c_str());
    _pt_header.put("Version", XUtil::format("%d", pHeader->m_header.m_version).c_str());
    _pt_header.put("Mode", XUtil::format("%d", pHeader->m_header.m_mode).c_str());
    XUtil::binaryBufferToHexString(pHeader->m_header.rom_uuid, sizeof(axlf_header::rom_uuid), tmpStr);
    _pt_header.put("FeatureRomUUID", tmpStr.c_str());
    _pt_header.put("PlatformVBNV", XUtil::format("%s", pHeader->m_header.m_platformVBNV).c_str());
    XUtil::binaryBufferToHexString((unsigned char*)&(pHeader->m_header.uuid), sizeof(axlf_header::uuid), tmpStr);
    _pt_header.put("XclBinUUID", tmpStr.c_str());
    _pt_header.put("DebugBin", XUtil::format("%s", pHeader->m_header.m_debug_bin).c_str());
  }
}


void
XclBin::writeXclBinBinaryHeader(std::fstream& _ostream, boost::property_tree::ptree& _mirroredData) {
  // Write the header (minus the section header array)
  XUtil::TRACE("Writing xclbin binary header");
  _ostream.write((char*)&m_xclBinHeader, sizeof(axlf) - sizeof(axlf_section_header));

  // Get mirror data
  boost::property_tree::ptree pt_header;
  addHeaderMirrorData(pt_header);

  _mirroredData.add_child("header", pt_header);
}


void
XclBin::writeXclBinBinarySections(std::fstream& _ostream, boost::property_tree::ptree& _mirroredData) {
  // Nothing to write
  if (m_sections.empty()) {
    return;
  }

  // Prepare the array
  struct axlf_section_header sectionHeader[m_sections.size()] = { 0 };

  // Populate the array size and offsets
  unsigned int currentOffset = sizeof(axlf) - sizeof(axlf_section_header) + sizeof(sectionHeader);
  for (unsigned int index = 0; index < m_sections.size(); ++index) {
    // Calculate padding
    currentOffset += XUtil::bytesToAlign(currentOffset);

    // Initialize section header
    m_sections[index]->initXclBinSectionHeader(sectionHeader[index]);
    sectionHeader[index].m_sectionOffset = currentOffset;
    currentOffset += sectionHeader[index].m_sectionSize;
  }

  XUtil::TRACE("Writing xclbin section header array");
  _ostream.write((char*)&sectionHeader, sizeof(axlf_section_header) * m_sections.size());

  // Write out each of the sections
  for (unsigned int index = 0; index < m_sections.size(); ++index) {
    XUtil::TRACE(XUtil::format("Writing section: Index: %d, ID: %d", index, sectionHeader[index].m_sectionKind));

    // Align section to next 8 byte boundary
    unsigned int runningOffset = _ostream.tellp();
    unsigned int bytePadding = XUtil::bytesToAlign(runningOffset);
    if (bytePadding != 0) {
      static char holePack[] = { (char)0, (char)0, (char)0, (char)0, (char)0, (char)0, (char)0, (char)0 };
      _ostream.write(holePack, bytePadding);
    }
    runningOffset += bytePadding;

    // Check current and expected offsets
    if (runningOffset != sectionHeader[index].m_sectionOffset) {
      std::string errMsg = XUtil::format("Error: Expected offset (0x%lx) does not match actual (0x%lx)", sectionHeader[index].m_sectionOffset, runningOffset);
      throw std::runtime_error(errMsg);
    }

    // Write buffer
    m_sections[index]->writeXclBinSectionBuffer(_ostream);

    // Write mirror data
    {
      XUtil::TRACE("");
      XUtil::TRACE(XUtil::format("Adding mirror properties[%d]", index));

      boost::property_tree::ptree pt_sectionHeader;

      XUtil::TRACE(XUtil::format("Kind: %d, Name: %s, Offset: 0x%lx, Size: 0x%lx",
                                 sectionHeader[index].m_sectionKind,
                                 sectionHeader[index].m_sectionName,
                                 sectionHeader[index].m_sectionOffset,
                                 sectionHeader[index].m_sectionSize));

      pt_sectionHeader.put("Kind", XUtil::format("%d", sectionHeader[index].m_sectionKind).c_str());
      pt_sectionHeader.put("Name", XUtil::format("%s", sectionHeader[index].m_sectionName).c_str());
      pt_sectionHeader.put("Offset", XUtil::format("0x%lx", sectionHeader[index].m_sectionOffset).c_str());
      pt_sectionHeader.put("Size", XUtil::format("0x%lx", sectionHeader[index].m_sectionSize).c_str());

      boost::property_tree::ptree pt_Payload;
      m_sections[index]->addMirrorPayload(pt_Payload);

      if (pt_Payload.size() != 0) {
        pt_sectionHeader.add_child("payload", pt_Payload);
      }

      _mirroredData.add_child("section_header", pt_sectionHeader);
    }
  }
}


void
XclBin::writeXclBinBinaryMirrorData(std::fstream& _ostream,
                                    const boost::property_tree::ptree& _mirroredData) const {
  _ostream << MIRROR_DATA_START;
  boost::property_tree::write_json(_ostream, _mirroredData, false /*Pretty print*/);
  _ostream << '\0';
  _ostream << MIRROR_DATA_END;


  XUtil::TRACE_PrintTree("Mirrored Data", _mirroredData);
}


void
XclBin::writeXclBinBinary(const std::string _binaryFileName) {
  // Error checks
  if (_binaryFileName.empty()) {
    std::string errMsg = "ERROR: Missing file name to write to.";
    throw std::runtime_error(errMsg);
  }

  // Write the xclbin file image
  XUtil::TRACE("Writing the xclbin binary file: " + _binaryFileName);
  std::fstream ofXclBin;
  ofXclBin.open(_binaryFileName, std::ifstream::out | std::ifstream::binary);
  if (!ofXclBin.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _binaryFileName;
    throw std::runtime_error(errMsg);
  }

  // Mirrored data
  boost::property_tree::ptree mirroredData;

  // Add Version information
  addPTreeSchemaVersion(mirroredData, m_SchemaVersionMirrorWrite);

  // Write in the header data
  writeXclBinBinaryHeader(ofXclBin, mirroredData);

  // Write the section array and sections
  writeXclBinBinarySections(ofXclBin, mirroredData);

  // Write out our mirror data
  writeXclBinBinaryMirrorData(ofXclBin, mirroredData);

  // TODO: Update header file length

  ofXclBin.close();

  XUtil::addCheckSumImage(_binaryFileName, CST_SDBM);
}


void
XclBin::addPTreeSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion const& _schemaVersion) {

  XUtil::TRACE("");
  XUtil::TRACE("Adding Versioning Properties");

  boost::property_tree::ptree pt_schemaVersion;

  XUtil::TRACE(XUtil::format("major: %d, minor: %d, patch: %d",
                             _schemaVersion.major,
                             _schemaVersion.minor,
                             _schemaVersion.patch));

  pt_schemaVersion.put("major", XUtil::format("%d", _schemaVersion.major).c_str());
  pt_schemaVersion.put("minor", XUtil::format("%d", _schemaVersion.minor).c_str());
  pt_schemaVersion.put("patch", XUtil::format("%d", _schemaVersion.patch).c_str());
  _pt.add_child("schema_version", pt_schemaVersion);
}


void
XclBin::getSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion& _schemaVersion) {
  XUtil::TRACE("SchemaVersion");

  _schemaVersion.major = _pt.get<unsigned int>("major");
  _schemaVersion.minor = _pt.get<unsigned int>("minor");
  _schemaVersion.patch = _pt.get<unsigned int>("patch");

  XUtil::TRACE(XUtil::format("major: %d, minor: %d, patch: %d",
                             _schemaVersion.major,
                             _schemaVersion.minor,
                             _schemaVersion.patch));
}

bool
findStringInStream(std::fstream& _istream, const std::string& _searchString, unsigned int& _foundOffset) {
  XUtil::TRACE(XUtil::format("Searching for: %s", _searchString.c_str()));
  _foundOffset = 0;
  unsigned int stringLength = _searchString.length();
  unsigned int matchIndex = 0;

  char aChar;
  while (_istream.get(aChar)) {
    ++_foundOffset;
    if (aChar == _searchString[matchIndex++]) {
      if (matchIndex == stringLength) {
        _foundOffset -= stringLength;
        return true;
      }
    } else {
      matchIndex = 0;
    }
  }
  return false;
}

void
XclBin::findAndReadMirrorData(std::fstream& _istream, boost::property_tree::ptree& _mirrorData) const {
  XUtil::TRACE("Searching for mirrored data...");

  // Find start of buffer
  _istream.seekg(0);
  unsigned int startOffset = 0;
  if (findStringInStream(_istream, MIRROR_DATA_START, startOffset) == true) {
    XUtil::TRACE(XUtil::format("Found MIRROR_DATA_START at offset: 0x%lx", startOffset));
    startOffset += MIRROR_DATA_START.length();
  }  else {
    std::string errMsg = "ERROR: Mirror backup data not found in given file.";
    throw std::runtime_error(errMsg);
  }

  // Find end of buffer (continue where we left off)
  _istream.seekg(startOffset);
  unsigned int bufferSize = 0;
  if (findStringInStream(_istream, MIRROR_DATA_END, bufferSize) == true) {
    XUtil::TRACE(XUtil::format("Found MIRROR_DATA_END.  Buffersize: 0x%lx", bufferSize));
  }  else {
    std::string errMsg = "ERROR: Mirror backup data not well formed in given file.";
    throw std::runtime_error(errMsg);
  }

  // Bring the mirror metadata into memory
  std::unique_ptr<unsigned char> memBuffer(new unsigned char[bufferSize]);
  _istream.clear();
  _istream.seekg(startOffset);
  _istream.read((char*)memBuffer.get(), bufferSize);

  XUtil::TRACE_BUF("Buffer", (char*)memBuffer.get(), bufferSize);

  // Convert the JSON file to a boost property tree
  std::stringstream ss((char*)memBuffer.get());

  // TODO: Catch the exception (if any) from this call and produce a nice message
  boost::property_tree::read_json(ss, _mirrorData);

  XUtil::TRACE_PrintTree("Mirror", _mirrorData);
}


void
XclBin::readXclBinHeader(const boost::property_tree::ptree& _ptHeader,
                         struct axlf& _axlfHeader) {
  XUtil::TRACE("Reading via JSON mirror xclbin header information.");
  XUtil::TRACE_PrintTree("Header Mirror Image", _ptHeader);

  // Clear the previous header information
  _axlfHeader = { 0 };

  std::string sMagic = _ptHeader.get<std::string>("Magic");
  XUtil::safeStringCopy((char*)&_axlfHeader.m_magic, sMagic, sizeof(axlf::m_magic));
  std::string sCipher = _ptHeader.get<std::string>("Cipher");
  XUtil::hexStringToBinaryBuffer(sCipher, (unsigned char*)&_axlfHeader.m_cipher, sizeof(axlf::m_cipher));
  std::string sKeyBlock = _ptHeader.get<std::string>("KeyBlock");
  XUtil::hexStringToBinaryBuffer(sKeyBlock, (unsigned char*)&_axlfHeader.m_keyBlock, sizeof(axlf::m_keyBlock));
  _axlfHeader.m_uniqueId = XUtil::stringToUInt64(_ptHeader.get<std::string>("UniqueID"));

  _axlfHeader.m_header.m_timeStamp = XUtil::stringToUInt64(_ptHeader.get<std::string>("TimeStamp"));
  _axlfHeader.m_header.m_featureRomTimeStamp = XUtil::stringToUInt64(_ptHeader.get<std::string>("FeatureRomTimeStamp"));
  _axlfHeader.m_header.m_version = _ptHeader.get<int>("Version");
  _axlfHeader.m_header.m_mode = _ptHeader.get<uint32_t>("Mode");

  std::string sFeatureRomUUID = _ptHeader.get<std::string>("FeatureRomUUID");
  XUtil::hexStringToBinaryBuffer(sFeatureRomUUID, (unsigned char*)&_axlfHeader.m_header.rom_uuid, sizeof(axlf_header::rom_uuid));
  std::string sPlatformVBNV = _ptHeader.get<std::string>("PlatformVBNV");
  XUtil::safeStringCopy((char*)&_axlfHeader.m_header.m_platformVBNV, sPlatformVBNV, sizeof(axlf_header::m_platformVBNV));
  std::string sXclBinUUID = _ptHeader.get<std::string>("XclBinUUID");
  XUtil::hexStringToBinaryBuffer(sXclBinUUID, (unsigned char*)&_axlfHeader.m_header.uuid, sizeof(axlf_header::uuid));

  std::string sDebugBin = _ptHeader.get<std::string>("DebugBin");
  XUtil::safeStringCopy((char*)&_axlfHeader.m_header.m_debug_bin, sDebugBin, sizeof(axlf_header::m_debug_bin));

  XUtil::TRACE("Done Reading via JSON mirror xclbin header information.");
}

void
XclBin::readXclBinSection(std::fstream& _istream,
                          const boost::property_tree::ptree& _ptSection) {
  enum axlf_section_kind eKind = (enum axlf_section_kind)_ptSection.get<unsigned int>("Kind");

  Section* pSection = Section::createSectionObjectOfKind(eKind);

  pSection->readXclBinBinary(_istream, _ptSection);
}



void
XclBin::readXclBinaryMirrorImage(std::fstream& _istream,
                                 const boost::property_tree::ptree& _mirrorData) {
  // Iterate over each entry
  for (boost::property_tree::ptree::const_iterator ptEntry = _mirrorData.begin();
       ptEntry != _mirrorData.end();
       ++ptEntry) {
    XUtil::TRACE("Processing: '" + ptEntry->first + "'");

    // ---------------------------------------------------------------------
    if (ptEntry->first == "schema_version") {
      XUtil::TRACE("Examining the xclbin version schema");
      // TODO: getSchemaVersion(ptSegment->second, schemaVersion);
      continue;
    }

    // ---------------------------------------------------------------------
    if (ptEntry->first == "header") {
      readXclBinHeader(ptEntry->second, m_xclBinHeader);
      continue;
    }

    // ---------------------------------------------------------------------
    if (ptEntry->first == "section_header") {
      readXclBinSection(_istream, ptEntry->second);
      continue;
    }
    XUtil::TRACE("Skipping unknown section: " + ptEntry->first);
  }
}




