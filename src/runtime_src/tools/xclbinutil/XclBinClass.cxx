/**
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
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
#include "XclBinClass.h"

#include "ElfUtilities.h"
#include "FormattedOutput.h"
#include "KernelUtilities.h"
#include "Section.h"
#include "version.h"                             // Generated include files
#include "XclBinUtilities.h"
#include <boost/algorithm/string.hpp>            // boost::split, is_any_of
#include <boost/property_tree/json_parser.hpp>
#include <random>                                // randomGen 
#include <stdlib.h>

// Constant data
static const std::string mirroDataStart("XCLBIN_MIRROR_DATA_START");
static const std::string mirrorDataEnd("XCLBIN_MIRROR_DATA_END");

namespace XUtil = XclBinUtilities;
namespace fs = std::filesystem;

static
bool getVersionMajorMinorPath(const char* _pVersion, uint8_t& _major, uint8_t& _minor, uint16_t& _patch)
{
  std::string sVersion(_pVersion);
  std::vector<std::string> tokens;
  boost::split(tokens, sVersion, boost::is_any_of("."));
  if (tokens.size() == 1) {
    _major = 0;
    _minor = 0;
    _patch = (uint16_t)std::stoi(tokens[0]);
    return true;
  }

  if (tokens.size() == 3) {
    _major = (uint8_t)std::stoi(tokens[0]);
    _minor = (uint8_t)std::stoi(tokens[1]);
    _patch = (uint16_t)std::stoi(tokens[2]);
    return true;
  }

  return false;
}

XclBin::XclBin()
    : m_xclBinHeader({ 0 })
    , m_SchemaVersionMirrorWrite({ 1, 0, 0 })
{
  initializeHeader(m_xclBinHeader);
}

XclBin::~XclBin()
{
  for (size_t index = 0; index < m_sections.size(); index++) {
    delete m_sections[index];
  }
  m_sections.clear();
}


void
XclBin::initializeHeader(axlf& _xclBinHeader)
{
  _xclBinHeader = { 0 };

  std::string sMagic = "xclbin2";
  XUtil::safeStringCopy(_xclBinHeader.m_magic, sMagic, sizeof(_xclBinHeader.m_magic));
  _xclBinHeader.m_signature_length = -1;  // Initialize to 0xFFs
  memset(_xclBinHeader.reserved, 0xFF, sizeof(_xclBinHeader.reserved));
  memset(_xclBinHeader.m_keyBlock, 0xFF, sizeof(_xclBinHeader.m_keyBlock));
  _xclBinHeader.m_uniqueId = time(nullptr);
  _xclBinHeader.m_header.m_timeStamp = time(nullptr);
  _xclBinHeader.m_header.m_actionMask = 0;

  // Now populate the version information
  getVersionMajorMinorPath(xrt_build_version,
                           _xclBinHeader.m_header.m_versionMajor,
                           _xclBinHeader.m_header.m_versionMinor,
                           _xclBinHeader.m_header.m_versionPatch);
}

void
XclBin::printSections(std::ostream& _ostream) const
{
  XUtil::TRACE("Printing Section Header(s)");
  for (const auto pSection : m_sections)
    pSection->printHeader(_ostream);
}

void
XclBin::readXclBinBinaryHeader(std::fstream& _istream)
{
  // Read in the buffer
  const unsigned int expectBufferSize = sizeof(axlf);

  _istream.seekg(0);
  _istream.read((char*)&m_xclBinHeader, sizeof(axlf));

  if (_istream.gcount() != expectBufferSize) {
    std::string errMsg = "ERROR: Input stream is smaller than the expected header size.";
    throw std::runtime_error(errMsg);
  }

  if (FormattedOutput::getMagicAsString(m_xclBinHeader).c_str() != std::string("xclbin2")) {
    std::string errMsg = "ERROR: The XCLBIN appears to be corrupted (header start key value is not what is expected).";
    throw std::runtime_error(errMsg);
  }
}

void
XclBin::readXclBinBinarySections(std::fstream& _istream)
{
  // Read in each section
  unsigned int numberOfSections = m_xclBinHeader.m_header.m_numSections;

  for (unsigned int index = 0; index < numberOfSections; ++index) {
    XUtil::TRACE(boost::format("Examining Section: %d of %d") % (index + 1) % m_xclBinHeader.m_header.m_numSections);
    // Find the section header data
    long long sectionOffset = sizeof(axlf) + (index * sizeof(axlf_section_header)) - sizeof(axlf_section_header);
    _istream.seekg(sectionOffset);

    // Read in the section header
    axlf_section_header sectionHeader = axlf_section_header{};
    const unsigned int expectBufferSize = sizeof(axlf_section_header);

    _istream.read((char*)&sectionHeader, sizeof(axlf_section_header));

    if (_istream.gcount() != expectBufferSize) {
      std::string errMsg = "ERROR: Input stream is smaller than the expected section header size.";
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
XclBin::readXclBinBinary(const std::string& _binaryFileName,
                         bool _bMigrate)
{
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

bool
XclBin::checkForValidSection()
{
  if (FormattedOutput::getXclBinUuidAsString(m_xclBinHeader) != "" &&
     FormattedOutput::getTimeStampAsString(m_xclBinHeader) != "" &&
     m_xclBinHeader.m_header.m_length != 0 &&
     m_xclBinHeader.m_header.m_numSections != 0)
    return true;

  return false;
}

bool
XclBin::checkForPlatformVbnv()
{
  if (FormattedOutput::getPlatformVbnvAsString(m_xclBinHeader) != "")
    return true;

  return false;
}

void
XclBin::addHeaderMirrorData(boost::property_tree::ptree& _pt_header)
{
  XUtil::TRACE("Creating Header Mirror ptree");

  // Axlf structure
  {
    _pt_header.put("Magic", FormattedOutput::getMagicAsString(m_xclBinHeader).c_str());
    _pt_header.put("SignatureLength", FormattedOutput::getSignatureLengthAsString(m_xclBinHeader).c_str());
    _pt_header.put("KeyBlock", FormattedOutput::getKeyBlockAsString(m_xclBinHeader).c_str());
    _pt_header.put("UniqueID", FormattedOutput::getUniqueIdAsString(m_xclBinHeader).c_str());
  }

  // Axlf_header structure
  {
    _pt_header.put("TimeStamp", FormattedOutput::getTimeStampAsString(m_xclBinHeader).c_str());
    _pt_header.put("FeatureRomTimeStamp", FormattedOutput::getFeatureRomTimeStampAsString(m_xclBinHeader).c_str());
    _pt_header.put("Version", FormattedOutput::getVersionAsString(m_xclBinHeader).c_str());
    _pt_header.put("Mode", FormattedOutput::getModeAsString(m_xclBinHeader).c_str());
    _pt_header.put("InterfaceUUID", FormattedOutput::getInterfaceUuidAsString(m_xclBinHeader).c_str());
    _pt_header.put("PlatformVBNV", FormattedOutput::getPlatformVbnvAsString(m_xclBinHeader).c_str());
    _pt_header.put("XclBinUUID", FormattedOutput::getXclBinUuidAsString(m_xclBinHeader).c_str());
    _pt_header.put("DebugBin", FormattedOutput::getDebugBinAsString(m_xclBinHeader).c_str());
  }
}


void
XclBin::writeXclBinBinaryHeader(std::ostream& _ostream, boost::property_tree::ptree& _mirroredData)
{
  // Write the header (minus the section header array)
  XUtil::TRACE("Writing xclbin binary header");
  _ostream.write((char*)&m_xclBinHeader, sizeof(axlf) - sizeof(axlf_section_header));
  _ostream.flush();

  // Get mirror data
  boost::property_tree::ptree pt_header;
  addHeaderMirrorData(pt_header);

  _mirroredData.add_child("header", pt_header);
}


void
XclBin::writeXclBinBinarySections(std::ostream& _ostream, boost::property_tree::ptree& _mirroredData)
{
  // Nothing to write
  if (m_sections.empty()) {
    return;
  }

  // Prepare the array
  struct axlf_section_header* sectionHeader = new struct axlf_section_header[m_sections.size()];
  memset(sectionHeader, 0, sizeof(struct axlf_section_header) * m_sections.size());  // Zero out memory

  // Populate the array size and offsets
  uint64_t currentOffset = (uint64_t)(sizeof(axlf) - sizeof(axlf_section_header) + (sizeof(axlf_section_header) * m_sections.size()));

  for (unsigned int index = 0; index < m_sections.size(); ++index) {
    // Calculate padding
    currentOffset += (uint64_t)XUtil::bytesToAlign(currentOffset);

    // Initialize section header
    m_sections[index]->initXclBinSectionHeader(sectionHeader[index]);
    sectionHeader[index].m_sectionOffset = currentOffset;
    currentOffset += (uint64_t)sectionHeader[index].m_sectionSize;
  }

  XUtil::TRACE("Writing xclbin section header array");
  _ostream.write((char*)sectionHeader, sizeof(axlf_section_header) * m_sections.size());
  _ostream.flush();

  // Write out each of the sections
  for (unsigned int index = 0; index < m_sections.size(); ++index) {
    XUtil::TRACE(boost::format("Writing section: Index: %d, ID: %d") % index % sectionHeader[index].m_sectionKind);

    // Align section to next 8 byte boundary
    unsigned int runningOffset = (unsigned int)_ostream.tellp();
    unsigned int bytePadding = XUtil::bytesToAlign(runningOffset);
    if (bytePadding != 0) {
      static const char holePack[] = { (char)0, (char)0, (char)0, (char)0, (char)0, (char)0, (char)0, (char)0 };
      _ostream.write(holePack, bytePadding);
      _ostream.flush();
    }
    runningOffset += bytePadding;

    // Check current and expected offsets
    if (runningOffset != sectionHeader[index].m_sectionOffset) {
      auto errMsg = boost::format("ERROR: Expected offset (0x%lx) does not match actual (0x%lx)") % sectionHeader[index].m_sectionOffset % runningOffset;
      throw std::runtime_error(errMsg.str());
    }

    // Write buffer
    m_sections[index]->writeXclBinSectionBuffer(_ostream);

    // Write mirror data
    {
      XUtil::TRACE("");
      XUtil::TRACE(boost::format("Adding mirror properties[%d]") % index);

      boost::property_tree::ptree pt_sectionHeader;

      XUtil::TRACE(boost::format("Kind: %d, Name: %s, Offset: 0x%lx, Size: 0x%lx")
                                 % sectionHeader[index].m_sectionKind
                                 % sectionHeader[index].m_sectionName
                                 % sectionHeader[index].m_sectionOffset
                                 % sectionHeader[index].m_sectionSize);

      pt_sectionHeader.put("Kind", (boost::format("%d") % sectionHeader[index].m_sectionKind).str());
      pt_sectionHeader.put("Name", (boost::format("%s") % sectionHeader[index].m_sectionName).str());
      pt_sectionHeader.put("Offset", (boost::format("0x%lx") % sectionHeader[index].m_sectionOffset).str());
      pt_sectionHeader.put("Size", (boost::format("0x%lx") % sectionHeader[index].m_sectionSize).str());

      boost::property_tree::ptree pt_Payload;

      if (Section::doesSupportAddFormatType(m_sections[index]->getSectionKind(), Section::FormatType::json) &&
          Section::doesSupportDumpFormatType(m_sections[index]->getSectionKind(), Section::FormatType::json)) {
        m_sections[index]->getPayload(pt_Payload);
      }

      if (pt_Payload.size() != 0) {
        pt_sectionHeader.add_child("payload", pt_Payload);
      }

      _mirroredData.add_child("section_header", pt_sectionHeader);
    }
  }

  delete[] sectionHeader;
}


void
XclBin::writeXclBinBinaryMirrorData(std::ostream& _ostream,
                                    const boost::property_tree::ptree& _mirroredData) const
{
  _ostream << mirroDataStart;
  boost::property_tree::write_json(_ostream, _mirroredData, false /*Pretty print*/);
  _ostream << mirrorDataEnd;

  XUtil::TRACE_PrintTree("Mirrored Data", _mirroredData);
}

void
XclBin::updateUUID()
{
  std::random_device device;
  std::mt19937_64 randomGen(device());

  // Create a 16 byte value
  std::stringstream uuidStream;
  uuidStream << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex << randomGen();
  uuidStream << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex << randomGen();

  XUtil::hexStringToBinaryBuffer(uuidStream.str(), m_xclBinHeader.m_header.uuid, sizeof(axlf_header::uuid));

  XUtil::TRACE(boost::format("Updated xclbin UUID to: '%s'") % uuidStream.str());
}

void
XclBin::writeXclBinBinary(const std::string& _binaryFileName,
                          bool _bSkipUUIDInsertion)
{
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
    std::string errMsg = "ERROR: Unable to open the file for writing: " + _binaryFileName;
    throw std::runtime_error(errMsg);
  }

  if (_bSkipUUIDInsertion) {
    XUtil::TRACE("Skipping xclbin's UUID insertion.");
  } else {
    updateUUID();
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

  // Update header file length
  {
    // Determine file size
    ofXclBin.seekg(0, ofXclBin.end);
    static_assert(sizeof(std::streamsize) <= sizeof(uint64_t), "std::streamsize precision is greater then 64 bits");
    std::streamsize streamSize = (std::streamsize)ofXclBin.tellg();

    // Update Header
    m_xclBinHeader.m_header.m_length = (uint64_t)streamSize;

    // Write out the header...again
    ofXclBin.seekg(0, ofXclBin.beg);
    boost::property_tree::ptree dummyData;
    writeXclBinBinaryHeader(ofXclBin, dummyData);
  }

  // Close file
  ofXclBin.close();

  XUtil::QUIET(boost::format("Successfully wrote (%ld bytes) to the output file: %s")
                             % m_xclBinHeader.m_header.m_length % _binaryFileName);
}


void
XclBin::addPTreeSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion const& _schemaVersion)
{

  XUtil::TRACE("");
  XUtil::TRACE("Adding Versioning Properties");

  boost::property_tree::ptree pt_schemaVersion;

  XUtil::TRACE(boost::format("major: %d, minor: %d, patch: %d")
                             % _schemaVersion.major
                             % _schemaVersion.minor
                             % _schemaVersion.patch);

  pt_schemaVersion.put("major", (boost::format("%d") % _schemaVersion.major).str());
  pt_schemaVersion.put("minor", (boost::format("%d") % _schemaVersion.minor).str());
  pt_schemaVersion.put("patch", (boost::format("%d") % _schemaVersion.patch).str());
  _pt.add_child("schema_version", pt_schemaVersion);
}


void
XclBin::getSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion& _schemaVersion)
{
  XUtil::TRACE("SchemaVersion");

  _schemaVersion.major = _pt.get<unsigned int>("major");
  _schemaVersion.minor = _pt.get<unsigned int>("minor");
  _schemaVersion.patch = _pt.get<unsigned int>("patch");

  XUtil::TRACE(boost::format("major: %d, minor: %d, patch: %d")
                             % _schemaVersion.major
                             % _schemaVersion.minor
                             % _schemaVersion.patch);
}

void
XclBin::findAndReadMirrorData(std::fstream& _istream, boost::property_tree::ptree& _mirrorData) const
{
  XUtil::TRACE("Searching for mirrored data...");

  // Find start of buffer
  _istream.seekg(0);
  unsigned int startOffset = 0;
  if (XUtil::findBytesInStream(_istream, mirroDataStart, startOffset) == true) {
    XUtil::TRACE(boost::format("Found MIRROR_DATA_START at offset: 0x%lx") % startOffset);
    startOffset += (unsigned int)mirroDataStart.length();
  }  else {
    std::string errMsg;
    errMsg  = "ERROR: Mirror backup data not found in given file.\n";
    errMsg += "       The given archive image does not contain any metadata to\n";
    errMsg += "       migrate the data image to the current format.\n";
    errMsg += "       The lack of metadata is usually the result of attempting\n";
    errMsg += "       to migrate a pre-2018.3 archive.";

    throw std::runtime_error(errMsg);
  }

  // Find end of buffer (continue where we left off)
  _istream.seekg(startOffset);
  unsigned int bufferSize = 0;
  if (XUtil::findBytesInStream(_istream, mirrorDataEnd, bufferSize) == true) {
    XUtil::TRACE(boost::format("Found MIRROR_DATA_END.  Buffersize: 0x%lx") % bufferSize);
  }  else {
    std::string errMsg = "ERROR: Mirror backup data not well formed in given file.";
    throw std::runtime_error(errMsg);
  }

  // Bring the mirror metadata into memory
  std::vector<unsigned char> memBuffer(bufferSize);
  _istream.clear();
  _istream.seekg(startOffset);
  _istream.read((char*)memBuffer.data(), bufferSize);

  XUtil::TRACE_BUF("Buffer", (char*)memBuffer.data(), bufferSize);

  // Convert the JSON file to a boost property tree
  std::stringstream ss;
  ss.write((char*)memBuffer.data(), bufferSize);

  try {
    boost::property_tree::read_json(ss, _mirrorData);
  } catch (const boost::property_tree::json_parser_error& e) {
    auto errMsg = boost::format("ERROR: Parsing mirror metadata in the xclbin archive on line %d: %s") % e.line() % e.message();
    throw std::runtime_error(errMsg.str());
  }

  XUtil::TRACE_PrintTree("Mirror", _mirrorData);
}


void
XclBin::readXclBinHeader(const boost::property_tree::ptree& _ptHeader,
                         struct axlf& _axlfHeader)
{
  XUtil::TRACE("Reading via JSON mirror xclbin header information.");
  XUtil::TRACE_PrintTree("Header Mirror Image", _ptHeader);

  // Clear the previous header information
  _axlfHeader = { 0 };

  auto sMagic = _ptHeader.get<std::string>("Magic");
  XUtil::safeStringCopy((char*)&_axlfHeader.m_magic, sMagic, sizeof(axlf::m_magic));
  _axlfHeader.m_signature_length = _ptHeader.get<int32_t>("SignatureLength", -1);
  auto sKeyBlock = _ptHeader.get<std::string>("KeyBlock");
  XUtil::hexStringToBinaryBuffer(sKeyBlock, (unsigned char*)&_axlfHeader.m_keyBlock, sizeof(axlf::m_keyBlock));
  _axlfHeader.m_uniqueId = XUtil::stringToUInt64(_ptHeader.get<std::string>("UniqueID"), true /*forceHex*/);

  _axlfHeader.m_header.m_timeStamp = XUtil::stringToUInt64(_ptHeader.get<std::string>("TimeStamp"));
  _axlfHeader.m_header.m_featureRomTimeStamp = XUtil::stringToUInt64(_ptHeader.get<std::string>("FeatureRomTimeStamp"));
  auto sVersion = _ptHeader.get<std::string>("Version");
  getVersionMajorMinorPath(sVersion.c_str(),
                           _axlfHeader.m_header.m_versionMajor,
                           _axlfHeader.m_header.m_versionMinor,
                           _axlfHeader.m_header.m_versionPatch);

  _axlfHeader.m_header.m_mode = _ptHeader.get<uint16_t>("Mode");

  auto sInterfaceUUID = _ptHeader.get<std::string>("InterfaceUUID");
  XUtil::hexStringToBinaryBuffer(sInterfaceUUID, (unsigned char*)&_axlfHeader.m_header.m_interface_uuid, sizeof(axlf_header::m_interface_uuid));
  auto sPlatformVBNV = _ptHeader.get<std::string>("PlatformVBNV");
  XUtil::safeStringCopy((char*)&_axlfHeader.m_header.m_platformVBNV,

                        sPlatformVBNV, sizeof(axlf_header::m_platformVBNV));
  auto sXclBinUUID = _ptHeader.get<std::string>("XclBinUUID");
  XUtil::hexStringToBinaryBuffer(sXclBinUUID, (unsigned char*)&_axlfHeader.m_header.uuid, sizeof(axlf_header::uuid));

  auto sDebugBin = _ptHeader.get<std::string>("DebugBin");
  XUtil::safeStringCopy((char*)&_axlfHeader.m_header.m_debug_bin, sDebugBin, sizeof(axlf_header::m_debug_bin));

  XUtil::TRACE("Done Reading via JSON mirror xclbin header information.");
}

void
XclBin::readXclBinSection(std::fstream& _istream,
                          const boost::property_tree::ptree& _ptSection)
{
  enum axlf_section_kind eKind = (enum axlf_section_kind)_ptSection.get<unsigned int>("Kind");

  Section* pSection = Section::createSectionObjectOfKind(eKind);

  pSection->readXclBinBinary(_istream, _ptSection);
  addSection(pSection);
}



void
XclBin::readXclBinaryMirrorImage(std::fstream& _istream,
                                 const boost::property_tree::ptree& _mirrorData)
{
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

void
XclBin::addSection(Section* _pSection)
{
  // Error check
  if (_pSection == nullptr) {
    return;
  }

  m_sections.push_back(_pSection);
  m_xclBinHeader.m_header.m_numSections = (uint32_t)m_sections.size();
}

void
XclBin::addReplaceSection(ParameterSectionData& _PSD)
{
  enum axlf_section_kind eKind;
  Section::translateSectionKindStrToKind(_PSD.getSectionName(), eKind);

  // Determine if the section exists, if so remove it
  const Section* pSection = findSection(eKind);
  if (pSection != nullptr)
    removeSection(_PSD.getSectionName());

  addSection(_PSD);
}

static void
readJSONFile(const std::string& filename, boost::property_tree::ptree& pt)
{
  // Initilize return variables
  pt.clear();

  // Open the file
  std::fstream fs;
  fs.open(filename, std::ifstream::in | std::ifstream::binary);
  if (!fs.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + filename;
    throw std::runtime_error(errMsg);
  }

  // Read in the JSON file
  try {
    boost::property_tree::read_json(fs, pt);
  } catch (const boost::property_tree::json_parser_error& e) {
    auto errMsg = boost::format("ERROR: Parsing the file '%s' on line %d: %s") % filename % e.line() % e.message();
    throw std::runtime_error(errMsg.str());
  }
}


void
XclBin::addMergeSection(ParameterSectionData& _PSD)
{
  enum axlf_section_kind eKind;
  Section::translateSectionKindStrToKind(_PSD.getSectionName(), eKind);

  if (_PSD.getFormatType() != Section::FormatType::json) {
    std::string errMsg = "ERROR: Adding or merging of sections are only supported with the JSON format.";
    throw std::runtime_error(errMsg);
  }

  // Determine if the section exists, in not, then add it.
  Section* pSection = findSection(eKind);
  if (pSection == nullptr) {
    addSection(_PSD);
    return;
  }

  // Section exists, then merge with it

  // Read in the JSON to merge
  boost::property_tree::ptree ptAll;
  readJSONFile(_PSD.getFile(), ptAll);

  // Find the section of interest
  const std::string jsonNodeName = Section::getJSONOfKind(eKind);
  const boost::property_tree::ptree ptEmpty;
  const boost::property_tree::ptree& ptMerge = ptAll.get_child(jsonNodeName, ptEmpty);

  if (ptMerge.empty()) {
    auto errMsg = boost::format("ERROR: Nothing to add for the section '%s'\n.Either the JSON node name '%s' is missing or the contents of this node is empty.")
                                % _PSD.getSectionName() %jsonNodeName;
    throw std::runtime_error(errMsg.str());
  }

  // Update the path where this file is coming from
  pSection->setPathAndName(_PSD.getFile());

  // Get the current section data
  boost::property_tree::ptree ptPayload;
  pSection->getPayload(ptPayload);

  // Merge the sections
  try {
    pSection->appendToSectionMetadata(ptMerge, ptPayload);
  } catch (const std::exception& e) {
    std::cerr << "\nERROR: An exception was thrown while attempting to merge the following JSON image to the section: '" << pSection->getSectionKindAsString() << "'\n";
    std::cerr << "       Exception Message: " << e.what() << "\n";
    std::ostringstream jsonBuf;
    boost::property_tree::write_json(jsonBuf, ptMerge, true);
    std::cerr << jsonBuf.str() << "\n";
    throw std::runtime_error("Aborting remaining operations");
  }

  // Store the resulting merger
  pSection->purgeBuffers();
  pSection->readJSONSectionImage(ptPayload);

  // Report our success
  XUtil::QUIET("");
  XUtil::QUIET(boost::format("Section: '%s'(%d) merged successfully with\nFile: '%s'")
                             % pSection->getSectionKindAsString()
                             % (unsigned int)  pSection->getSectionKind()
                             % _PSD.getFile());
}

void
XclBin::removeSection(const Section* _pSection)
{
  // Error check
  if (_pSection == nullptr) {
    return;
  }

  for (unsigned int index = 0; index < m_sections.size(); ++index) {
    if ((void*)m_sections[index] == (void*)_pSection) {
      XUtil::TRACE(boost::format("Removing and deleting section '%s' (%d).") % _pSection->getSectionKindAsString() % (unsigned int) _pSection->getSectionKind());
      m_sections.erase(m_sections.begin() + index);
      delete _pSection;
      m_xclBinHeader.m_header.m_numSections = (uint32_t)m_sections.size();
      return;
    }
  }

  auto errMsg = boost::format("ERROR: Section '%s' (%d) not found") % _pSection->getSectionKindAsString() % (unsigned int) _pSection->getSectionKind();
  throw XUtil::XclBinUtilException(xet_missing_section, errMsg.str());
}

Section*
XclBin::findSection(enum axlf_section_kind _eKind,
                    const std::string& _indexName) const
{
  for (auto& section : m_sections) {
    if (section->getSectionKind() != _eKind)
      continue;

    if (section->getSectionIndexName().compare(_indexName) == 0)
      return section;
  }
  return nullptr;
}

// make it more flexible to return multiple Section's with the same type
std::vector<Section*>
XclBin::findSection(enum axlf_section_kind _eKind,
                    bool _ignoreIndex,
                    const std::string& _indexName) const
{
  std::vector<Section*> vSections;
  for (auto& section : m_sections) {
    if (section->getSectionKind() != _eKind)
      continue;

    if (_ignoreIndex || section->getSectionIndexName().compare(_indexName) == 0)
      vSections.push_back(section);
  }
  return vSections;
}

void
XclBin::removeSection(const std::string& _sSectionToRemove)
{
  XUtil::TRACE("Removing Section: " + _sSectionToRemove);

  std::string sectionName = _sSectionToRemove;
  std::string sectionIndexName;

  // Extract the section index (if it is there)
  const std::string sectionIndexStartDelimiter = "[";
  const char sectionIndexEndDelimiter = ']';
  std::size_t sectionIndex =  _sSectionToRemove.find_first_of(sectionIndexStartDelimiter, 0);

  // Was the start index found?
  if (sectionIndex != std::string::npos) {
    // We need to have an end delimiter
    if (sectionIndexEndDelimiter != _sSectionToRemove.back()) {
      auto errMsg = boost::format("Error: Expected format <section>[<section_index>] when using a section index.  Received: %s.") % _sSectionToRemove;
      throw std::runtime_error(errMsg.str());
    }

    // Extract the index name
    sectionIndexName = _sSectionToRemove.substr(sectionIndex + 1);
    sectionIndexName.pop_back();  // Remove ']'

    // Extract the section name
    sectionName = _sSectionToRemove.substr(0, sectionIndex);
  }

  enum axlf_section_kind _eKind;

  Section::translateSectionKindStrToKind(sectionName, _eKind);

  if ((Section::supportsSectionIndex(_eKind) == true) &&
      (sectionIndexName.empty() && !Section::supportsSubSectionName(_eKind, ""))) {
    auto errMsg = boost::format("ERROR: Section '%s' can only be deleted with indexes.") % sectionName;
    throw std::runtime_error(errMsg.str());
  }

  if ((Section::supportsSectionIndex(_eKind) == false) &&
      (!sectionIndexName.empty())) {
    auto errMsg = boost::format("ERROR: Section '%s' cannot be deleted with index values (not supported).") % sectionName;
    throw std::runtime_error(errMsg.str());
  }

  const Section* pSection = findSection(_eKind, sectionIndexName);
  if (pSection == nullptr) {
    auto errMsg = boost::format("ERROR: Section '%s' is not part of the xclbin archive.") % _sSectionToRemove;
    throw XUtil::XclBinUtilException(xet_missing_section, errMsg.str());
  }

  removeSection(pSection);
  pSection = nullptr;

  std::string indexEntry;
  if (!sectionIndexName.empty()) {
    indexEntry = "[" + sectionIndexName + "]";
  }

  XUtil::QUIET("");
  XUtil::QUIET(boost::format("Section '%s%s'(%d) was successfully removed")
                             % _sSectionToRemove % indexEntry
                             % (unsigned int) _eKind);
}


void
XclBin::replaceSection(ParameterSectionData& _PSD)
{
  enum axlf_section_kind eKind;
  Section::translateSectionKindStrToKind(_PSD.getSectionName(), eKind);

  Section* pSection = findSection(eKind);
  if (pSection == nullptr) {
    auto errMsg = boost::format("ERROR: Section '%s' does not exist.") % _PSD.getSectionName();
    throw XUtil::XclBinUtilException(xet_missing_section, errMsg.str());
  }

  std::string sSectionFileName = _PSD.getFile();
  // Write the xclbin file image
  std::fstream iSectionFile;
  iSectionFile.open(sSectionFileName, std::ifstream::in | std::ifstream::binary);
  if (!iSectionFile.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + sSectionFileName;
    throw std::runtime_error(errMsg);
  }

  pSection->purgeBuffers();

  pSection->setPathAndName(sSectionFileName);
  pSection->readPayload(iSectionFile, _PSD.getFormatType());

  updateHeaderFromSection(pSection);

  fs::path p(sSectionFileName);
  std::string sBaseName = p.stem().string();
  pSection->setName(sBaseName);

  XUtil::TRACE(boost::format("Section '%s' (%d) successfully added.") % pSection->getSectionKindAsString() % (unsigned int)  pSection->getSectionKind());
  XUtil::QUIET("");
  XUtil::QUIET(boost::format("Section: '%s'(%d) was successfully added.\nSize   : %ld bytes\nFormat : %s\nFile   : '%s'")
                             % pSection->getSectionKindAsString() % (unsigned int)  pSection->getSectionKind()
                             % pSection->getSize()
                             % _PSD.getFormatTypeAsStr() % sSectionFileName);
}

void
XclBin::updateHeaderFromSection(Section* _pSection)
{
  if (_pSection == nullptr) {
    return;
  }

  if (_pSection->getSectionKind() == BUILD_METADATA) {
    boost::property_tree::ptree pt;
    _pSection->getPayload(pt);

    boost::property_tree::ptree ptDsa;
    ptDsa = pt.get_child("build_metadata.dsa");

    auto feature_roms = XUtil::as_vector<boost::property_tree::ptree>(ptDsa, "feature_roms");

    boost::property_tree::ptree featureRom;
    if (!feature_roms.empty()) {
      featureRom = feature_roms[0];
    }

    // Feature ROM Time Stamp
    m_xclBinHeader.m_header.m_featureRomTimeStamp = XUtil::stringToUInt64(featureRom.get<std::string>("timeSinceEpoch", "0"));

    // Feature ROM VBNV
    auto sPlatformVBNV = featureRom.get<std::string>("vbnvName", "");
    XUtil::safeStringCopy((char*)&m_xclBinHeader.m_header.m_platformVBNV, sPlatformVBNV, sizeof(axlf_header::m_platformVBNV));

    // Examine OLD names -- // This code can be removed AFTER v++ has been updated to use the new format
    {
      // Feature ROM Time Stamp
      if (m_xclBinHeader.m_header.m_featureRomTimeStamp == 0) {
        m_xclBinHeader.m_header.m_featureRomTimeStamp = XUtil::stringToUInt64(featureRom.get<std::string>("time_epoch", "0"));
      }

      // Feature ROM VBNV
      if (sPlatformVBNV.empty()) {
        sPlatformVBNV = featureRom.get<std::string>("vbnv_name", "");
        XUtil::safeStringCopy((char*)&m_xclBinHeader.m_header.m_platformVBNV, sPlatformVBNV, sizeof(axlf_header::m_platformVBNV));
      }
    }

    XUtil::TRACE_PrintTree("Build MetaData To Be examined", pt);
  }
}

void
XclBin::addSubSection(ParameterSectionData& _PSD)
{
  XUtil::TRACE("Add Sub-Section");

  // See if there is a subsection to add
  std::string sSubSection = _PSD.getSubSectionName();

  // Get the section kind
  enum axlf_section_kind eKind;
  Section::translateSectionKindStrToKind(_PSD.getSectionName(), eKind);

  // See if the section support sub-sections
  if (Section::supportsSubSections(eKind) == false) {
    auto errMsg = boost::format("ERROR: Section '%s' doesn't support sub sections.") % _PSD.getSectionName();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Determine if the section already exists
  Section* pSection = findSection(eKind, _PSD.getSectionIndexName());
  bool bNewSection = false;
  if (pSection != nullptr) {
    // Check to see if the subsection is supported
    if (Section::supportsSubSectionName(pSection->getSectionKind(), sSubSection) == false) {
      auto errMsg = boost::format("ERROR: Section '%s' does not support the subsection: '%s'") % pSection->getSectionKindAsString() % sSubSection;
      throw std::runtime_error(boost::str(errMsg));
    }

    // Check to see if this subsection exists, if so bail
    std::ostringstream buffer;
    if (pSection->subSectionExists(_PSD.getSubSectionName()) == true) {
      auto errMsg = boost::format("ERROR: Section '%s' subsection '%s' already exists") % pSection->getSectionKindAsString() % sSubSection;
      throw std::runtime_error(boost::str(errMsg));
    }
  } else {
    pSection = Section::createSectionObjectOfKind(eKind, _PSD.getSectionIndexName());
    bNewSection = true;

    // Check to see if the subsection is supported
    if (Section::supportsSubSectionName(pSection->getSectionKind(), sSubSection) == false) {
      auto errMsg = boost::format("ERROR: Section '%s' does not support the subsection: '%s'") % pSection->getSectionKindAsString() % sSubSection;
      throw std::runtime_error(boost::str(errMsg));
    }

    fs::path p(_PSD.getFile());
    std::string sBaseName = p.stem().string();
    pSection->setName(sBaseName);
  }

  // At this point we know we can add the subsection

  // Open the file to be read.
  std::string sSectionFileName = _PSD.getFile();
  std::fstream iSectionFile;
  iSectionFile.open(sSectionFileName, std::ifstream::in | std::ifstream::binary);
  if (!iSectionFile.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + sSectionFileName;
    throw std::runtime_error(errMsg);
  }

  // Read in the data
  pSection->setPathAndName(sSectionFileName);
  pSection->readSubPayload(iSectionFile, _PSD.getSubSectionName(), _PSD.getFormatType());

  // Clean-up
  if (bNewSection == true)
    addSection(pSection);

  std::string sSectionAddedName = pSection->getSectionKindAsString();

  XUtil::TRACE(boost::str(boost::format(
                          "Section '%s%s%s' (%d) successfully added.")
                          % sSectionAddedName
                          % (sSubSection.empty() ? "" : "-")
                          % sSubSection % (unsigned int)  pSection->getSectionKind()));
  std::string optionalIndex;
  if (!(pSection->getSectionIndexName().empty()))
    optionalIndex = (boost::format("[%s]") % pSection->getSectionIndexName()).str();

  XUtil::QUIET("");
  XUtil::QUIET(boost::str(boost::format(
                         "Section: '%s%s%s%s'(%d) was successfully added.\nSize   : %ld bytes\nFormat : %s\nFile   : '%s'")
                         % sSectionAddedName
                         % optionalIndex
                         % (sSubSection.empty() ? "" : "-")
                         % sSubSection.c_str() % (unsigned int) pSection->getSectionKind()
                         % pSection->getSize()
                         % _PSD.getFormatTypeAsStr() % sSectionFileName));
}


void
XclBin::addSection(ParameterSectionData& _PSD)
{
  XUtil::TRACE("Add Section");

  // Get the section kind
  enum axlf_section_kind eKind;
  Section::translateSectionKindStrToKind(_PSD.getSectionName(), eKind);

  // See if the user is attempting to add a sub-section
  {
    if (!_PSD.getSubSectionName().empty() ||       // A subsection name has been added
        Section::supportsSubSectionName(eKind, "")) {  // The section supports default empty subsection
      addSubSection(_PSD);
      return;
    }
  }

  // Open the file to be read.
  std::string sSectionFileName = _PSD.getFile();
  std::fstream iSectionFile;
  iSectionFile.open(sSectionFileName, std::ifstream::in | std::ifstream::binary);
  if (!iSectionFile.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + sSectionFileName;
    throw std::runtime_error(errMsg);
  }

  // Determine if the section already exists
  Section* pSection = findSection(eKind);
  if (pSection != nullptr) {
    auto errMsg = boost::format("ERROR: Section '%s' already exists.") % _PSD.getSectionName();
    throw std::runtime_error(boost::str(errMsg));
  }

  pSection = Section::createSectionObjectOfKind(eKind);

  // Check to see if the given format type is supported
  if (Section::doesSupportAddFormatType(pSection->getSectionKind(), _PSD.getFormatType()) == false) {
    auto errMsg = boost::format("ERROR: The %s section does not support reading the %s file type.")
                                % pSection->getSectionKindAsString()
                                % _PSD.getFormatTypeAsStr();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Read in the data
  pSection->setPathAndName(sSectionFileName);
  pSection->readPayload(iSectionFile, _PSD.getFormatType());

  // Post-cleanup
  fs::path p(sSectionFileName);
  std::string sBaseName = p.stem().string();
  pSection->setName(sBaseName);

  bool bAllowZeroSize = ((pSection->getSectionKind() == DEBUG_DATA)
                         && (_PSD.getFormatType() == Section::FormatType::raw));

  if ((!bAllowZeroSize) && (pSection->getSize() == 0)) {
    XUtil::QUIET("");
    XUtil::QUIET(boost::str(boost::format(
                            "Section: '%s'(%d) was empty.  No action taken.\nFormat : %s\nFile   : '%s'")
                             % pSection->getSectionKindAsString()
                             % (unsigned int) pSection->getSectionKind()
                             % _PSD.getFormatTypeAsStr() % sSectionFileName));
    delete pSection;
    pSection = nullptr;
    return;
  }

  addSection(pSection);
  updateHeaderFromSection(pSection);

  std::string sSectionAddedName = pSection->getSectionKindAsString();

  XUtil::TRACE(boost::str(boost::format("Section '%s' (%d) successfully added.") % sSectionAddedName % (unsigned int) pSection->getSectionKind()));
  XUtil::QUIET("");
  XUtil::QUIET(boost::str(boost::format(
                         "Section: '%s'(%d) was successfully added.\nSize   : %ld bytes\nFormat : %s\nFile   : '%s'")
                          % sSectionAddedName % (unsigned int) pSection->getSectionKind()
                          % pSection->getSize()
                          % _PSD.getFormatTypeAsStr() % sSectionFileName));
}


void
XclBin::addSections(ParameterSectionData& _PSD)
{
  if (!_PSD.getSectionName().empty()) {
    std::string errMsg = "ERROR: Section given for a wildcard JSON section add is not empty.";
    throw std::runtime_error(errMsg);
  }

  if (_PSD.getFormatType() != Section::FormatType::json) {
    auto errMsg = boost::format("ERROR: Expecting JSON format type, got '%s'.") % _PSD.getFormatTypeAsStr();
    throw std::runtime_error(errMsg.str());
  }

  std::string sJSONFileName = _PSD.getFile();

  std::fstream fs;
  fs.open(sJSONFileName, std::ifstream::in | std::ifstream::binary);
  if (!fs.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + sJSONFileName;
    throw std::runtime_error(errMsg);
  }

  //  Add a new element to the collection and parse the JSON file
  XUtil::TRACE("Reading JSON File: '" + sJSONFileName + '"');
  boost::property_tree::ptree pt;
  try {
    boost::property_tree::read_json(fs, pt);
  } catch (const boost::property_tree::json_parser_error& e) {
    auto errMsg = boost::format("ERROR: Parsing the file '%s' on line %d: %s") % sJSONFileName % e.line() % e.message();
    throw std::runtime_error(errMsg.str());
  }

  XUtil::TRACE("Examining the property tree from the JSON's file: '" + sJSONFileName + "'");
  XUtil::TRACE("Property Tree: Root");
  XUtil::TRACE_PrintTree("Root", pt);

  for (boost::property_tree::ptree::iterator ptSection = pt.begin(); ptSection != pt.end(); ++ptSection) {
    const std::string& sectionName = ptSection->first;
    if (sectionName == "schema_version") {
      XUtil::TRACE("Skipping: '" + sectionName + "'");
      continue;
    }

    XUtil::TRACE("Processing: '" + sectionName + "'");

    auto eKind = Section::getKindOfJSON(sectionName);    // Can throw

    Section* pSection = findSection(eKind);
    if (pSection != nullptr) {
      auto errMsg = boost::format("ERROR: Section '%s' already exists.") % pSection->getSectionKindAsString();
      throw std::runtime_error(errMsg.str());
    }

    pSection = Section::createSectionObjectOfKind(eKind);
    try {
      pSection->readJSONSectionImage(pt);
    } catch (const std::exception& e) {
      std::cerr << "\nERROR: An exception was thrown while attempting to add following JSON image to the section: '" << pSection->getSectionKindAsString() << "'\n";
      std::cerr << "       Exception Message: " << e.what() << "\n";
      std::ostringstream jsonBuf;
      boost::property_tree::write_json(jsonBuf, pt, true);
      std::cerr << jsonBuf.str() << "\n";
      throw std::runtime_error("Aborting remaining operations");
    }

    if (pSection->getSize() == 0) {
      XUtil::QUIET("");
      XUtil::QUIET(boost::format("Section: '%s'(%d) was empty.  No action taken.\nFormat : %s\nFile   : '%s'")
                                 % pSection->getSectionKindAsString()
                                 % (unsigned int) pSection->getSectionKind()
                                 % _PSD.getFormatTypeAsStr() % sectionName);
      delete pSection;
      pSection = nullptr;
      continue;
    }
    addSection(pSection);
    updateHeaderFromSection(pSection);
    XUtil::TRACE(boost::format("Section '%s' (%d) successfully added.") % pSection->getSectionKindAsString() % (unsigned int) pSection->getSectionKind());
    XUtil::QUIET("");
    XUtil::QUIET(boost::format("Section: '%s'(%d) was successfully added.\nFormat : %s\nFile   : '%s'")
                               % pSection->getSectionKindAsString()
                               % (unsigned int) pSection->getSectionKind()
                               % _PSD.getFormatTypeAsStr() % sectionName);
  }
}

void
XclBin::appendSections(ParameterSectionData& _PSD)
{
  if (!_PSD.getSectionName().empty()) {
    std::string errMsg = "ERROR: Section given for a wildcard JSON section add is not empty.";
    throw std::runtime_error(errMsg);
  }

  if (_PSD.getFormatType() != Section::FormatType::json) {
    auto errMsg = boost::format("ERROR: Expecting JSON format type, got '%s'.") % _PSD.getFormatTypeAsStr();
    throw std::runtime_error(errMsg.str());
  }

  // Read in the boost property tree
  boost::property_tree::ptree pt;
  const std::string sJSONFileName = _PSD.getFile();
  readJSONFile(sJSONFileName, pt);

  XUtil::TRACE("Examining the property tree from the JSON's file: '" + sJSONFileName + "'");
  XUtil::TRACE("Property Tree: Root");
  XUtil::TRACE_PrintTree("Root", pt);

  for (boost::property_tree::ptree::iterator ptSection = pt.begin(); ptSection != pt.end(); ++ptSection) {
    const std::string& sectionName = ptSection->first;
    if (sectionName == "schema_version") {
      XUtil::TRACE("Skipping: '" + sectionName + "'");
      continue;
    }

    XUtil::TRACE("Processing: '" + sectionName + "'");

    auto eKind = Section::getKindOfJSON(sectionName);  // Can throw

    Section* pSection = findSection(eKind);

    if (pSection == nullptr) {
      Section* pTempSection = Section::createSectionObjectOfKind(eKind);

      if ((eKind == PARTITION_METADATA) ||
          (eKind == IP_LAYOUT)) {
        pSection = Section::createSectionObjectOfKind(eKind);
        addSection(pSection);
      } else {
        auto errMsg = boost::format("ERROR: Section '%s' doesn't exists for JSON key '%s'.  Must have an existing section in order to append.") % pTempSection->getSectionKindAsString() % sectionName;
        throw std::runtime_error(errMsg.str());
      }
    }

    boost::property_tree::ptree ptPayload;
    pSection->getPayload(ptPayload);

    try {
      pSection->appendToSectionMetadata(ptSection->second, ptPayload);
    } catch (const std::exception& e) {
      std::cerr << "\nERROR: An exception was thrown while attempting to append the following JSON image to the section: '" << pSection->getSectionKindAsString() << "'\n";
      std::cerr << "       Exception Message: " << e.what() << std::endl;
      std::ostringstream jsonBuf;
      boost::property_tree::write_json(jsonBuf, ptSection->second, true);
      std::cerr << jsonBuf.str() << "\n";
      throw std::runtime_error("Aborting remaining operations");
    }

    pSection->purgeBuffers();
    pSection->readJSONSectionImage(ptPayload);


    XUtil::TRACE(boost::format("Section '%s' (%d) successfully appended to.") % pSection->getSectionKindAsString() % (unsigned int) pSection->getSectionKind());
    XUtil::QUIET("");
    XUtil::QUIET(boost::format("Section: '%s'(%d) was successfully appended to.\nFormat : %s\nFile   : '%s'")
                               % pSection->getSectionKindAsString()
                               % (unsigned int) pSection->getSectionKind()
                               % _PSD.getFormatTypeAsStr() % sectionName);
  }
}

void
XclBin::dumpSubSection(ParameterSectionData& _PSD)
{
  XUtil::TRACE("Dump Sub-Section");

  std::string sSubSection = _PSD.getSubSectionName();
  // Get the section kind
  enum axlf_section_kind eKind;
  Section::translateSectionKindStrToKind(_PSD.getSectionName(), eKind);

  // See if the section support sub-sections
  if (Section::supportsSubSections(eKind) == false) {
    auto errMsg = boost::format("ERROR: Section '%s' doesn't support sub sections.") % _PSD.getSectionName();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Determine if the section exists
  Section* pSection = findSection(eKind, _PSD.getSectionIndexName());
  if (pSection == nullptr) {
    auto errMsg = boost::format("ERROR: Section %s[%s] does not exist.") % _PSD.getSectionName() % _PSD.getSectionIndexName();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Check to see if the subsection is supported
  if (Section::supportsSubSectionName(pSection->getSectionKind(), sSubSection) == false) {
    auto errMsg = boost::format("ERROR: Section '%s' does not support the subsection: '%s'") % pSection->getSectionKindAsString() % sSubSection;
    throw std::runtime_error(boost::str(errMsg));
  }

  // Check to see if this subsection exists
  std::ostringstream buffer;
  if (pSection->subSectionExists(_PSD.getSubSectionName()) == false) {
    auto errMsg = boost::format("ERROR: Section '%s' subsection '%s' doesn't exists") % pSection->getSectionKindAsString() % sSubSection;
    throw std::runtime_error(boost::str(errMsg));
  }

  // At this point we know we can dump the subsection
  std::string sDumpFileName = _PSD.getFile();
  // Write the xclbin file image
  std::fstream oDumpFile;
  oDumpFile.open(sDumpFileName, std::ifstream::out | std::ifstream::binary);
  if (!oDumpFile.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for writing: " + sDumpFileName;
    throw std::runtime_error(errMsg);
  }

  pSection->setPathAndName(sDumpFileName);
  pSection->dumpSubSection(oDumpFile, sSubSection, _PSD.getFormatType());

  XUtil::TRACE(boost::format("Section '%s' (%d) dumped.") % pSection->getSectionKindAsString() % (unsigned int) pSection->getSectionKind());
  XUtil::QUIET("");

  std::string optionalIndex;
  if (!(pSection->getSectionIndexName().empty()))
    optionalIndex = boost::str(boost::format("[%s]") % pSection->getSectionIndexName());

  XUtil::QUIET(boost::format("Section: '%s%s%s%s'(%d) was successfully written.\nFormat : %s\nFile   : '%s'")
                             % pSection->getSectionKindAsString()
                             % optionalIndex
                             % (sSubSection.empty() ? "" : "-")
                             % sSubSection % (unsigned int) pSection->getSectionKind()
                             % _PSD.getFormatTypeAsStr() % sDumpFileName);
}


void
XclBin::dumpSection(ParameterSectionData& _PSD)
{
  XUtil::TRACE("Dump Section");

  enum axlf_section_kind eKind;
  Section::translateSectionKindStrToKind(_PSD.getSectionName(), eKind);

  // See if the user is attempting to dump a sub-section
  {
    if (!_PSD.getSubSectionName().empty() ||           // A subsection name has been added
        Section::supportsSubSectionName(eKind, "")) {  // The section supports default empty subsection
      dumpSubSection(_PSD);
      return;
    }
  }

  Section* pSection = findSection(eKind);
  if (pSection == nullptr) {
    auto errMsg = boost::format("ERROR: Section '%s' does not exists.") % _PSD.getSectionName();
    throw XUtil::XclBinUtilException(xet_missing_section, boost::str(errMsg));
  }

  if (_PSD.getFormatType() == Section::FormatType::unknown) {
    std::string errMsg = "ERROR: Unknown format type '" + _PSD.getFormatTypeAsStr() + "' in the dump section option: '" + _PSD.getOriginalFormattedString() + "'";
    throw std::runtime_error(errMsg);
  }

  if (_PSD.getFormatType() == Section::FormatType::undefined) {
    std::string errMsg = "ERROR: The format type is missing from the dump section option: '" + _PSD.getOriginalFormattedString() + "'.  Expected: <SECTION>:<FORMAT>:<OUTPUT_FILE>.  See help for more format details.";
    throw std::runtime_error(errMsg);
  }

  if (Section::doesSupportDumpFormatType(pSection->getSectionKind(), _PSD.getFormatType()) == false) {
    auto errMsg = boost::format("ERROR: The %s section does not support writing to a %s file type.")
                                % pSection->getSectionKindAsString()
                                % _PSD.getFormatTypeAsStr();
    throw std::runtime_error(boost::str(errMsg));
  }

  std::string sDumpFileName = _PSD.getFile();
  // Write the xclbin file image
  std::fstream oDumpFile;
  oDumpFile.open(sDumpFileName, std::ifstream::out | std::ifstream::binary);
  if (!oDumpFile.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for writing: " + sDumpFileName;
    throw std::runtime_error(errMsg);
  }

  pSection->setPathAndName(sDumpFileName);
  pSection->dumpContents(oDumpFile, _PSD.getFormatType());

  XUtil::TRACE(boost::format("Section '%s' (%d) dumped.") % pSection->getSectionKindAsString() % (unsigned int) pSection->getSectionKind());
  XUtil::QUIET("");
  XUtil::QUIET(boost::format("Section: '%s'(%d) was successfully written.\nFormat: %s\nFile  : '%s'")
                             % pSection->getSectionKindAsString()
                             % (unsigned int) pSection->getSectionKind()
                             % _PSD.getFormatTypeAsStr() % sDumpFileName);
}

void
XclBin::dumpSections(ParameterSectionData& _PSD)
{
  if (!_PSD.getSectionName().empty()) {
    std::string errMsg = "ERROR: Section given for a wildcard JSON section to dump is not empty.";
    throw std::runtime_error(errMsg);
  }

  if (_PSD.getFormatType() != Section::FormatType::json) {
    auto errMsg = boost::format("ERROR: Expecting JSON format type, got '%s'.") % _PSD.getFormatTypeAsStr();
    throw std::runtime_error(errMsg.str());
  }

  std::string sDumpFileName = _PSD.getFile();
  // Write the xclbin file image
  std::fstream oDumpFile;
  oDumpFile.open(sDumpFileName, std::ifstream::out | std::ifstream::binary);
  if (!oDumpFile.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for writing: " + sDumpFileName;
    throw std::runtime_error(errMsg);
  }

  switch (_PSD.getFormatType()) {
    case Section::FormatType::json: {
        boost::property_tree::ptree pt;
        for (const auto pSection : m_sections) {
          std::string sectionName = pSection->getSectionKindAsString();
          XUtil::TRACE(std::string("Examining: '") + sectionName + "'");
          pSection->getPayload(pt);
        }

        boost::property_tree::write_json(oDumpFile, pt, true /*Pretty print*/);
        break;
      }
    case Section::FormatType::html:
    case Section::FormatType::raw:
    case Section::FormatType::txt:
    case Section::FormatType::undefined:
    case Section::FormatType::unknown:
    default:
      break;
  }

  XUtil::QUIET("");
  XUtil::QUIET(boost::format("Successfully wrote all of sections which support the format '%s' to the file: '%s'")
                             % _PSD.getFormatTypeAsStr() % sDumpFileName);
}

std::string
XclBin::findKeyAndGetValue(const std::string& _searchDomain,
                           const std::string& _searchKey,
                           const std::vector<std::string>& _keyValues)
{
  std::string sDomain;
  std::string sKey;
  std::string sValue;

  for (auto const& keyValue : _keyValues) {
    getKeyValueComponents(keyValue, sDomain, sKey, sValue);
    if ((_searchDomain == sDomain) &&
        (_searchKey == sKey)) {
      return sValue;
    }
  }
  return std::string("");
}


void
XclBin::getKeyValueComponents(const std::string& _keyValue,
                              std::string& _domain,
                              std::string& _key,
                              std::string& _value)
{
  // Reset output arguments
  _domain.clear();
  _key.clear();
  _value.clear();

  const std::string& delimiters = ":";      // Our delimiter

  // Working variables
  std::string::size_type pos = 0;
  std::string::size_type lastPos = 0;
  std::vector<std::string> tokens;

  // Parse the string until the entire string has been parsed or 3 tokens have been found
  while ((lastPos < _keyValue.length() + 1) &&
         (tokens.size() < 3)) {
    pos = _keyValue.find_first_of(delimiters, lastPos);

    if ((pos == std::string::npos) ||
        (tokens.size() == 2)) {
      pos = _keyValue.length();
    }

    std::string token = _keyValue.substr(lastPos, pos - lastPos);
    tokens.push_back(token);
    lastPos = pos + 1;
  }

  if (tokens.size() != 3) {
    auto errMsg = boost::format("ERROR: Expected format [USER | SYS]:<key>:<value> when using adding a key value pair.  Received: %s.") % _keyValue;
    throw std::runtime_error(errMsg.str());
  }

  boost::to_upper(tokens[0]);
  _domain = tokens[0];
  _key = tokens[1];
  _value = tokens[2];
}

void
XclBin::setKeyValue(const std::string& _keyValue)
{
  std::string sDomain, sKey, sValue;
  getKeyValueComponents(_keyValue, sDomain, sKey, sValue);

  XUtil::TRACE(boost::format("Setting key-value pair \"%s\":  domain:'%s', key:'%s', value:'%s'")
                             % _keyValue % sDomain % sKey % sValue);

  if (sDomain == "SYS") {
    if (sKey == "mode") {
      if (sValue == "flat") {
        m_xclBinHeader.m_header.m_mode = XCLBIN_FLAT;
      } else if (sValue == "hw_pr") {
        m_xclBinHeader.m_header.m_mode = XCLBIN_PR;
      } else if (sValue == "tandem") {
        m_xclBinHeader.m_header.m_mode = XCLBIN_TANDEM_STAGE2;
      } else if (sValue == "tandem_pr") {
        m_xclBinHeader.m_header.m_mode = XCLBIN_TANDEM_STAGE2_WITH_PR;
      } else if (sValue == "hw_emu") {
        m_xclBinHeader.m_header.m_mode = XCLBIN_HW_EMU;
      } else if (sValue == "sw_emu") {
        m_xclBinHeader.m_header.m_mode = XCLBIN_SW_EMU;
      } else if (sValue == "hw_emu_pr") {
        m_xclBinHeader.m_header.m_mode = XCLBIN_HW_EMU_PR;
      } else {
        auto errMsg = boost::format("ERROR: Unknown value '%s' for key '%s'. Key-value pair: '%s'.") % sValue % sKey % _keyValue;
        throw std::runtime_error(errMsg.str());
      }
      return; // Key processed
    }

    if (sKey == "action_mask") {
      if (sValue == "LOAD_AIE") {
        m_xclBinHeader.m_header.m_actionMask |= AM_LOAD_AIE;
      }
      else if (sValue == "LOAD_PDI") {
        m_xclBinHeader.m_header.m_actionMask |= AM_LOAD_PDI;
      }
      else {
        auto errMsg = boost::format("ERROR: Unknown bit mask '%s' for the key '%s'. Key-value pair: '%s'.") % sValue % sKey % _keyValue;
        throw std::runtime_error(errMsg.str());
      }
      return; // Key processed
    }

    if (sKey == "FeatureRomTimestamp") {
      m_xclBinHeader.m_header.m_featureRomTimeStamp = XUtil::stringToUInt64(sValue);
      return; // Key processed
    }

    if (sKey == "InterfaceUUID") {
      sValue.erase(std::remove(sValue.begin(), sValue.end(), '-'), sValue.end()); // Remove the '-'
      XUtil::hexStringToBinaryBuffer(sValue, (unsigned char*)&m_xclBinHeader.m_header.m_interface_uuid, sizeof(axlf_header::m_interface_uuid));
      return; // Key processed
    }

    if (sKey == "PlatformVBNV") {
      XUtil::safeStringCopy((char*)&m_xclBinHeader.m_header.m_platformVBNV, sValue, sizeof(axlf_header::m_platformVBNV));
      return; // Key processed
    }

    if (sKey == "XclbinUUID") {
      std::cout << "Warning: Changing this 'XclbinUUID' property to a non-unique value can result in non-determinist negative runtime behavior.\n";
      sValue.erase(std::remove(sValue.begin(), sValue.end(), '-'), sValue.end()); // Remove the '-'
      XUtil::hexStringToBinaryBuffer(sValue, (unsigned char*)&m_xclBinHeader.m_header.uuid, sizeof(axlf_header::uuid));
      return; // Key processed
    }

    auto errMsg = boost::format("ERROR: Unknown key '%s' for key-value pair '%s'.") % sKey % _keyValue;
    throw std::runtime_error(errMsg.str());
  }

  if (sDomain == "USER") {
    Section* pSection = findSection(KEYVALUE_METADATA);
    if (pSection == nullptr) {
      pSection = Section::createSectionObjectOfKind(KEYVALUE_METADATA);
      addSection(pSection);
    }

    boost::property_tree::ptree ptKeyValueMetadata;
    pSection->getPayload(ptKeyValueMetadata);

    XUtil::TRACE_PrintTree("KEYVALUE:", ptKeyValueMetadata);
    boost::property_tree::ptree ptKeyValues = ptKeyValueMetadata.get_child("keyvalue_metadata");
    auto keyValues = XUtil::as_vector<boost::property_tree::ptree>(ptKeyValues, "key_values");

    // Update existing key
    bool bKeyFound = false;
    for (auto& keyvalue : keyValues) {
      if (keyvalue.get<std::string>("key") == sKey) {
        keyvalue.put("value", sValue);
        bKeyFound = true;
        XUtil::QUIET(std::string("Updating key '") + sKey + "' to '" + sValue + "'");
        break;
      }
    }

    // Need to create a new key
    if (bKeyFound == false) {
      boost::property_tree::ptree keyValue;
      keyValue.put("key", sKey);
      keyValue.put("value", sValue);
      keyValues.push_back(keyValue);
      XUtil::QUIET(std::string("Creating new key '") + sKey + "' with the value '" + sValue + "'");
    }

    // Now create a new tree to add back into the section
    boost::property_tree::ptree ptKeyValuesNew;
    for (const auto &keyvalue : keyValues) {
      ptKeyValuesNew.push_back({"", keyvalue});
    }

    boost::property_tree::ptree ptKeyValueMetadataNew;
    ptKeyValueMetadataNew.add_child("key_values", ptKeyValuesNew);

    boost::property_tree::ptree pt;
    pt.add_child("keyvalue_metadata", ptKeyValueMetadataNew);

    XUtil::TRACE_PrintTree("Final KeyValue", pt);
    pSection->readJSONSectionImage(pt);
    return;
  }

  auto errMsg = boost::format("ERROR: Unknown key domain for key-value pair '%s'.  Expected either 'USER' or 'SYS'.") % sDomain;
  throw std::runtime_error(errMsg.str());
}

void
XclBin::removeKey(const std::string& _sKey)
{

  XUtil::TRACE(boost::format("Removing User Key: '%s'") % _sKey);

  Section* pSection = findSection(KEYVALUE_METADATA);
  if (pSection == nullptr) {
    auto errMsg = boost::format("ERROR: Key '%s' not found.") % _sKey;
    throw std::runtime_error(errMsg.str());
  }

  boost::property_tree::ptree ptKeyValueMetadata;
  pSection->getPayload(ptKeyValueMetadata);

  XUtil::TRACE_PrintTree("KEYVALUE:", ptKeyValueMetadata);
  boost::property_tree::ptree ptKeyValues = ptKeyValueMetadata.get_child("keyvalue_metadata");
  auto keyValues = XUtil::as_vector<boost::property_tree::ptree>(ptKeyValues, "key_values");

  // Update existing key
  bool bKeyFound = false;
  for (unsigned int index = 0; index < keyValues.size(); ++index) {
    if (keyValues[index].get<std::string>("key") == _sKey) {
      bKeyFound = true;
      XUtil::QUIET(std::string("Removing key '") + _sKey + "'");
      keyValues.erase(keyValues.begin() + index);
      break;
    }
  }

  if (bKeyFound == false) {
    auto errMsg = boost::format("ERROR: Key '%s' not found.") % _sKey;
    throw std::runtime_error(errMsg.str());
  }

  // Now create a new tree to add back into the section
  boost::property_tree::ptree ptKeyValuesNew;
  for (const auto &keyvalue : keyValues) {
    ptKeyValuesNew.push_back({"", keyvalue});
  }

  boost::property_tree::ptree ptKeyValueMetadataNew;
  ptKeyValueMetadataNew.add_child("key_values", ptKeyValuesNew);

  boost::property_tree::ptree pt;
  pt.add_child("keyvalue_metadata", ptKeyValueMetadataNew);

  XUtil::TRACE_PrintTree("Final KeyValue", pt);
  pSection->readJSONSectionImage(pt);
  return;
}



void
XclBin::reportInfo(std::ostream& _ostream, const std::string& _sInputFile, bool _bVerbose) const
{
  FormattedOutput::reportInfo(_ostream, _sInputFile, m_xclBinHeader, m_sections, _bVerbose);
}


static void
parsePSKernelString(const std::string& encodedString,
                    std::string& mem_banks,
                    std::string& symbol_name,
                    unsigned long& num_instances,
                    std::string& path_to_library)
// Line being parsed:
//   Syntax: <mem_banks>:<symbol_name>:<instances>:<path_to_shared_library>
//   Example: 0,1:myKernel:3:./data/mylib.so
//
// Note: A file name can contain a colen (e.g., C:\test)
{
  XUtil::TRACE("Parsing PSKernel command argument: '" + encodedString + "'");
  const std::string delimiters = ":";

  // Working variables
  std::string::size_type pos = 0;
  std::string::size_type lastPos = 0;
  std::vector<std::string> tokens;

  // Parse the string until the entire string has been parsed or MAX_TOKENS tokens have been found
  constexpr size_t maxTokens = 4;
  while ((lastPos < encodedString.length() + 1) &&
         (tokens.size() < maxTokens)) {
    pos = encodedString.find_first_of(delimiters, lastPos);

    // Update the substring end to be at then end of the encodedString if:
    // a. No more delimiters were found.
    // b. The last known 'token' is being parsed.
    if ((pos == std::string::npos) ||
        (tokens.size() == maxTokens-1)) {
      pos = encodedString.length();
    }

    std::string token = encodedString.substr(lastPos, pos - lastPos);
    tokens.push_back(token);
    lastPos = pos + 1;
  }

  // Invert the vector for it makes the following parsing code easy to support
  std::reverse(tokens.begin(), tokens.end());

  // -- [0]: Path to library --
  path_to_library = (tokens.size() > 0) ? tokens[0] : "";

  // -- [1]: Number of instances --
  num_instances = 1;               // Assume a count of 1 (default)
  if ((tokens.size() > 1) && (!tokens[1].empty())) {
    char* endPtr = nullptr;
    num_instances = std::strtoul(tokens[1].c_str(), &endPtr, 10 /*base10*/);
    if (*endPtr)
      throw std::runtime_error("The value for the number of PS kernel instances is not a number: '" + tokens[1] + "'");
  }

  // -- [2]: Symbolic name --
  symbol_name = (tokens.size() > 2) ? tokens[2] : "";

  // -- [3]: Mem banks --
  mem_banks = (tokens.size() > 3) ? tokens[3] : "";

  // add check for leading and trailing ','
  if (!mem_banks.empty()) {
    if (mem_banks.front() == ',' || mem_banks.back() == ',' )
      throw std::runtime_error("Specified mem_banks is not valid");

    std::cout << "Attention: Specifying memory banks in --add-pskernel is an advanced feature." << std::endl;
    std::cout << "           Be sure to validate connections after performing this operation." << std::endl;
  }

  XUtil::TRACE(boost::format("PSKernel command arguments: mem_banks='%s', symbol_name='%s'; num_instances=%d; library='%s'") % mem_banks % symbol_name % num_instances % path_to_library);
}

void getSectionPayload(const XclBin* pXclBin,
                       axlf_section_kind kind,
                       boost::property_tree::ptree& ptPayLoad)
{
  ptPayLoad.clear();
  Section* pSection = pXclBin->findSection(kind);

  if (pSection != nullptr)
    pSection->getPayload(ptPayLoad);
}

void putSectionPayload(XclBin* pXclBin,
                       axlf_section_kind kind,
                       const boost::property_tree::ptree& ptPayLoad)
{
  // Is there anything to update, if not then exit early
  if (ptPayLoad.empty())
    return;

  Section* pSection = pXclBin->findSection(kind);

  if (pSection == nullptr) {
    pSection = Section::createSectionObjectOfKind(kind);
    pXclBin->addSection(pSection);
  }

  pSection->readJSONSectionImage(ptPayLoad);
}



void
updateKernelSections(const std::vector<boost::property_tree::ptree> &kernels,
                     bool isFixedPS,
                     XclBin *pXclbin)
{
  for (const auto& kernel : kernels) {
    boost::property_tree::ptree ptEmbedded;
    boost::property_tree::ptree ptIPLayout;
    boost::property_tree::ptree ptConnectivity;
    boost::property_tree::ptree ptMemTopology;

    // -- Get the various sections
    getSectionPayload(pXclbin, axlf_section_kind::EMBEDDED_METADATA, ptEmbedded);
    getSectionPayload(pXclbin, axlf_section_kind::IP_LAYOUT, ptIPLayout);
    getSectionPayload(pXclbin, axlf_section_kind::CONNECTIVITY, ptConnectivity);
    getSectionPayload(pXclbin, axlf_section_kind::MEM_TOPOLOGY, ptMemTopology);

    // -- Update these sections with the kernel information
    XUtil::addKernel(kernel, isFixedPS, ptEmbedded);
    XUtil::addKernel(kernel, ptMemTopology, ptIPLayout, ptConnectivity);

    // -- Update the sections and if necessary create a new section
    putSectionPayload(pXclbin, axlf_section_kind::EMBEDDED_METADATA, ptEmbedded);
    putSectionPayload(pXclbin, axlf_section_kind::IP_LAYOUT, ptIPLayout);
    putSectionPayload(pXclbin, axlf_section_kind::CONNECTIVITY, ptConnectivity);
    putSectionPayload(pXclbin, axlf_section_kind::MEM_TOPOLOGY, ptMemTopology);
  }
}

// --add-pskernel
void
XclBin::addPsKernel(const std::string& encodedString)
{
  XUtil::TRACE("Adding PSKernel");
  // Get the PS Kernel metadata from the encoded string
  std::string memBanks;
  std::string symbolicName;
  std::string kernelLibrary;
  unsigned long numInstances = 0;
  parsePSKernelString(encodedString, memBanks, symbolicName, numInstances, kernelLibrary);

  // Examine the PS library data mining the function and its arguments
  // Convert the function signatures into something useful.
  boost::property_tree::ptree ptFunctions;
  XUtil::dataMineExportedFunctionsDWARF(kernelLibrary, ptFunctions);
  XUtil::validateFunctions(kernelLibrary, ptFunctions);

  // Create the same schema that is used for kernels
  boost::property_tree::ptree ptPSKernels;
  XUtil::createPSKernelMetadata(memBanks, numInstances, ptFunctions, kernelLibrary, ptPSKernels);

  // Update the EMBEDDED_METADATA, MEM_TOPOLOGY, IP_LAYOUT, and CONNECTIVITY sections
  const boost::property_tree::ptree ptEmpty;

  const boost::property_tree::ptree ptKernels = ptPSKernels.get_child("ps-kernels", ptEmpty);
  auto kernels = XUtil::as_vector<boost::property_tree::ptree>(ptKernels, "kernels");

  if (kernels.empty()) {
    std::string errMsg = "ERROR: No kernels found in the kernel library file: " + kernelLibrary;
    throw std::runtime_error(errMsg);
  }

  // Update the sections with the PS Kernel information
  updateKernelSections(kernels, false /*isFixedPS*/, this);

  // Now add each of the kernel SOFT_KERNEL sections
  for (const auto& ptKernel : kernels) {
    // Determine if this section already exists
    const auto & kernelName = ptKernel.get<std::string>("name");
    Section* pSection = findSection(SOFT_KERNEL, kernelName);
    if (pSection != nullptr) {
      auto errMsg = boost::format("ERROR: The PS Kernel (e.g SOFT_KERNEL) section with the symbolic name '%s' already exists") % kernelName;
      throw std::runtime_error(errMsg.str());
    }

    // Create the section
    pSection = Section::createSectionObjectOfKind(SOFT_KERNEL, kernelName);
    XUtil::TRACE(boost::format("Adding PS Kernel SubSection '%s' OBJ") % kernelName);

    // Add shared library first
    std::fstream iSectionFile;
    iSectionFile.open(kernelLibrary, std::ifstream::in | std::ifstream::binary);
    if (!iSectionFile.is_open()) {
      std::string errMsg = "ERROR: Unable to open the file for reading: " + kernelLibrary;
      throw std::runtime_error(errMsg);
    }

    pSection->readSubPayload(iSectionFile, "OBJ", Section::FormatType::raw);

    // -- Add the metadata
    XUtil::TRACE(boost::format("Adding PS Kernel SubSection '%s' METADATA") % kernelName);
    boost::property_tree::ptree ptPsKernel;
    ptPsKernel.put("mpo_name", kernelName);
    ptPsKernel.put("mpo_version", "0.0.0");
    ptPsKernel.put("mpo_md5_value", "00000000000000000000000000000000");
    ptPsKernel.put("mpo_symbol_name", kernelName);
    ptPsKernel.put("m_num_instances", numInstances);

    boost::property_tree::ptree ptRTD;
    ptRTD.add_child("soft_kernel_metadata", ptPsKernel);

    std::ostringstream buffer;
    boost::property_tree::write_json(buffer, ptRTD);
    std::istringstream iSectionMetadata(buffer.str());
    pSection->readSubPayload(iSectionMetadata, "METADATA", Section::FormatType::json);

    // -- Now add the section to the collection and report our successful status
    addSection(pSection);
    std::string sSectionAddedName = pSection->getSectionKindAsString();

    XUtil::QUIET("");
    XUtil::QUIET(boost::format("Section: SOFT_KERNEL (PS KERNEL), SubName: '%s' was successfully added.") % kernelName);
  }
}

// --add-kernel
void
XclBin::addKernels(const std::string& jsonFile)
{
  XUtil::TRACE("Adding fixed kernel");

  // -- Read in the Fixed Kernel Metadata
  XUtil::TRACE("Reading given Fixed Kernel JSON file: " + jsonFile);
  std::fstream ifFixedKernels;
  ifFixedKernels.open(jsonFile, std::ifstream::in | std::ifstream::binary);
  if (!ifFixedKernels.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + jsonFile;
    throw std::runtime_error(errMsg);
  }

  boost::property_tree::ptree ptFixKernels;
  boost::property_tree::read_json(ifFixedKernels, ptFixKernels);
  XUtil::TRACE_PrintTree("Fixed Kernels Metadata", ptFixKernels);

  const boost::property_tree::ptree ptEmpty;    // Empty ptree

  // Get the kernels from the property_tree
  const boost::property_tree::ptree ptKernels = ptFixKernels.get_child("ps-kernels", ptEmpty);
  auto kernels = XUtil::as_vector<boost::property_tree::ptree>(ptKernels, "kernels");
  if (kernels.empty()) {
    std::string errMsg = "ERROR: No kernels found in the JSON file: " + jsonFile;
    throw std::runtime_error(errMsg);
  }

  // Update all of the sections with the fixed kernel metadata
  updateKernelSections(kernels, true /*isFixedPS*/, this);
}

void
XclBin::updateInterfaceuuid()
{
  XUtil::TRACE("Updating Interface uuid in xclbin");
  // Get the PARTITION_METADATA property tree (if there is one)
  const boost::property_tree::ptree ptEmpty;
  Section* pSection = findSection(PARTITION_METADATA);
  if (pSection == nullptr) {
    return;
  }

  // Get the complete JSON metadata tree
  boost::property_tree::ptree ptRoot;
  pSection->getPayload(ptRoot);
  if (ptRoot.empty()) {
    throw std::runtime_error("ERROR: Unable to get the complete JSON metadata tree.");
  }

  // Look for the "partition_metadata" node
  boost::property_tree::ptree ptPartitionMetadata = ptRoot.get_child("partition_metadata", ptEmpty);
  if (ptPartitionMetadata.empty()) {
    throw std::runtime_error("ERROR: Partition metadata node not found.");
  }

  // Look for the "interfaces" node
  auto ptInterfaces = XUtil::as_vector<boost::property_tree::ptree>(ptPartitionMetadata, "interfaces");
  // DRC check for "interfaces"
  if (m_xclBinHeader.m_header.m_mode == XCLBIN_PR) { // check only for xclbin's, not for xsabin's
    if (ptInterfaces.size() > 1) {
      throw std::runtime_error("ERROR: Invalid interfaces found in partition_metadata");
    }
  }

  // Updating axlf header interface_uuid with interface_uuid from partition_metadata
  boost::property_tree::ptree ptInterface = ptInterfaces[0];
  auto sInterfaceUUID = ptInterface.get<std::string>("interface_uuid", "00000000-0000-0000-0000-000000000000");
  sInterfaceUUID.erase(std::remove(sInterfaceUUID.begin(), sInterfaceUUID.end(), '-'), sInterfaceUUID.end()); // Remove the '-'
  XUtil::hexStringToBinaryBuffer(sInterfaceUUID, (unsigned char*)&m_xclBinHeader.m_header.m_interface_uuid, sizeof(axlf_header::m_interface_uuid));
}
