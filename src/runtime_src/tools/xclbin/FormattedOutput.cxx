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

void
FormattedOutput::printHeader(std::ostream &_ostream, const axlf &_xclBinHeader, const std::vector<Section*> _sections) {
  XUtil::TRACE("Printing Binary Header");

  _ostream << "Binary Header\n";
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

  // Section Splitting
  Section *pBuildSection = nullptr;
  Section *pClockSection = nullptr;
  for (Section *pSection : _sections) {
    if (pSection->getSectionKind() == BUILD_METADATA) {
      pBuildSection = pSection;
    }
    if (pSection->getSectionKind() == CLOCK_FREQ_TOPOLOGY) {
      pClockSection = pSection;
    }
  }

  // Build Metadata Printing
  if (pBuildSection == nullptr) {
    std::cout << "No build metadata section present.\n";
  } else {
    std::cout << "Build Information\n";
    boost::property_tree::ptree pt;
    pBuildSection->getPayload(pt);
    
    { // xclbin
      std::string sProductName = pt.get<std::string>("build_metadata.xclbin.packaged_by.name", "UnknownName");
      std::string sVersion = pt.get<std::string>("build_metadata.xclbin.packaged_by.version", "UnknownVersion");
      std::string sHash = pt.get<std::string>("build_metadata.xclbin.packaged_by.hash", "0");
      std::string sTimeStamp = pt.get<std::string>("build_metadata.xclbin.packaged_by.time_stamp", "UnknownTime");
      std::cout << "  XCLBIN packaged by " << sProductName << " "
        << sVersion << " (Built: " << sTimeStamp << " - Hash " << sHash << ")\n";
    }

    { // dsa
      std::string sProductName = pt.get<std::string>("build_metadata.dsa.generated_by.name", "UnknownName");
      std::string sVersion = pt.get<std::string>("build_metadata.dsa.generated_by.version", "UnknownVersion");
      std::string sCl = pt.get<std::string>("build_metadata.dsa.generated_by.cl", "0");
      std::string sTimeStamp = pt.get<std::string>("build_metadata.dsa.generated_by.time_stamp", "UnknownTime");
      std::cout << "  DSA generated by " << sProductName << " " 
        << sVersion << " (Built: " << sTimeStamp << " - CL " << sCl << ")\n";

      std::string sVendor = pt.get<std::string>("build_metadata.dsa.vendor", "UnknownVendor");
      std::cout << "  DSA Vendor   : " << sVendor << "\n";
      std::string sBoardId = pt.get<std::string>("build_metadata.dsa.board_id", "UnknownBoardId");
      std::cout << "  DSA Board ID : " << sBoardId << "\n";
      std::string sMajor = pt.get<std::string>("build_metadata.dsa.version_major", "UnknownMajorVersion");
      std::string sMinor = pt.get<std::string>("build_metadata.dsa.version_minor", "UnknownMinorVersion");
      std::cout << "  DSA Version  : " << sMajor << "." << sMinor << "\n";

      boost::property_tree::ptree regions = pt.get_child("build_metadata.xclbin.user_regions");
      for (const auto & region : regions) {
        std::string sRegionName = region.second.get<std::string>("name");
        std::cout << "  Custom Logic Region: " << sRegionName << "\n";
        boost::property_tree::ptree kernels = region.second.get_child("kernels");
        for (const auto & kernel : kernels) {
          std::string sKernelName = kernel.second.get<std::string>("name");
          std::cout << "    Kernel: " << sKernelName << "\n";
          boost::property_tree::ptree instances = kernel.second.get_child("instances");
          std::cout << "    Instances: ";
          for (const auto & instance : instances) {
            std::string sInstanceName = instance.second.get<std::string>("name");
            std::cout << sInstanceName << " ";
          }
          std::cout << "\n";
        }
      }
    }
  }

  // Clock Frequency Topology Printing
  if (pClockSection == nullptr) {
    std::cout << "No clock metadata section present.\n";
  } else {
    std::cout << "Clock Information\n";
    boost::property_tree::ptree pt;
    pClockSection->getPayload(pt);
    
    { // clock_freq_topology
      boost::property_tree::ptree frequencies = pt.get_child("clock_freq_topology.m_clock_freq");
      for (const auto & frequency : frequencies) {
        std::string sName = frequency.second.get<std::string>("m_name", "MissingName");
        std::string sType = frequency.second.get<std::string>("m_type", "MissingType");
        std::string sFreqMhz = frequency.second.get<std::string>("m_freq_Mhz", "MissingFrequency");
        //std::string sOriginalName = frequency.second.get<std::string>("m_original_name", "");
        std::cout << "  Clock " << sName << " (" << sType << ") has frequency: " << sFreqMhz << " MHz\n";
      }
    }
  }

//1) XRT Tool Version information (e.g., the version print command)
//2) Vivado / SDx Version information and options used by xocc.
//3) Platform used (and its version information)
//4) CU Region and the Kernels associated with it.
//5) Clock Frequency
//6) Packaged By
//+ Should include Kernel name, lanugage, and VLNV
  
}

