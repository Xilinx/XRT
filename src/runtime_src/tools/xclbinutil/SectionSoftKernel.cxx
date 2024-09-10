/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
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

#include "SectionSoftKernel.h"

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
SectionSoftKernel::init SectionSoftKernel::initializer;

SectionSoftKernel::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(SOFT_KERNEL, "SOFT_KERNEL", boost::factory<SectionSoftKernel*>());
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

using SubSectionTableCollection = std::vector<std::pair<std::string, SectionSoftKernel::SubSection>>;
static const SubSectionTableCollection&
getSubSectionTable()
{
  static const SubSectionTableCollection subSectionTable = {
    { "UNKNOWN", SectionSoftKernel::SubSection::unknown },
    { "OBJ", SectionSoftKernel::SubSection::obj },
    { "METADATA", SectionSoftKernel::SubSection::metadata }
  };
  return subSectionTable;
}

SectionSoftKernel::SubSection
SectionSoftKernel::getSubSectionEnum(const std::string& sSubSectionName)
{
  auto subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return boost::iequals(entry.first, sSubSectionName);});

  if (iter == subSectionTable.end())
    return SubSection::unknown;

  return iter->second;
}

// -------------------------------------------------------------------------

const std::string&
SectionSoftKernel::getSubSectionName(SectionSoftKernel::SubSection eSubSection)
{
  const auto& subSectionTable = getSubSectionTable();
  auto iter = std::find_if(subSectionTable.begin(), subSectionTable.end(), [&](const auto& entry) {return entry.second == eSubSection;});

  if (iter == subSectionTable.end())
    return getSubSectionName(SubSection::unknown);

  return iter->first;
}

// -------------------------------------------------------------------------

bool
SectionSoftKernel::subSectionExists(const std::string& _sSubSectionName) const
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

    // Create a property tree and determine if the variables are all default values
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);

    boost::property_tree::ptree& ptSoftKernel = pt.get_child("soft_kernel_metadata");

    XUtil::TRACE_PrintTree("Current SOFT_KERNEL contents", pt);
    if ((ptSoftKernel.get<std::string>("mpo_version") == "") &&
        (ptSoftKernel.get<std::string>("mpo_md5_value") == "") &&
        (ptSoftKernel.get<std::string>("mpo_symbol_name") == "") &&
        (ptSoftKernel.get<std::string>("m_num_instances") == "0")) {
      // All default values, metadata sub-section has yet to be added
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------------------------

void
SectionSoftKernel::copyBufferUpdateMetadata(const char* _pOrigDataSection,
                                            unsigned int _origSectionSize,
                                            std::istream& _istream,
                                            std::ostringstream& _buffer) const
{
  XUtil::TRACE("SectionSoftKernel::CopyBufferUpdateMetadata");

  // Do we have enough room to overlay the header structure
  if (_origSectionSize < sizeof(soft_kernel)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the soft_kernel structure (%d)") % _origSectionSize % sizeof(soft_kernel);
    throw std::runtime_error(errMsg.str());
  }

  // Prepare our destination header buffer
  soft_kernel softKernelHdr = { 0 };      // Header buffer
  std::ostringstream stringBlock;         // String block (stored immediately after the header)

  auto pHdr = reinterpret_cast<const soft_kernel*>(_pOrigDataSection);

  XUtil::TRACE_BUF("soft_kernel-original", reinterpret_cast<const char*>(pHdr), sizeof(soft_kernel));
  XUtil::TRACE(boost::format("Original: \n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  mpo_md5_value (0x%lx): '%s'\n"
                             "  mpo_symbol_name (0x%lx): '%s'\n"
                             "  m_num_instances: %d")
               % pHdr->mpo_name % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name)
               % pHdr->m_image_offset % pHdr->m_image_size
               % pHdr->mpo_version % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_version)
               % pHdr->mpo_md5_value % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_md5_value)
               % pHdr->mpo_symbol_name % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_symbol_name)
               % pHdr->m_num_instances);

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
  boost::property_tree::read_json(ss, pt);

  // ----------------------

  // Extract and update the data
  boost::property_tree::ptree& ptSK = pt.get_child("soft_kernel_metadata");

  // Update and record the variables

  // mpo_name
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(soft_kernel) + pHdr->mpo_name;
    auto sValue = ptSK.get<std::string>("mpo_name", sDefault);

    if (sValue.compare(getSectionIndexName()) != 0) {
      auto errMsg = boost::format("ERROR: Metadata data mpo_name '%s' does not match expected section name '%s'") % sValue % getSectionIndexName();
      throw std::runtime_error(errMsg.str());
    }

    softKernelHdr.mpo_name = sizeof(soft_kernel) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  mpo_name (0x%lx): '%s'") % softKernelHdr.mpo_name % sValue);
  }

  // mpo_version
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(soft_kernel) + pHdr->mpo_version;
    auto sValue = ptSK.get<std::string>("mpo_version", sDefault);
    softKernelHdr.mpo_version = sizeof(soft_kernel) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  mpo_version (0x%lx): '%s'") % softKernelHdr.mpo_version % sValue);
  }

  // mpo_md5_value
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(soft_kernel) + pHdr->mpo_md5_value;
    auto sValue = ptSK.get<std::string>("mpo_md5_value", sDefault);
    softKernelHdr.mpo_md5_value = sizeof(soft_kernel) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  mpo_md5_value (0x%lx): '%s'") % softKernelHdr.mpo_md5_value % sValue);
  }

  // mpo_symbol_name
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(soft_kernel) + pHdr->mpo_symbol_name;
    auto sValue = ptSK.get<std::string>("mpo_symbol_name", sDefault);
    softKernelHdr.mpo_symbol_name = sizeof(soft_kernel) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::format("  mpo_symbol_name (0x%lx): '%s'") % softKernelHdr.mpo_symbol_name % sValue);

    // DRC Check checking to see if the maximum symbol name length has been violated
    constexpr unsigned int MAX_SYMBOL_NAME_LENGTH = 19;
    if (sValue.length() > MAX_SYMBOL_NAME_LENGTH) {
      auto errMsg = boost::format("ERROR: The given symbol name '%s' (length %d) exceeds the maximum support length of %d characters.")
          % sValue % sValue.length() % MAX_SYMBOL_NAME_LENGTH;
      throw std::runtime_error(errMsg.str());
    }
  }

  // m_num_instances
  {
    uint32_t defaultValue = pHdr->m_num_instances;
    uint32_t value = ptSK.get<uint32_t>("m_num_instances", defaultValue);
    softKernelHdr.m_num_instances = value;
    XUtil::TRACE(boost::format("  m_num_instances: %d") % softKernelHdr.m_num_instances);

    // DRC Check checking the maximum number of instances
    constexpr unsigned int MAX_NUM_INSTANCES = 128;
    if (value > MAX_NUM_INSTANCES) {
      auto errMsg = boost::format("ERROR: The number of instances (%d) exceeds the maximum supported value (%d).") % value % MAX_NUM_INSTANCES;
      throw std::runtime_error(errMsg.str());
    }
  }

  // Last item to be initialized
  {
    softKernelHdr.m_image_offset = sizeof(soft_kernel) + stringBlock.tellp();
    softKernelHdr.m_image_size = pHdr->m_image_size;
    XUtil::TRACE(boost::format("  m_image_offset: 0x%lx") % softKernelHdr.m_image_offset);
    XUtil::TRACE(boost::format("    m_image_size: 0x%lx") % softKernelHdr.m_image_size);
  }

  // Copy the output to the output buffer.
  // Header
  _buffer.write(reinterpret_cast<const char*>(&softKernelHdr), sizeof(soft_kernel));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Image
  _buffer.write(reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionSoftKernel::createDefaultImage(std::istream& _istream, std::ostringstream& _buffer) const
{
  XUtil::TRACE("SOFT_KERNEL-OBJ");

  soft_kernel softKernelHdr = soft_kernel{};
  std::ostringstream stringBlock;       // String block (stored immediately after the header)

  // Initialize default values
  {
    // Have all of the mpo (member, point, offset) values point to the zero length terminate string
    softKernelHdr.mpo_name = sizeof(soft_kernel) + stringBlock.tellp();
    stringBlock << getSectionIndexName() << '\0';

    uint32_t mpo_emptyChar = sizeof(soft_kernel) + stringBlock.tellp();
    stringBlock << '\0';

    softKernelHdr.mpo_version = mpo_emptyChar;
    softKernelHdr.mpo_md5_value = mpo_emptyChar;
    softKernelHdr.mpo_symbol_name = mpo_emptyChar;
  }

  // Initialize the object image values (last)
  {
    _istream.seekg(0, _istream.end);
    softKernelHdr.m_image_size = _istream.tellg();
    softKernelHdr.m_image_offset = sizeof(soft_kernel) + stringBlock.tellp();
  }

  XUtil::TRACE_BUF("soft_kernel", reinterpret_cast<const char*>(&softKernelHdr), sizeof(soft_kernel));

  // Write the header information
  _buffer.write(reinterpret_cast<const char*>(&softKernelHdr), sizeof(soft_kernel));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Write Data
  {
    std::unique_ptr<unsigned char[]> memBuffer(new unsigned char[softKernelHdr.m_image_size]);
    _istream.seekg(0);
    _istream.clear();
    _istream.read(reinterpret_cast<char*>(memBuffer.get()), softKernelHdr.m_image_size);

    _buffer.write(reinterpret_cast<const char*>(memBuffer.get()), softKernelHdr.m_image_size);
  }
}

// -------------------------------------------------------------------------

void
SectionSoftKernel::readSubPayload(const char* _pOrigDataSection,
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
        std::string errMsg = "ERROR: Soft kernel object image already exists.";
        throw std::runtime_error(errMsg);
      }

      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: Soft kernel's object only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      createDefaultImage(_istream, _buffer);
      break;

    case SubSection::metadata: {
        // Some basic DRC checks
        if (_pOrigDataSection == nullptr) {
          std::string errMsg = "ERROR: Missing soft kernel object image.  Add the SOFT_KERNEL-OBJ image prior to changing its metadata.";
          throw std::runtime_error(errMsg);
        }

        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: SOFT_KERNEL-METADATA only supports the JSON format.";
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
SectionSoftKernel::writeObjImage(std::ostream& _oStream) const
{
  XUtil::TRACE("SectionSoftKernel::writeObjImage");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(soft_kernel)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the bmc structure (%d)") % m_bufferSize % sizeof(soft_kernel);
    throw std::runtime_error(errMsg.str());
  }

  // No look at the data
  auto pHdr = reinterpret_cast<soft_kernel*>(m_pBuffer);

  auto pFWBuffer = reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset;
  _oStream.write(pFWBuffer, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionSoftKernel::writeMetadata(std::ostream& _oStream) const
{
  XUtil::TRACE("SOFTKERNEL-METADATA");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(soft_kernel)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the softkernel structure (%d)") % m_bufferSize % sizeof(soft_kernel);
    throw std::runtime_error(errMsg.str());
  }

  auto pHdr = reinterpret_cast<soft_kernel*>(m_pBuffer);

  XUtil::TRACE(boost::format("Original: \n"
                             "  mpo_name (0x%lx): '%s'\n"
                             "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n"
                             "  mpo_version (0x%lx): '%s'\n"
                             "  mpo_md5_value (0x%lx): '%s'\n"
                             "  mpo_symbol_name (0x%lx): '%s'\n"
                             "  m_num_instances: %d")
               % pHdr->mpo_name % (reinterpret_cast<char*>(pHdr) + pHdr->mpo_name)
               % pHdr->m_image_offset % pHdr->m_image_size
               % pHdr->mpo_version % (reinterpret_cast<char*>(pHdr) + pHdr->mpo_version)
               % pHdr->mpo_md5_value % (reinterpret_cast<char*>(pHdr) + pHdr->mpo_md5_value)
               % pHdr->mpo_symbol_name % (reinterpret_cast<char*>(pHdr) + pHdr->mpo_symbol_name)
               % pHdr->m_num_instances);

  // Convert the data from the binary format to JSON
  boost::property_tree::ptree ptSoftKernel;

  ptSoftKernel.put("mpo_name", reinterpret_cast<char*>(pHdr) + pHdr->mpo_name);
  ptSoftKernel.put("mpo_version", reinterpret_cast<char*>(pHdr) + pHdr->mpo_version);
  ptSoftKernel.put("mpo_md5_value", reinterpret_cast<char*>(pHdr) + pHdr->mpo_md5_value);
  ptSoftKernel.put("mpo_symbol_name", reinterpret_cast<char*>(pHdr) + pHdr->mpo_symbol_name);
  ptSoftKernel.put("m_num_instances", (boost::format("%d") % pHdr->m_num_instances).str());

  boost::property_tree::ptree root;
  root.put_child("soft_kernel_metadata", ptSoftKernel);

  boost::property_tree::write_json(_oStream, root);
}

// -------------------------------------------------------------------------

void
SectionSoftKernel::writeSubPayload(const std::string& _sSubSectionName,
                                   FormatType _eFormatType,
                                   std::fstream&  _oStream) const
{
  // Some basic DRC checks
  if (m_pBuffer == nullptr) {
    std::string errMsg = "ERROR: Soft Kernel section does not exist.";
    throw std::runtime_error(errMsg);
  }

  SubSection eSubSection = getSubSectionEnum(_sSubSectionName);

  switch (eSubSection) {
    case SubSection::obj:
      // Some basic DRC checks
      if (_eFormatType != Section::FormatType::raw) {
        std::string errMsg = "ERROR: SOFT_KERNEL-OBJ only supports the RAW format.";
        throw std::runtime_error(errMsg);
      }

      writeObjImage(_oStream);
      break;

    case SubSection::metadata: {
        if (_eFormatType != Section::FormatType::json) {
          std::string errMsg = "ERROR: SOFT_KERNEL-METADATA only supports the JSON format.";
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
SectionSoftKernel::readXclBinBinary(std::istream& _istream, const axlf_section_header& _sectionHeader)
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

  boost::property_tree::ptree& ptSoftKernel = pt.get_child("soft_kernel_metadata");

  XUtil::TRACE_PrintTree("Current SOFT_KERNEL contents", pt);
  std::string sName = ptSoftKernel.get<std::string>("mpo_name");

  Section::m_sIndexName = sName;
}
