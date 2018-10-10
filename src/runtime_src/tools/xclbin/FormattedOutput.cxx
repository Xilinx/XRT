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

#include "FormattedOutput.h"
#include "Section.h"

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

std::string
FormattedOutput::getTimeStampAsString(const axlf &_xclBinHeader) {
  return XUtil::format("%ld", _xclBinHeader.m_header.m_timeStamp);
}

std::string
FormattedOutput::getFeatureRomTimeStampAsString(const axlf &_xclBinHeader) {
  return XUtil::format("%d", _xclBinHeader.m_header.m_featureRomTimeStamp);
}

std::string
FormattedOutput::getVersionAsString(const axlf &_xclBinHeader) {
  return XUtil::format("%d", _xclBinHeader.m_header.m_version);
}

// String Getters
std::string 
FormattedOutput::getMagicAsString(const axlf &_xclBinHeader) { 
  return XUtil::format("%s", _xclBinHeader.m_magic); 
}

std::string 
FormattedOutput::getCipherAsString(const axlf &_xclBinHeader) { 
  std::string sTemp("");
  XUtil::binaryBufferToHexString((unsigned char*)&_xclBinHeader.m_cipher, sizeof(_xclBinHeader.m_cipher), sTemp);
  return sTemp; // TBD: "0x" + sTemp; ? do the others too...
}

std::string 
FormattedOutput::getKeyBlockAsString(const axlf &_xclBinHeader) { 
  std::string sTemp("");
  XUtil::binaryBufferToHexString((unsigned char*)&_xclBinHeader.m_keyBlock, sizeof(_xclBinHeader.m_keyBlock), sTemp);
  return sTemp;
}

std::string 
FormattedOutput::getUniqueIdAsString(const axlf &_xclBinHeader) { 
  std::string sTemp("");
  XUtil::binaryBufferToHexString((unsigned char*)&_xclBinHeader.m_uniqueId, sizeof(_xclBinHeader.m_uniqueId), sTemp);
  return sTemp;
}

std::string
getSizeAsString(const axlf &_xclBinHeader) {
  return XUtil::format("%ld", _xclBinHeader.m_header.m_length);
}


std::string
FormattedOutput::getModeAsString(const axlf &_xclBinHeader) {
  return XUtil::format("%d", _xclBinHeader.m_header.m_mode);
}

std::string
getModeAsPrettyString(const axlf &_xclBinHeader) {
  switch (_xclBinHeader.m_header.m_mode) {
    case MEM_DDR3: 
      return "MEM_DDR3";
      break;
    case MEM_DDR4: 
      return "MEM_DDR4";
      break;
    case MEM_DRAM: 
      return "MEM_DRAM";
      break;
    case MEM_STREAMING: 
      return "MEM_STREAMING";
      break;
    case MEM_PREALLOCATED_GLOB: 
      return "MEM_PREALLOCATED_GLOB";
      break;
    case MEM_ARE: 
      return "MEM_ARE"; // Aurora
      break;
    case MEM_HBM: 
      return "MEM_HBM";
      break;
    case MEM_BRAM: 
      return "MEM_BRAM";
      break;
    case MEM_URAM: 
      return "MEM_URAM";
      break;
    default: 
      return "UNKNOWN";
      break;
  }
}

std::string
FormattedOutput::getFeatureRomUuidAsString(const axlf &_xclBinHeader) {
  std::string sTemp("");
  XUtil::binaryBufferToHexString(_xclBinHeader.m_header.rom_uuid, sizeof(axlf_header::rom_uuid), sTemp);
  return sTemp;
}

std::string
FormattedOutput::getPlatformVbnvAsString(const axlf &_xclBinHeader) {
  return XUtil::format("%s", _xclBinHeader.m_header.m_platformVBNV);
}

std::string
FormattedOutput::getXclBinUuidAsString(const axlf &_xclBinHeader) {
  std::string sTemp("");
  XUtil::binaryBufferToHexString(_xclBinHeader.m_header.uuid, sizeof(axlf_header::uuid), sTemp);
  return sTemp;
}

std::string
FormattedOutput::getDebugBinAsString(const axlf &_xclBinHeader) {
  return XUtil::format("%s", _xclBinHeader.m_header.m_debug_bin);
}

template <typename T>
std::vector<T> as_vector(boost::property_tree::ptree const& pt, 
                         boost::property_tree::ptree::key_type const& key)
{
    std::vector<T> r;
    for (auto& item : pt.get_child(key))
        r.push_back(item.second);
    return r;
}


void 
FormattedOutput::getKernelDDRMemory(const std::string _sKernelInstanceName, 
                                    const std::vector<Section*> _sections,
                                    boost::property_tree::ptree &_pMemTopology)
{
  if (_sKernelInstanceName.empty()) {
    return;
  }

  // 1) Look for our sections section
  Section *pMemTopology = nullptr;
  Section *pConnectivity = nullptr;
  Section *pIPLayout = nullptr;

  for (auto pSection : _sections) {
    if (MEM_TOPOLOGY == pSection->getSectionKind() ) {
      pMemTopology=pSection; 
    } else if (CONNECTIVITY == pSection->getSectionKind() ) {
      pConnectivity=pSection; 
    } else if (IP_LAYOUT == pSection->getSectionKind() ) {
      pIPLayout=pSection; 
    }
  }
  
  if ((pMemTopology == nullptr) ||
      (pConnectivity == nullptr) ||
      (pIPLayout == nullptr)) {
    // Nothing to work on
    return; 
  }

  // 2) Get the property trees and convert section into vector arrays
  boost::property_tree::ptree ptSections;
  pMemTopology->getPayload(ptSections);
  pConnectivity->getPayload(ptSections);
  pIPLayout->getPayload(ptSections);
  XUtil::TRACE_PrintTree("Top", ptSections);

  boost::property_tree::ptree& ptMemTopology = ptSections.get_child("mem_topology");
  std::vector<boost::property_tree::ptree> memTopology = as_vector<boost::property_tree::ptree>(ptMemTopology, "m_mem_data");

  boost::property_tree::ptree& ptConnectivity = ptSections.get_child("connectivity");
  std::vector<boost::property_tree::ptree> connectivity = as_vector<boost::property_tree::ptree>(ptConnectivity, "m_connection");

  boost::property_tree::ptree& ptIPLayout = ptSections.get_child("ip_layout");
  std::vector<boost::property_tree::ptree> ipLayout = as_vector<boost::property_tree::ptree>(ptIPLayout, "m_ip_data");

  // 3) Establish the connections
  for (auto connection : connectivity) {
    unsigned int ipLayoutIndex = connection.get<uint32_t>("m_ip_layout_index");
    unsigned int memDataIndex = connection.get<uint32_t>("mem_data_index");

    if (_sKernelInstanceName == ipLayout[ipLayoutIndex].get<std::string>("m_name")) {
      _pMemTopology.add_child("mem_data", memTopology[memDataIndex]);
    }
  }
}


void
FormattedOutput::printHeader(std::ostream &_ostream, const axlf &_xclBinHeader, const std::vector<Section*> _sections) {

  boost::property_tree::ptree pt;

  std::string ipInstantName = "rtl_krnl_vadd_const:rtl_krnl_vadd_const_1";
  getKernelDDRMemory(ipInstantName, _sections, pt);
  XUtil::TRACE_PrintTree("Memory Section", pt);




  XUtil::TRACE("Printing Binary Header");

  _ostream << "OpenCL Binary Header\n";
  //_ostream << "  Magic                   : '" << getMagicAsString() << "'\n";
  //_ostream << "  Cipher                  : '" << getCipherAsString() << "'\n";
  //_ostream << "  Key Block               : '" << getKeyBlockAsString() << "'\n";
  //_ostream << "  Unique ID               : '" << getUniqueIdAsString() << "'\n";
  //_ostream << "  Size                    : '" << getSizeAsString() << "' bytes\n";
  _ostream << "  Time Stamp              : '" << getTimeStampAsString(_xclBinHeader) << "'\n";
  _ostream << "  Feature ROM Time Stamp  : '" << getFeatureRomTimeStampAsString(_xclBinHeader) << "'\n";
  _ostream << "  Version                 : '" << getVersionAsString(_xclBinHeader) << "'\n";
  _ostream << "  Mode                    : '" << getModeAsPrettyString(_xclBinHeader) << "' (" << getModeAsString(_xclBinHeader) << ")\n";
  _ostream << "  Feature ROM UUID        : '" << getFeatureRomUuidAsString(_xclBinHeader) << "'\n";
  _ostream << "  Platform VBNV           : '" << getPlatformVbnvAsString(_xclBinHeader) << "'\n";
  _ostream << "  OpenCL Binary UUID      : '" << getXclBinUuidAsString(_xclBinHeader) << "'\n";
  _ostream << "  Debug Bin               : '" << getDebugBinAsString(_xclBinHeader) << "'\n";
  _ostream << "  Section Count           : '" << _sections.size() << "'\n";

  std::string sSectionKind("");
  for (Section *pSection : _sections) {
    if (!sSectionKind.empty()) {
        sSectionKind += ", ";
    }
    sSectionKind += "'" + pSection->getSectionKindAsString() + "'";
  }

  _ostream << "  Sections present        : " << sSectionKind << "\n";
}

