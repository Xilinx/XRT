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
#include  <set>

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
    case XCLBIN_FLAT: 
      return "XCLBIN_FLAT";
      break;
    case XCLBIN_PR: 
      return "XCLBIN_PR";
      break;
    case XCLBIN_TANDEM_STAGE2: 
      return "XCLBIN_TANDEM_STAGE2";
      break;
    case XCLBIN_TANDEM_STAGE2_WITH_PR:
      return "XCLBIN_TANDEM_STAGE2_WITH_PR";
      break;
    case XCLBIN_HW_EMU: 
      return "XCLBIN_HW_EMU";
      break;
    case XCLBIN_SW_EMU: 
      return "XCLBIN_SW_EMU";
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
                                    boost::property_tree::ptree &_ptKernelInstance,
                                    boost::property_tree::ptree &_ptMemoryConnections)
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
  std::set<int> addedIndex;
  for (auto connection : connectivity) {
    unsigned int ipLayoutIndex = connection.get<uint32_t>("m_ip_layout_index");
    unsigned int memDataIndex = connection.get<uint32_t>("mem_data_index");

    if ((_sKernelInstanceName == ipLayout[ipLayoutIndex].get<std::string>("m_name")) &&
        (addedIndex.find(memDataIndex) == addedIndex.end())) {
      _ptMemoryConnections.add_child("mem_data", memTopology[memDataIndex]);
      addedIndex.insert(memDataIndex);
    }
  }

  // 4) Get the kernel information
  for (auto ipdata : ipLayout) {
    if (_sKernelInstanceName == ipdata.get<std::string>("m_name")) {
      _ptKernelInstance.add_child("ip_data", ipdata);
      break;
    }
  }
}


void
FormattedOutput::printHeader(std::ostream &_ostream, 
                             const axlf &_xclBinHeader, 
                             const std::vector<Section*> _sections) {

  XUtil::TRACE("Printing Binary Header");

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
    _ostream << "\nNo build metadata section present.\n";
  } else {
    boost::property_tree::ptree pt;
    pBuildSection->getPayload(pt);
    
    _ostream << "\nTool Version\n------------\n";
    { 
      std::string sName = pt.get<std::string>("build_metadata.xclbin.packaged_by.name", "UnknownName");
      std::string sVersion = pt.get<std::string>("build_metadata.xclbin.packaged_by.version", "UnknownVersion");
      std::string sHash = pt.get<std::string>("build_metadata.xclbin.packaged_by.hash", "0");
      std::string sTimeStamp = pt.get<std::string>("build_metadata.xclbin.packaged_by.time_stamp", "UnknownTime");
      _ostream << "XCLBIN packaged by " << sName << " "
        << sVersion << " (Built: " << sTimeStamp << " - Hash " << sHash << ")\n";
    }
    {
      std::string sName = pt.get<std::string>("build_metadata.xclbin.generated_by.name", "UnknownName");
      std::string sVersion = pt.get<std::string>("build_metadata.xclbin.generated_by.version", "UnknownVersion");
      std::string sCl = pt.get<std::string>("build_metadata.xclbin.generated_by.cl", "UnknownCl");
      std::string sTime = pt.get<std::string>("build_metadata.xclbin.generated_by.time_stamp", "UnknownTime");
      _ostream << "XCLBIN generated by " << sName << " " << sVersion 
        << " (Built: " << sTime << " - CL " << sCl << ")\n";
    }

    _ostream << "\nXOCC Link Command Line\n----------------------\n";
    {
      std::string sCommandLine = pt.get<std::string>("build_metadata.xclbin.generated_by.options", "UnknownCommand");
     
      _ostream << sCommandLine << std::endl;
      const std::string delimiters = " -";      // Our delimiter

      // Working variables
      std::string::size_type pos = 0;
      std::string::size_type lastPos = 0;
      std::vector<std::string> commandAndOptions;

      // Parse the string until the entire string has been parsed or 3 tokens have been found
      while(true)  
      {
         pos = sCommandLine.find(delimiters, lastPos);
         std::string token;

         if (pos == std::string::npos) {
            pos = sCommandLine.length();
            commandAndOptions.push_back(sCommandLine.substr(lastPos, pos-lastPos));
            break;
         }

         commandAndOptions.push_back(sCommandLine.substr(lastPos, pos-lastPos));
         lastPos = ++pos ;
      }

      _ostream << "xocc command: " << commandAndOptions[0] << std::endl;
      for (unsigned int index = 1; index < commandAndOptions.size(); ++index) {
        _ostream << "          " << commandAndOptions[index] << std::endl;
      }
    }

    _ostream << "\nPlatform / Shell Build Information\n------------------------------\n";
    { 
      std::string sProductName = pt.get<std::string>("build_metadata.dsa.generated_by.name", "UnknownName");
      std::string sVersion = pt.get<std::string>("build_metadata.dsa.generated_by.version", "UnknownVersion");
      std::string sCl = pt.get<std::string>("build_metadata.dsa.generated_by.cl", "0");
      std::string sTimeStamp = pt.get<std::string>("build_metadata.dsa.generated_by.time_stamp", "UnknownTime");
      _ostream << "Shell generated by " << sProductName << " " 
        << sVersion << " (Built: " << sTimeStamp << " - CL " << sCl << ")\n";

      std::string sVendor = pt.get<std::string>("build_metadata.dsa.vendor", "UnknownVendor");
      _ostream << "Shell Vendor:    " << sVendor << "\n";
      std::string sBoardId = pt.get<std::string>("build_metadata.dsa.board_id", "UnknownBoardId");
      _ostream << "Shell Board ID:  " << sBoardId << "\n";
      std::string sMajor = pt.get<std::string>("build_metadata.dsa.version_major", "UnknownMajorVersion");
      std::string sMinor = pt.get<std::string>("build_metadata.dsa.version_minor", "UnknownMinorVersion");
      _ostream << "Shell Version:   " << sMajor << "." << sMinor << "\n";
    }

    _ostream << "\nKernels\n-------\n";
    {
      boost::property_tree::ptree regions = pt.get_child("build_metadata.xclbin.user_regions");
      for (const auto & region : regions) {
        //std::string sRegionName = region.second.get<std::string>("name");
        //_ostream << "Dynamic Region: " << sRegionName << "\n";
        boost::property_tree::ptree kernels = region.second.get_child("kernels");
        for (const auto & kernel : kernels) {
          std::string sKernelName = kernel.second.get<std::string>("name");
          boost::property_tree::ptree instances = kernel.second.get_child("instances");
          for (const auto & instance : instances) {
            std::string sInstanceName = instance.second.get<std::string>("name");
            std::string sKernelInstanceName = sKernelName + ":" + sInstanceName;
            _ostream << "Name:Instance - " << sKernelInstanceName << "\n";

            boost::property_tree::ptree memoryConnections;
            boost::property_tree::ptree kernelInstance;
            getKernelDDRMemory(sKernelInstanceName, _sections, kernelInstance, memoryConnections);
            _ostream << "  Base Addresses:    " << kernelInstance.get<std::string>("ip_data.m_base_address") << std::endl;
            for (const auto& kv : memoryConnections) {
              boost::property_tree::ptree ptMemData = kv.second;
              std::string sType = ptMemData.get<std::string>("m_type");
              std::string sTag = ptMemData.get<std::string>("m_tag");
              _ostream << "  Memory Connection: " << sType << ":" << sTag << std::endl;
            }
          }
          _ostream << "\n";
        }
      }
    }
  }

  // Clock Frequency Topology Printing
  if (pClockSection == nullptr) {
    _ostream << "\nNo clock metadata section present.\n";
  } else {
    _ostream << "\nClock Information\n-----------------\n";
    boost::property_tree::ptree pt;
    pClockSection->getPayload(pt);
    
    { // clock_freq_topology
      boost::property_tree::ptree frequencies = pt.get_child("clock_freq_topology.m_clock_freq");
      for (const auto & frequency : frequencies) {
        std::string sName = frequency.second.get<std::string>("m_name", "MissingName");
        std::string sType = frequency.second.get<std::string>("m_type", "MissingType");
        std::string sFreqMhz = frequency.second.get<std::string>("m_freq_Mhz", "MissingFrequency");
        //std::string sOriginalName = frequency.second.get<std::string>("m_original_name", "");
        _ostream << "Clock " << sName << " (" << sType << ") has frequency: " << sFreqMhz << " MHz\n";
      }
    }
  }
  
  _ostream << "\nBinary Header\n-------------\n";
  _ostream << "Time Stamp:               '" << getTimeStampAsString(_xclBinHeader) << "'\n";
  _ostream << "Feature ROM Time Stamp:   '" << getFeatureRomTimeStampAsString(_xclBinHeader) << "'\n";
  _ostream << "Version:                  '" << getVersionAsString(_xclBinHeader) << "'\n";
  _ostream << "Mode:                     '" << getModeAsPrettyString(_xclBinHeader) << "' (" << getModeAsString(_xclBinHeader) << ")\n";
  _ostream << "Feature ROM UUID:         '" << getFeatureRomUuidAsString(_xclBinHeader) << "'\n";
  _ostream << "Platform VBNV:            '" << getPlatformVbnvAsString(_xclBinHeader) << "'\n";
  _ostream << "OpenCL Binary UUID:       '" << getXclBinUuidAsString(_xclBinHeader) << "'\n";
  _ostream << "Debug Bin:                '" << getDebugBinAsString(_xclBinHeader) << "'\n";
  _ostream << "Section Count:            '" << _sections.size() << "'\n";
  std::string sSectionKind("");
  for (Section *pSection : _sections) {
    if (!sSectionKind.empty()) {
        sSectionKind += ", ";
    }
    sSectionKind += "'" + pSection->getSectionKindAsString() + "'";
  }
  _ostream << "Sections present:         " << sSectionKind << "\n";

}

