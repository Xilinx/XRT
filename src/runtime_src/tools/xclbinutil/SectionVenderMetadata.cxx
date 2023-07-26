/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc.
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

#include "SectionVenderMetadata.h"

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
SectionVenderMetadata::init SectionVenderMetadata::initializer;

SectionVenderMetadata::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(VENDER_METADATA, "VENDER_METADATA", boost::factory<SectionVenderMetadata*>());
  sectionInfo->supportsSubSections = true;

  // There is only one-subsection that is supported.  By default it is not named.
  sectionInfo->subSections.push_back("");

  sectionInfo->supportsIndexing = true;

  // Add format support empty (no support)
  // The top-level section doesn't support any add syntax.
  // Must use sub-sections

  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// -------------------------------------------------------------------------

bool
SectionVenderMetadata::subSectionExists(const std::string& _sSubSectionName) const
{
  // No buffer no subsections
  return  (m_pBuffer != nullptr);
}

// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
void
SectionVenderMetadata::copyBufferUpdateMetadata(const char* _pOrigDataSection,
                                                unsigned int _origSectionSize,
                                                std::istream& _istream,
                                                std::ostringstream& _buffer) const
{
  XUtil::TRACE("SectionVenderMetadata::CopyBufferUpdateMetadata");

  // Do we have enough room to overlay the header structure
  if (_origSectionSize < sizeof(vender_metadata)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the vender_metadata structure (%d)")
        % _origSectionSize % sizeof(vender_metadata);
    throw std::runtime_error(boost::str(errMsg));
  }

  // Prepare our destination header buffer
  vender_metadata venderMetadataHdr = { 0 };   // Header buffer
  std::ostringstream stringBlock;              // String block (stored immediately after the header)

  const vender_metadata* pHdr = reinterpret_cast<const vender_metadata*>(_pOrigDataSection);

  XUtil::TRACE_BUF("vender_metadata-original", reinterpret_cast<const char*>(pHdr), sizeof(vender_metadata));
  XUtil::TRACE(boost::str(boost::format(
                              "Original: \n"
                              "  mpo_name (0x%lx): '%s'\n"
                              "  m_image_offset: 0x%lx, m_image_size: 0x%lx\n")
                          % pHdr->mpo_name % (reinterpret_cast<const char*>(pHdr) + pHdr->mpo_name)
                          % pHdr->m_image_offset % pHdr->m_image_size));

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

  // Extract and update the data
  const boost::property_tree::ptree ptEmpty;
  const boost::property_tree::ptree& ptSK = pt.get_child("vender_metadata", ptEmpty);
  if (ptSK.empty())
    throw std::runtime_error("ERROR: copyBufferUpdateMetadata could not find the vender_metadata section.");

  // Update and record the variables
  // mpo_name
  {
    auto sDefault = reinterpret_cast<const char*>(pHdr) + sizeof(vender_metadata) + pHdr->mpo_name;
    auto sValue = ptSK.get<std::string>("mpo_name", sDefault);

    if (sValue.compare(getSectionIndexName()) != 0) {
      auto errMsg = boost::format("ERROR: Metadata data mpo_name '%s' does not match expected section name '%s'")
          % sValue % getSectionIndexName();
      throw std::runtime_error(boost::str(errMsg));
    }

    venderMetadataHdr.mpo_name = sizeof(vender_metadata) + stringBlock.tellp();
    stringBlock << sValue << '\0';
    XUtil::TRACE(boost::str(boost::format("  mpo_name (0x%lx): '%s'") % venderMetadataHdr.mpo_name % sValue));
  }

  // Last item to be initialized
  {
    venderMetadataHdr.m_image_offset = sizeof(vender_metadata) + stringBlock.tellp();
    venderMetadataHdr.m_image_size = pHdr->m_image_size;
    XUtil::TRACE(boost::str(boost::format("  m_image_offset: 0x%lx") % venderMetadataHdr.m_image_offset));
    XUtil::TRACE(boost::str(boost::format("    m_image_size: 0x%lx") % venderMetadataHdr.m_image_size));
  }

  // Copy the output to the output buffer.
  // Header
  _buffer.write(reinterpret_cast<const char*>(&venderMetadataHdr), sizeof(vender_metadata));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Image
  _buffer.write(reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionVenderMetadata::createDefaultImage(std::istream& _istream, std::ostringstream& _buffer) const
{
  XUtil::TRACE("VENDER_METADATA IMAGE");

  vender_metadata venderMetadataHdr = vender_metadata{};
  std::ostringstream stringBlock;       // String block (stored immediately after the header)

  // Initialize default values
  {
    venderMetadataHdr.mpo_name = sizeof(vender_metadata) + stringBlock.tellp();
    stringBlock << getSectionIndexName() << '\0';
  }

  // Initialize the object image values (last)
  {
    _istream.seekg(0, _istream.end);
    venderMetadataHdr.m_image_size = _istream.tellg();
    venderMetadataHdr.m_image_offset = sizeof(vender_metadata) + stringBlock.tellp();
  }

  XUtil::TRACE_BUF("vender_metadata", reinterpret_cast<const char*>(&venderMetadataHdr), sizeof(vender_metadata));

  // Write the header information
  _buffer.write(reinterpret_cast<const char*>(&venderMetadataHdr), sizeof(vender_metadata));

  // String block
  std::string sStringBlock = stringBlock.str();
  _buffer.write(sStringBlock.c_str(), sStringBlock.size());

  // Write Data
  {
    std::unique_ptr<unsigned char[]> memBuffer(new unsigned char[venderMetadataHdr.m_image_size]);
    _istream.seekg(0);
    _istream.clear();
    _istream.read(reinterpret_cast<char*>(memBuffer.get()), venderMetadataHdr.m_image_size);

    _buffer.write(reinterpret_cast<const char*>(memBuffer.get()), venderMetadataHdr.m_image_size);
  }
}

// -------------------------------------------------------------------------

void
SectionVenderMetadata::readSubPayload(const char* _pOrigDataSection,
                                      unsigned int _origSectionSize,
                                      std::istream& _istream,
                                      const std::string& _sSubSectionName,
                                      Section::FormatType _eFormatType,
                                      std::ostringstream& _buffer) const
{
  // Only default (e.g. empty) sub sections are supported
  if (!_sSubSectionName.empty()) {
    auto errMsg = boost::format("ERROR: Subsection '%s' not support by section '%s")
        % _sSubSectionName % getSectionKindAsString();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Some basic DRC checks
  if (_pOrigDataSection != nullptr) {
    std::string errMsg = "ERROR: Vendor Metadata image already exists.";
    throw std::runtime_error(errMsg);
  }

  if (_eFormatType != Section::FormatType::raw) {
    std::string errMsg = "ERROR: Vendor Metadata only supports the RAW format.";
    throw std::runtime_error(errMsg);
  }

  createDefaultImage(_istream, _buffer);
}

// -------------------------------------------------------------------------

void
SectionVenderMetadata::writeObjImage(std::ostream& _oStream) const
{
  XUtil::TRACE("SectionVenderMetadata::writeObjImage");

  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(vender_metadata)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the vendor metadata structure (%d)") % m_bufferSize % sizeof(vender_metadata);
    throw std::runtime_error(boost::str(errMsg));
  }

  // Now look at the data
  auto pHdr = reinterpret_cast<vender_metadata*>(m_pBuffer);

  auto pFWBuffer = reinterpret_cast<const char*>(pHdr) + pHdr->m_image_offset;
  _oStream.write(pFWBuffer, pHdr->m_image_size);
}

// -------------------------------------------------------------------------

void
SectionVenderMetadata::writeMetadata(std::ostream& _oStream) const
{
  XUtil::TRACE("VENDER_METADATA writeMetadata");
  // Overlay the structure
  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(vender_metadata)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the vender_metadata structure (%d)")
        % m_bufferSize % sizeof(vender_metadata);
    throw std::runtime_error(boost::str(errMsg));
  }

  auto pHdr = reinterpret_cast<vender_metadata*>(m_pBuffer);
  XUtil::TRACE(boost::str(boost::format(
                              "Original: \n"
                              "  mpo_name (0x%lx): '%s'\n"
                              "  m_image_offset: 0x%lx, m_image_size: 0x%lx")
                          % pHdr->mpo_name % (reinterpret_cast<char*>(pHdr) + pHdr->mpo_name)
                          % pHdr->m_image_offset % pHdr->m_image_size));

  // Convert the data from the binary format to JSON
  boost::property_tree::ptree ptVenderMetadata;

  ptVenderMetadata.put("mpo_name", reinterpret_cast<char*>(pHdr) + pHdr->mpo_name);

  boost::property_tree::ptree root;
  root.put_child("vender_metadata", ptVenderMetadata);

  boost::property_tree::write_json(_oStream, root);
}

// -------------------------------------------------------------------------

void
SectionVenderMetadata::writeSubPayload(const std::string& _sSubSectionName,
                                       FormatType _eFormatType,
                                       std::fstream&  _oStream) const
{
  // Some basic DRC checks
  if (m_pBuffer == nullptr) {
    std::string errMsg = "ERROR: Vendor Metadata section does not exist.";
    throw std::runtime_error(errMsg);
  }

  if (!_sSubSectionName.empty()) {
    auto errMsg = boost::format("ERROR: Subsection '%s' not support by section '%s")
        % _sSubSectionName % getSectionKindAsString();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Some basic DRC checks
  if (_eFormatType != Section::FormatType::raw) {
    std::string errMsg = "ERROR: Vendor Metadata section only supports the RAW format.";
    throw std::runtime_error(errMsg);
  }

  writeObjImage(_oStream);
}

void
SectionVenderMetadata::readXclBinBinary(std::istream& _istream, const axlf_section_header& _sectionHeader)
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

  const boost::property_tree::ptree ptEmpty;
  const boost::property_tree::ptree& ptVenderMetadata = pt.get_child("vender_metadata", ptEmpty);
  if (ptVenderMetadata.empty())
    throw std::runtime_error("ERROR: copyBufferUpdateMetadata could not find the vender_metadata section.");

  XUtil::TRACE_PrintTree("Current VENDER_METADATA contents", pt);
  auto sName = ptVenderMetadata.get<std::string>("mpo_name");

  Section::m_sIndexName = sName;
}
