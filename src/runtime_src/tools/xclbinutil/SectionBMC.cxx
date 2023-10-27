/**
 * Copyright (C) 2018, 2020-2023 Xilinx, Inc
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

#include "SectionBMC.h"

#include "XclBinUtilities.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace XUtil = XclBinUtilities;

// Disable windows compiler warnings
#ifdef _WIN32
  #pragma warning( disable : 4100)      // 4100 - Unreferenced formal parameter
#endif

// Static Variables / Classes
SectionBMC::init SectionBMC::initializer;

SectionBMC::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(BMC, "BMC", boost::factory<SectionBMC*>());
  sectionInfo->supportsSubSections = true;
  sectionInfo->subSections.push_back(getSubSectionName(SubSection::fw));
  sectionInfo->subSections.push_back(getSubSectionName(SubSection::metadata));

  // Add format support empty (no support)
  // The top-level section doesn't support any add syntax.
  // Must use sub-sections

  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

using SubSectionTableCollection = std::vector<std::pair<std::string, SectionBMC::SubSection>>;
static const SubSectionTableCollection&
getSubSectionTable()
{
  static const SubSectionTableCollection subSectionTable = {
    { "UNKNOWN", SectionBMC::SubSection::unknown },
    { "FW", SectionBMC::SubSection::fw },
    { "METADATA", SectionBMC::SubSection::metadata }
  };

  return subSectionTable;
}


SectionBMC::SubSection
SectionBMC::getSubSectionEnum(const std::string& sSubSectionName)
{
  const auto& subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return boost::iequals(entry.first, sSubSectionName);});

  if (iter == subSectionTable.end())
    return SubSection::unknown;

  return iter->second;
}

// -------------------------------------------------------------------------

const std::string&
SectionBMC::getSubSectionName(SectionBMC::SubSection eSubSection)
{
  const auto& subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return entry.second == eSubSection;});

  if (iter == subSectionTable.end())
    return getSubSectionName(SubSection::unknown);

  return iter->first;
}

// -------------------------------------------------------------------------

bool
SectionBMC::subSectionExists(const std::string& _sSubSectionName) const
{
  // No buffer no subsections
  if (m_pBuffer == nullptr) {
    return false;
  }

  SubSection eSS = getSubSectionEnum(_sSubSectionName);

  if (eSS == SubSection::metadata) {
    std::ostringstream buffer;
    writeMetadata(buffer);

    std::stringstream ss;
    const std::string& sBuffer = buffer.str();
    ss.write((char*)sBuffer.c_str(), sBuffer.size());

    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);

    boost::property_tree::ptree& ptBMC = pt.get_child("bmc_metadata");

    if ((ptBMC.get<std::string>("m_image_name") == "") &&
        (ptBMC.get<std::string>("m_device_name") == "") &&
        (ptBMC.get<std::string>("m_version") == "") &&
        (ptBMC.get<std::string>("m_md5value") == "")) {
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------------------------

void
SectionBMC::copyBufferUpdateMetadata(const char* _pOrigDataSection,
                                     unsigned int _origSectionSize,
                                     std::istream& _istream,
                                     std::ostringstream& _buffer) const
{
  XUtil::TRACE("SectionBMC::CopyBufferUpdateMetadata");

  // Copy the buffer
  std::unique_ptr<unsigned char[]> copyBuffer(new unsigned char[_origSectionSize]);
  memcpy(copyBuffer.get(), _pOrigDataSection, _origSectionSize);

  // ----------------------

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (_origSectionSize < sizeof(bmc)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the bmc structure (%d)") % _origSectionSize % sizeof(bmc);
    throw std::runtime_error(errMsg.str());
  }

  bmc* pHdr = (bmc*)copyBuffer.get();

  XUtil::TRACE_BUF("bmc", reinterpret_cast<const char*>(pHdr), sizeof(bmc));

  XUtil::TRACE(boost::format("Original: m_offset: 0x%lx, m_size: 0x%lx, m_image_name: '%s', m_device_name: '%s', m_version: '%s', m_md5Value: '%s'")
               % pHdr->m_offset
               % pHdr->m_size
               % pHdr->m_image_name
               % pHdr->m_device_name
               % pHdr->m_version
               % pHdr->m_md5value);

  uint64_t expectedSize = pHdr->m_offset + pHdr->m_size;

  // Check to see if array size
  if (expectedSize > _origSectionSize) {
    auto errMsg = boost::format("ERROR: bmc section size (0x%lx) exceeds the given segment size (0x%lx).") % expectedSize % _origSectionSize;
    throw std::runtime_error(errMsg.str());
  }

  // ----------------------

  // Get the JSON metadata
  _istream.seekg(0, _istream.end);
  std::streamsize fileSize = _istream.tellg();

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

  // ----------------------

  // Extract and update the data
  boost::property_tree::ptree& ptBMC = pt.get_child("bmc_metadata");

  // Image Name
  auto sImageName = ptBMC.get<std::string>("m_image_name");
  if (sImageName.length() >= sizeof(bmc::m_image_name)) {
    auto errMsg = boost::format("ERROR: The m_image_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'")
        % (int)sImageName.length() % (unsigned int)sizeof(bmc::m_image_name) % sImageName;
    throw std::runtime_error(errMsg.str());
  }
  memcpy(pHdr->m_image_name, sImageName.c_str(), sImageName.length() + 1);

  // Device Name
  auto sDeviceName = ptBMC.get<std::string>("m_device_name");
  if (sDeviceName.length() >= sizeof(bmc::m_device_name)) {
    auto errMsg = boost::format("ERROR: The m_device_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'")
        % (int)sDeviceName.length() % (unsigned int)sizeof(bmc::m_device_name) % sDeviceName;
    throw std::runtime_error(errMsg.str());
  }
  memcpy(pHdr->m_device_name, sDeviceName.c_str(), sDeviceName.length() + 1);

  // Version
  auto sVersion = ptBMC.get<std::string>("m_version");
  if (sVersion.length() >= sizeof(bmc::m_version)) {
    auto errMsg = boost::format("ERROR: The m_version entry length (%d), exceeds the allocated space (%d).  Version: '%s'")
        % (int)sVersion.length() % (unsigned int)sizeof(bmc::m_version) % sVersion;
    throw std::runtime_error(errMsg.str());
  }
  memcpy(pHdr->m_version, sVersion.c_str(), sVersion.length() + 1);

  // MD5 Value
  auto sMD5Value = ptBMC.get<std::string>("m_md5value");
  if (sMD5Value.length() >= sizeof(bmc::m_md5value)) {
    auto errMsg = boost::format("ERROR: The m_md5value entry length (%d), exceeds the allocated space (%d).  Value: '%s'")
        % (int)sMD5Value.length() % (unsigned int)sizeof(bmc::m_md5value) % sMD5Value;
    throw std::runtime_error(errMsg.str());
  }
  memcpy(pHdr->m_md5value, sMD5Value.c_str(), sMD5Value.length() + 1);

  XUtil::TRACE(boost::format("Modified: m_offset: 0x%lx, m_size: 0x%lx, m_image_name: '%s', m_device_name: '%s', m_version: '%s', m_md5Value: '%s'")
               % pHdr->m_offset
               % pHdr->m_size
               % pHdr->m_image_name
               % pHdr->m_device_name
               % pHdr->m_version
               % pHdr->m_md5value);
  // ----------------------

  // Copy the output to the output buffer.
  _buffer.write((const char*)copyBuffer.get(), _origSectionSize);
}

// -------------------------------------------------------------------------

void
SectionBMC::createDefaultFWImage(std::istream& _istream, std::ostringstream& _buffer) const
{
  bmc bmcHdr = bmc{};

  XUtil::TRACE("BMC-FW");

  // Determine if the file can be opened and its size
  {
    _istream.seekg(0, _istream.end);

    static_assert(sizeof(std::streamsize) <= sizeof(uint64_t), "std::streamsize precision is greater then 64 bits");
    bmcHdr.m_size = (uint64_t)_istream.tellg();
    bmcHdr.m_offset = sizeof(bmc);
  }

  XUtil::TRACE(boost::format("Default: m_offset: 0x%lx, m_size: 0x%lx, m_image_name: '%s', m_device_name: '%s', m_version: '%s', m_md5Value: '%s'")
               % bmcHdr.m_offset
               % bmcHdr.m_size
               % bmcHdr.m_image_name
               % bmcHdr.m_device_name
               % bmcHdr.m_version
               % bmcHdr.m_md5value);

  XUtil::TRACE_BUF("bmc", reinterpret_cast<const char*>(&bmcHdr), sizeof(bmc));

  // Create the buffer
  _buffer.write(reinterpret_cast<const char*>(&bmcHdr), sizeof(bmc));

  // Write Data
  {


    std::unique_ptr<unsigned char[]> memBuffer(new unsigned char[bmcHdr.m_size]);
    _istream.seekg(0);
    _istream.clear();
    _istream.read((char*)memBuffer.get(), bmcHdr.m_size);

    _buffer.write(reinterpret_cast<const char*>(memBuffer.get()), bmcHdr.m_size);
  }
}

// -------------------------------------------------------------------------

void
SectionBMC::readSubPayload(const char* _pOrigDataSection,
                           unsigned int _origSectionSize,
                           std::istream& _istream,
                           const std::string& _sSubSectionName,
                           Section::FormatType _eFormatType,
                           std::ostringstream& _buffer) const
{
  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SubSection::fw:
      // Some basic DRC checks
      if (_pOrigDataSection != nullptr) {
        std::string errMsg = "ERROR: Firmware image already exists.";
        throw std::runtime_error(errMsg);
      }

      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: BMC-FW only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      createDefaultFWImage(_istream, _buffer);
      break;

    case SubSection::metadata: {
        // Some basic DRC checks
        if (_pOrigDataSection == nullptr) {
          std::string errMsg = "ERROR: Missing firmware image.  Add the BMC-FW image prior to change its metadata.";
          throw std::runtime_error(errMsg);
        }

        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: BMC-METADATA only supports the JSON format.";
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

void
SectionBMC::writeFWImage(std::ostream& _oStream) const
{
  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(bmc)) {
    auto errMsg = boost::format("ERROR: Segment size (%u) is smaller than the size of the bmc structure (%d)") % m_bufferSize % sizeof(bmc);
    throw std::runtime_error(errMsg.str());
  }

  bmc* pHdr = (bmc*)m_pBuffer;

  char* pFWBuffer = (char*)pHdr + pHdr->m_offset;
  _oStream.write(pFWBuffer, pHdr->m_size);
}

void
SectionBMC::writeMetadata(std::ostream& _oStream) const
{
  XUtil::TRACE("BMC-METADATA");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(bmc)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the bmc structure (%d)") % m_bufferSize % sizeof(bmc);
    throw std::runtime_error(errMsg.str());
  }

  bmc* pHdr = (bmc*)m_pBuffer;

  XUtil::TRACE(boost::format("m_offset: 0x%lx, m_size: 0x%lx, m_image_name: '%s', m_device_name: '%s', m_version: '%s', m_md5Value: '%s'")
               % pHdr->m_offset
               % pHdr->m_size
               % pHdr->m_image_name
               % pHdr->m_device_name
               % pHdr->m_version
               % pHdr->m_md5value);

  // Image Name
  boost::property_tree::ptree ptBMC;


  ptBMC.put("m_image_name", pHdr->m_image_name);
  ptBMC.put("m_device_name", pHdr->m_device_name);
  ptBMC.put("m_version", pHdr->m_version);
  ptBMC.put("m_md5value", pHdr->m_md5value);

  boost::property_tree::ptree root;
  root.put_child("bmc_metadata", ptBMC);

  boost::property_tree::write_json(_oStream, root);
}


void
SectionBMC::writeSubPayload(const std::string& _sSubSectionName,
                            FormatType _eFormatType,
                            std::fstream&  _oStream) const
{
  // Some basic DRC checks
  if (m_pBuffer == nullptr) {
    std::string errMsg = "ERROR: BMC section does not exist.";
    throw std::runtime_error(errMsg);
  }

  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SubSection::fw:
      // Some basic DRC checks
      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: BMC-FW only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      writeFWImage(_oStream);
      break;

    case SubSection::metadata: {
        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: BMC-METADATA only supports the JSON format.";
          throw std::runtime_error(errMsg);
        }

        writeMetadata(_oStream);
      }
      break;

    case SubSection::unknown:
    default: {
        auto errMsg = boost::format("ERROR: Subsection '%s' not support by section '%s") % _sSubSectionName.c_str() % getSectionKindAsString();
        throw std::runtime_error(errMsg.str());
      }
      break;
  }
}
