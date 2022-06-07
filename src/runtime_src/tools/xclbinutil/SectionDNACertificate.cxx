/**
 * Copyright (C) 2018, 2022 Xilinx, Inc
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

#include "SectionDNACertificate.h"

#include "XclBinUtilities.h"
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <string>

namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionDNACertificate::init SectionDNACertificate::initializer;

SectionDNACertificate::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(DNA_CERTIFICATE, "DNA_CERTIFICATE", boost::factory<SectionDNACertificate*>());

  // Add format support empty (no support)

  sectionInfo->supportedDumpFormats.push_back(FormatType::json);
  sectionInfo->supportedDumpFormats.push_back(FormatType::html);
  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// ----------------------------------------------------------------------------

#define signatureSizeBytes 512

#define dnaEntrySizeBytes 12

struct dnaEntry {
  char bytes[dnaEntrySizeBytes];
};


#define dnaByteAlignment 64

void
SectionDNACertificate::marshalToJSON(char* _pDataSection,
                                     unsigned int _sectionSize,
                                     boost::property_tree::ptree& _ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: DNA_CERTIFICATE");
  XUtil::TRACE_BUF("Section Buffer", reinterpret_cast<const char*>(_pDataSection), _sectionSize);

  if ((_sectionSize % dnaByteAlignment) != 0) {
    auto errMsg = boost::format("ERROR: The DNA_CERTIFICATE section size doesn't align to 64 byte boundaries.  Current size: %ld") % _sectionSize;
    throw std::runtime_error(errMsg.str());
  }

  static const int minimumSectionSizeBytes = signatureSizeBytes + dnaByteAlignment;
  if (_sectionSize < minimumSectionSizeBytes) {
    auto errMsg = boost::format("ERROR: The DNA_CERTIFICATE section size (%ld) is smaller then the minimum section permitted (%ld).") % _sectionSize % minimumSectionSizeBytes;
    throw std::runtime_error(errMsg.str());
  }

  // Get the dnsSignature
  std::string sDNSSignature;
  XUtil::binaryBufferToHexString((unsigned char*)&_pDataSection[_sectionSize - signatureSizeBytes], signatureSizeBytes, sDNSSignature);

  // Get the number of sections
  unsigned char* pWorking = (unsigned char*)&_pDataSection[_sectionSize - signatureSizeBytes - sizeof(uint64_t)];
  XUtil::TRACE_BUF("DNA Entries", (char*)pWorking, sizeof(uint64_t));
  uint64_t dnaEntriesBitSize = 0;
  ;
  for (unsigned int index = 0; index < sizeof(uint64_t); ++index) {
    dnaEntriesBitSize = dnaEntriesBitSize << 8;
    dnaEntriesBitSize += (uint64_t)pWorking[index];
  }

  // Instead of dividing by 8 we multiple the other side by 8
  if ((dnaEntriesBitSize % (8 * dnaEntrySizeBytes)) != 0) {
    auto errMsg = boost::format("ERROR: The DNA_CERTIFICATE reserved DNA entries bit size (0x%lx) does not align with the byte boundary (0x%lx)") % dnaEntriesBitSize % dnaEntrySizeBytes;
    throw std::runtime_error(errMsg.str());
  }

  if (((dnaEntriesBitSize / 8) > _sectionSize)) {
    auto errMsg = boost::format("ERROR: The message DNA length (0x%x bytes) exceeds the DNA_CERTIFICATE size (0x%x bytes).") % (dnaEntriesBitSize / 8) % _sectionSize;
    throw std::runtime_error(errMsg.str());
  }

  uint64_t dnaEntryCount = (dnaEntriesBitSize / 8) / dnaEntrySizeBytes;
  XUtil::TRACE("DNA Entry Count: " + std::to_string(dnaEntryCount));

  // Get padding string
  std::string sPadding;
  uint64_t paddingOffset = dnaEntryCount * dnaEntrySizeBytes;
  uint64_t paddingSize = (_sectionSize - signatureSizeBytes) - paddingOffset;
  XUtil::binaryBufferToHexString((unsigned char*)&_pDataSection[paddingOffset], paddingSize, sPadding);



  boost::property_tree::ptree dna_list;
  struct dnaEntry* dnaEntries = reinterpret_cast<struct dnaEntry*>((void*)_pDataSection);

  for (unsigned int index = 0; index < dnaEntryCount; ++index) {
    std::string dnaString;
    XUtil::binaryBufferToHexString((unsigned char*)&dnaEntries[index], dnaEntrySizeBytes, dnaString);

    boost::property_tree::ptree ptDNA;
    ptDNA.put("", dnaString.c_str());
    dna_list.push_back({ "", ptDNA });
  }
  XUtil::TRACE_PrintTree("DNA_LIST", dna_list);

  boost::property_tree::ptree ptDNACertificate;
  ptDNACertificate.add_child("dna_list", dna_list);
  ptDNACertificate.put("padding", sPadding.c_str());
  ptDNACertificate.put("signature", sDNSSignature.c_str());

  XUtil::TRACE_PrintTree("DNA_TREE", ptDNACertificate);

  _ptree.add_child("dna_certificate", ptDNACertificate);
}

