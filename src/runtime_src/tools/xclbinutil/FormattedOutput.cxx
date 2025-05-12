/**
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or 
 * implied. See the License for the specific language governing 
 * permissions and limitations under the License. 
 */

#include "FormattedOutput.h"
#include "Section.h"
#include "SectionBitstream.h"
#include "version.h"
#include "XclBinSignature.h"
#include "XclBinUtilities.h"
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include  <set>

namespace XUtil = XclBinUtilities;

std::string
FormattedOutput::getTimeStampAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%ld") % _xclBinHeader.m_header.m_timeStamp);
}

std::string
FormattedOutput::getFeatureRomTimeStampAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%d") % _xclBinHeader.m_header.m_featureRomTimeStamp);
}

std::string
FormattedOutput::getVersionAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%d.%d.%d") % _xclBinHeader.m_header.m_versionMajor % _xclBinHeader.m_header.m_versionMinor % _xclBinHeader.m_header.m_versionPatch);
}

// String Getters
std::string
FormattedOutput::getMagicAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%s") % _xclBinHeader.m_magic);
}

std::string
FormattedOutput::getSignatureLengthAsString(const axlf& _xclBinHeader)
{
  std::string sTemp("");
  XUtil::binaryBufferToHexString((unsigned char*)&_xclBinHeader.m_signature_length, sizeof(_xclBinHeader.m_signature_length), sTemp);
  return sTemp; // TBD: "0x" + sTemp; ? do the others too...
}

std::string
FormattedOutput::getKeyBlockAsString(const axlf& _xclBinHeader)
{
  std::string sTemp("");
  XUtil::binaryBufferToHexString((unsigned char*)&_xclBinHeader.m_keyBlock, sizeof(_xclBinHeader.m_keyBlock), sTemp);
  return sTemp;
}

std::string
FormattedOutput::getUniqueIdAsString(const axlf& _xclBinHeader)
{
  std::string sTemp("");
  XUtil::binaryBufferToHexString((unsigned char*)&_xclBinHeader.m_uniqueId, sizeof(_xclBinHeader.m_uniqueId), sTemp);
  return sTemp;
}

std::string
getSizeAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%ld") % _xclBinHeader.m_header.m_length);
}

std::string
FormattedOutput::getModeAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%d") %  _xclBinHeader.m_header.m_mode);
}

std::string
getModeAsPrettyString(const axlf& _xclBinHeader)
{
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
FormattedOutput::getInterfaceUuidAsString(const axlf& _xclBinHeader)
{
  std::string sTemp("");
  XUtil::binaryBufferToHexString(_xclBinHeader.m_header.m_interface_uuid, sizeof(axlf_header::m_interface_uuid), sTemp);
  return sTemp;
}

std::string
FormattedOutput::getPlatformVbnvAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%s") % _xclBinHeader.m_header.m_platformVBNV);
}

std::string
FormattedOutput::getXclBinUuidAsString(const axlf& _xclBinHeader)
{
  std::string sTemp("");
  XUtil::binaryBufferToHexString(_xclBinHeader.m_header.uuid, sizeof(axlf_header::uuid), sTemp);
  return sTemp;
}

std::string
FormattedOutput::getDebugBinAsString(const axlf& _xclBinHeader)
{
  return boost::str(boost::format("%s") % _xclBinHeader.m_header.m_debug_bin);
}


void
FormattedOutput::getKernelDDRMemory(const std::string& _sKernelInstanceName,
                                    const std::vector<Section*> _sections,
                                    boost::property_tree::ptree& _ptKernelInstance,
                                    boost::property_tree::ptree& _ptMemoryConnections)
{
  if (_sKernelInstanceName.empty()) {
    return;
  }

  // 1) Look for our sections section
  Section* pMemTopology = nullptr;
  Section* pConnectivity = nullptr;
  Section* pIPLayout = nullptr;

  for (auto pSection : _sections) {
    if (MEM_TOPOLOGY == pSection->getSectionKind()) {
      pMemTopology = pSection;
    } else if (CONNECTIVITY == pSection->getSectionKind()) {
      pConnectivity = pSection;
    } else if (IP_LAYOUT == pSection->getSectionKind()) {
      pIPLayout = pSection;
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
  auto memTopology = XUtil::as_vector<boost::property_tree::ptree>(ptMemTopology, "m_mem_data");

  boost::property_tree::ptree& ptConnectivity = ptSections.get_child("connectivity");
  auto connectivity = XUtil::as_vector<boost::property_tree::ptree>(ptConnectivity, "m_connection");

  boost::property_tree::ptree& ptIPLayout = ptSections.get_child("ip_layout");
  auto ipLayout = XUtil::as_vector<boost::property_tree::ptree>(ptIPLayout, "m_ip_data");

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
reportBuildVersion(std::ostream& _ostream)
{
  _ostream << boost::format("%17s: %s (%s)\n") % "XRT Build Version" % xrt_build_version % xrt_build_version_branch;
  _ostream << boost::format("%17s: %s\n") % "Build Date" % xrt_build_version_date;
  _ostream << boost::format("%17s: %s\n") % "Hash ID" % xrt_build_version_hash;
}

void
FormattedOutput::reportVersion(bool bShort)
{
  if (bShort == true) {
    reportBuildVersion(std::cout);
  } else {
    xrt::version::print(std::cout);
  }
}


void
reportXclbinInfo(std::ostream& _ostream,
                 const std::string& _sInputFile,
                 const axlf& _xclBinHeader,
                 boost::property_tree::ptree& _ptMetaData,
                 const std::vector<Section*> _sections)
{
  std::string sSignatureState;

  // Look for the PKCS signature first
  if (!_sInputFile.empty()) {
    try {
      XclBinPKCSImageStats xclBinPKCSStats = { 0 };
      getXclBinPKCSStats(_sInputFile, xclBinPKCSStats);

      if (xclBinPKCSStats.is_PKCS_signed) {
        sSignatureState = (boost::format("Present - Signed PKCS - Offset: 0x%lx, Size: 0x%lx") % xclBinPKCSStats.signature_offset % xclBinPKCSStats.signature_size).str();
      }
    } catch (...) {
      // Do nothing
    }
  }

  // Calculate if the signature is present or not because this is a slow
  {
    if (!_sInputFile.empty() && sSignatureState.empty()) {
      std::fstream inputStream;
      inputStream.open(_sInputFile, std::ifstream::in | std::ifstream::binary);
      if (inputStream.is_open()) {
        std::string sSignature;
        std::string sSignedBy;
        unsigned int totalSize;
        if (XUtil::getSignature(inputStream, sSignature, sSignedBy, totalSize)) {
          sSignatureState = "Present - " + sSignature;
        }
      }
      inputStream.close();
    }
  }


  _ostream << "xclbin Information\n";
  _ostream << "------------------\n";

  // Generated By:
  {
    auto sTool = _ptMetaData.get<std::string>("xclbin.generated_by.name", "");
    auto sVersion = _ptMetaData.get<std::string>("xclbin.generated_by.version", "");
    auto sTimeStamp = _ptMetaData.get<std::string>("xclbin.generated_by.time_stamp", "");
    auto sGeneratedBy = (boost::format("%s (%s) on %s") % sTool % sVersion % sTimeStamp).str();
    if (sTool.empty()) {
      sGeneratedBy = "<unknown>";
    }

    _ostream << boost::format("   %-23s %s\n") % "Generated by:" 
                                               % sGeneratedBy;
  }

  // Version:
  _ostream << boost::format("   %-23s %d.%d.%d\n") % "Version:" 
                                                   % (unsigned int)_xclBinHeader.m_header.m_versionMajor 
                                                   % (unsigned int) _xclBinHeader.m_header.m_versionMinor 
                                                   % (unsigned int) _xclBinHeader.m_header.m_versionPatch;

  // Kernels
  {
    std::string sKernels;
    if (!_ptMetaData.empty()) {
      boost::property_tree::ptree& ptXclBin = _ptMetaData.get_child("xclbin");
      auto userRegions = XUtil::as_vector<boost::property_tree::ptree>(ptXclBin, "user_regions");
      for (auto& userRegion : userRegions) {
        auto kernels = XUtil::as_vector<boost::property_tree::ptree>(userRegion, "kernels");
        for (auto& kernel : kernels) {
          auto sKernel = kernel.get<std::string>("name", "");
          if (sKernel.empty()) {
            continue;
          }
          if (!sKernels.empty()) {
            sKernels += ", ";
          }
          sKernels += sKernel;
        }
      }
    } else {
      sKernels = "<unknown>";
    }
    _ostream << boost::format("   %-23s %s\n") % "Kernels:" 
                                               % sKernels;
  }

  // Signature
  {
    _ostream << boost::format("   %-23s %s\n") % "Signature:" 
                                               % sSignatureState;
  }

  // Action Masks
  {
    // Are there any masks set
    if (_xclBinHeader.m_header.m_actionMask) {
      _ostream << boost::format("   %-23s ") % "Action Mask(s):";
      if (_xclBinHeader.m_header.m_actionMask & AM_LOAD_AIE) 
        _ostream << "LOAD_AIE ";
      if (_xclBinHeader.m_header.m_actionMask & AM_LOAD_PDI)
        _ostream << "LOAD_PDI ";      
      _ostream << std::endl;
    }
  }

  // Content
  {
    std::string sContent;
    for (auto pSection : _sections) {
      if (pSection->getSectionKind() == BITSTREAM) {
        SectionBitstream* pBitstreamSection = static_cast<SectionBitstream*>(pSection);
        sContent = pBitstreamSection->getContentTypeAsString();
        break;
      }
    }
    _ostream << boost::format("   %-23s %s\n") % "Content:" 
                                               % sContent;
  }

  {
    std::string sUUID = XUtil::getUUIDAsString(_xclBinHeader.m_header.uuid);
    _ostream << boost::format("   %-23s %s\n") % "UUID (xclbin):" 
                                               % sUUID;
  }

  {
    // Get the PARTITION_METADATA property tree (if there is one)
    for (auto pSection : _sections) {
      if (pSection->getSectionKind() != PARTITION_METADATA) {
        continue;
      }

      // Get the complete JSON metadata tree
      boost::property_tree::ptree ptRoot;
      pSection->getPayload(ptRoot);
      if (ptRoot.empty()) {
        continue;
      }

      // Look for the "partition_metadata" node
      boost::property_tree::ptree ptPartitionMetadata = ptRoot.get_child("partition_metadata");
      if (ptPartitionMetadata.empty()) {
        continue;
      }

      // Look for the "interfaces" node
      boost::property_tree::ptree ptInterfaces = ptPartitionMetadata.get_child("interfaces");
      if (ptInterfaces.empty()) {
        continue;
      }

      // Report all of the interfaces
      for (const auto& kv : ptInterfaces) {
        boost::property_tree::ptree ptInterface = kv.second;
        auto sUUID = ptInterface.get<std::string>("interface_uuid", "");
        if (!sUUID.empty()) {
          _ostream << boost::format("   %-23s %s\n") % "UUID (IINTF):" 
                                                     % sUUID;
        }
      }
    }
  }

  // Sections
  {
    std::string sSections;
    for (Section* pSection : _sections) {
      std::string sKindStr = pSection->getSectionKindAsString();

      if (!sSections.empty()) {
        sSections += ", ";
      }

      sSections += sKindStr;

      if (!pSection->getSectionIndexName().empty()) {
        sSections += "[" + pSection->getSectionIndexName() + "]";
      }
    }

    std::vector<std::string> sections;
    const unsigned int wrapLength = 54;
    std::string::size_type lastPos = 0;
    std::string::size_type pos = 0;
    std::string delimiters = ", ";

    while (pos != std::string::npos)  {
      pos = sSections.find(delimiters, lastPos);
      std::string token;

      if (pos == std::string::npos) {
        token = sSections.substr(lastPos, (sSections.length() - lastPos));
      } else {
        token = sSections.substr(lastPos, pos - lastPos + 2);
      }

      if (sections.empty()) {
        sections.push_back(token);
      } else {
        unsigned int index = (unsigned int)sections.size() - 1;
        if ((sections[index].length() + token.length()) > wrapLength) {
          sections.push_back(token);
        } else {
          sections[index] += token;
        }
      }

      lastPos = pos + 2;
    }

    for (unsigned index = 0; index < sections.size(); ++index) {
      if (index == 0) {
        _ostream << boost::format("   %-23s %s\n") % "Sections:" 
                                                   % sections[index];
      } else {
        _ostream << boost::format("   %-23s %s\n") % "" 
                                                   % sections[index];
      }
    }
  }
}

/*
 * get string value from boost:property_tree, first try platform.<name>
 * then try dsa.<name>.
 */
std::string
getPTreeValue(boost::property_tree::ptree& _ptMetaData, std::string name)
{
  auto result = _ptMetaData.get<std::string>("platform." + name, "--");
  if (result.compare("--") == 0)
    result = _ptMetaData.get<std::string>("dsa." + name, "--");

  return result;
}

void
reportHardwarePlatform(std::ostream& _ostream,
                       const axlf& _xclBinHeader,
                       boost::property_tree::ptree& _ptMetaData)
{
  _ostream << "Hardware Platform (Shell) Information\n";
  _ostream << "-------------------------------------\n";

  if (!_ptMetaData.empty()) {
    // Vendor
    {
      std::string sVendor = getPTreeValue(_ptMetaData, "vendor");
      _ostream << boost::format("   %-23s %s\n") % "Vendor:" % sVendor;
    }

    // Board
    {
      std::string sName = getPTreeValue(_ptMetaData, "board_id");
      _ostream << boost::format("   %-23s %s\n") % "Board:" % sName;
    }

    // Name
    {
      std::string sName = getPTreeValue(_ptMetaData, "name");
      _ostream << boost::format("   %-23s %s\n") % "Name:" % sName;
    }

    // Version
    {
      std::string sVersion = getPTreeValue(_ptMetaData, "version_major")
          + "."
          + getPTreeValue(_ptMetaData, "version_minor");
      _ostream << boost::format("   %-23s %s\n") % "Version:" % sVersion;
    }

    // Generated Version
    {
      std::string sGeneratedVersion = getPTreeValue(_ptMetaData, "generated_by.name") + " "
          + getPTreeValue(_ptMetaData, "generated_by.version")
          + " (SW Build: "
          + getPTreeValue(_ptMetaData, "generated_by.cl");
      std::string sIPCL = getPTreeValue(_ptMetaData, "generated_by.ip_cl");
      if (sIPCL.compare("--") != 0) {
        sGeneratedVersion += "; " + sIPCL;
      }
      sGeneratedVersion += ")";

      _ostream << boost::format("   %-23s %s\n") % "Generated Version:" % sGeneratedVersion;
    }

    // Created
    {
      std::string sCreated = getPTreeValue(_ptMetaData, "generated_by.time_stamp");
      _ostream << boost::format("   %-23s %s") % "Created:\n" % sCreated;
    }

    // FPGA Device
    {
      std::string sFPGADevice = getPTreeValue(_ptMetaData, "board.part");
      if (sFPGADevice.compare("--") != 0) {
        std::string::size_type pos = sFPGADevice.find("-", 0);

        if (pos == std::string::npos) {
          sFPGADevice = "--";
        } else {
          sFPGADevice = sFPGADevice.substr(0, pos);
        }
      }

      _ostream << boost::format("   %-23s %s\n") % "FPGA Device:" % sFPGADevice;
    }

    // Board Vendor
    {
      std::string sBoardVendor = getPTreeValue(_ptMetaData, "board.vendor");
      _ostream << boost::format("   %-23s %s\n") % "Board Vendor:" % sBoardVendor;
    }

    // Board Name
    {
      std::string sBoardName = getPTreeValue(_ptMetaData, "board.name");
      _ostream << boost::format("   %-23s %s\n") % "Board Name:" % sBoardName;
    }

    // Board Part
    {
      std::string sBoardPart = getPTreeValue(_ptMetaData, "board.board_part");
      _ostream << boost::format("   %-23s %s\n") % "Board Part:" % sBoardPart;
    }
  }

  // Platform VBNV
  {
    std::string sPlatformVBNV = (char*)_xclBinHeader.m_header.m_platformVBNV;
    if (sPlatformVBNV.empty()) {
      sPlatformVBNV = "<not defined>";
    }
    _ostream << boost::format("   %-23s %s\n") % "Platform VBNV:" % sPlatformVBNV;
  }

  // Static UUID
  {
    std::string sStaticUUID = XUtil::getUUIDAsString(_xclBinHeader.m_header.m_interface_uuid);
    _ostream << boost::format("   %-23s %s\n") % "Static UUID:" % sStaticUUID;
  }

  // TimeStamp
  {
    _ostream << boost::format("   %-23s %ld\n") % "Feature ROM TimeStamp:" % _xclBinHeader.m_header.m_featureRomTimeStamp;
  }


}


void
reportClocks(std::ostream& _ostream,
             const std::vector<Section*> _sections)
{
  boost::property_tree::ptree ptEmpty;

  _ostream << "Scalable Clocks\n";
  _ostream << "---------------\n";

  boost::property_tree::ptree ptClockFreqTopology;
  for (Section* pSection : _sections) {
    if (pSection->getSectionKind() == CLOCK_FREQ_TOPOLOGY) {
      boost::property_tree::ptree pt;
      pSection->getPayload(pt);
      if (!pt.empty()) {
        ptClockFreqTopology = pt.get_child("clock_freq_topology");
      }
      break;
    }
  }

  if (ptClockFreqTopology.empty()) {
    _ostream << "   No scalable clock data available.\n";
  }

  auto clockFreqs = XUtil::as_vector<boost::property_tree::ptree>(ptClockFreqTopology, "m_clock_freq");
  for (unsigned int index = 0; index < clockFreqs.size(); ++index) {
    boost::property_tree::ptree& ptClockFreq = clockFreqs[index];
    auto sName = ptClockFreq.get<std::string>("m_name");
    auto sType = ptClockFreq.get<std::string>("m_type");
    auto sFreqMhz = ptClockFreq.get<std::string>("m_freq_Mhz");

    _ostream << boost::format("   %-10s %s\n") % "Name:" % sName;
    _ostream << boost::format("   %-10s %d\n") % "Index:" % index;
    _ostream << boost::format("   %-10s %s\n") % "Type:" % sType;
    _ostream << boost::format("   %-10s %s MHz\n") % "Frequency:" % sFreqMhz;

    if (&ptClockFreq != &clockFreqs.back()) {
      _ostream << std::endl;
    }
  }

  // the following information is from SYSTEM_METADATA section (system diagram json)
  _ostream << "\n"
      << "System Clocks\n"
      << "------\n";
  // system_diagram_metadata
  //   xsa
  //     clocks
  // example of clocks info in SYSTEM_METADATA
  //   "clocks": [
  //    {
  //      "name": "CPU",
  //      "orig_name": "CPU",
  //      "id": -1,
  //      "default": false,
  //      "type": "RESERVED",
  //      "spec_frequency": 1200,
  //      "spec_period": 0.833333,
  //      "requested_frequency": 0,
  //      "achieved_frequency": 0,
  //      "clock_domain": "",
  //      "inst_ref": "",
  //      "comp_ref": "",
  //      "xclbin_name": ""
  //    },
  //    ...
  boost::property_tree::ptree ptXsa;
  for (const auto& pSection : _sections) {
    if (pSection->getSectionKind() == SYSTEM_METADATA) {
      boost::property_tree::ptree pt;
      pSection->getPayload(pt);
      if (!pt.empty()) {
        boost::property_tree::ptree ptSysDiaMet = pt.get_child("system_diagram_metadata", ptEmpty);
        if (!ptSysDiaMet.empty())
          ptXsa = ptSysDiaMet.get_child("xsa", ptEmpty);
      }
      break;
    }
  }

  auto clocks = XUtil::as_vector<boost::property_tree::ptree>(ptXsa, "clocks");
  if (clocks.empty()) {
    _ostream << "   No system clock data available.\n";
    return;
  }

  for (const auto& ptClock : clocks) {
    auto sName = ptClock.get<std::string>("orig_name", "");
    auto sType = ptClock.get<std::string>("type", "");
    auto sSpecFreq = ptClock.get<std::string>("spec_frequency", "");
    auto sRequestedFreq = ptClock.get<std::string>("requested_frequency", "");
    auto sAchievedFreq = ptClock.get<std::string>("achieved_frequency", "");

    // skip CPU clocks (type = RESERVED)
    if (boost::iequals(sType, "RESERVED"))
      continue;

    boost::format systemClockFmt("   %-15s %s %s\n");
    _ostream << systemClockFmt % "Name:" % sName % "";
    _ostream << systemClockFmt % "Type:" % sType % "";
    _ostream << systemClockFmt % "Default Freq:" % sSpecFreq % "MHz";

    // display requested and achieved frequency only for scalable clocks
    if (boost::iequals(sType, "SCALABLE")) {
      _ostream << systemClockFmt % "Requested Freq:" % sRequestedFreq % "MHz";
      _ostream << systemClockFmt % "Achieved Freq:" % sAchievedFreq % "MHz";
    }

    if (&ptClock != &clocks.back())
      _ostream << std::endl;
  }
}

void
reportMemoryConfiguration(std::ostream& _ostream,
                          const std::vector<Section*> _sections)
{
  _ostream << "Memory Configuration\n";
  _ostream << "--------------------\n";

  boost::property_tree::ptree ptMemTopology;
  for (Section* pSection : _sections) {
    if (pSection->getSectionKind() == MEM_TOPOLOGY) {
      boost::property_tree::ptree pt;
      pSection->getPayload(pt);
      if (!pt.empty()) {
        ptMemTopology = pt.get_child("mem_topology");
      }
      break;
    }
  }

  if (ptMemTopology.empty()) {
    _ostream << "   No memory configuration data available.\n";
    return;
  }

  auto memDatas = XUtil::as_vector<boost::property_tree::ptree>(ptMemTopology, "m_mem_data");
  for (unsigned int index = 0; index < memDatas.size(); ++index) {
    boost::property_tree::ptree& ptMemData = memDatas[index];

    auto sName = ptMemData.get<std::string>("m_tag");
    auto sType = ptMemData.get<std::string>("m_type");
    auto sBaseAddress = ptMemData.get<std::string>("m_base_address");
    auto sAddressSizeKB = ptMemData.get<std::string>("m_sizeKB");
    uint64_t addressSize = XUtil::stringToUInt64(sAddressSizeKB) * 1024;
    auto sUsed = ptMemData.get<std::string>("m_used");

    std::string sBankUsed = "No";

    if (sUsed != "0") {
      sBankUsed = "Yes";
    }

    _ostream << boost::format("   %-13s %s\n") % "Name:" % sName;
    _ostream << boost::format("   %-13s %d\n") % "Index:" % index;
    _ostream << boost::format("   %-13s %s\n") % "Type:" % sType;
    _ostream << boost::format("   %-13s %s\n") % "Base Address:" % sBaseAddress;
    _ostream << boost::format("   %-13s 0x%lx\n") % "Address Size:" % addressSize;
    _ostream << boost::format("   %-13s %s\n") % "Bank Used:" % sBankUsed;

    if (&ptMemData != &memDatas.back()) {
      _ostream << std::endl;
    }
  }
}

void
reportKernels(std::ostream& _ostream,
              boost::property_tree::ptree& _ptMetaData,
              const std::vector<Section*> _sections)
{
  if (_ptMetaData.empty()) {
    _ostream << "   No kernel metadata available.\n";
    return;
  }

  // Cross reference data
  std::vector<boost::property_tree::ptree> memTopology;
  std::vector<boost::property_tree::ptree> connectivity;
  std::vector<boost::property_tree::ptree> ipLayout;

  for (auto pSection : _sections) {
    boost::property_tree::ptree pt;
    if (MEM_TOPOLOGY == pSection->getSectionKind()) {
      pSection->getPayload(pt);
      memTopology = XUtil::as_vector<boost::property_tree::ptree>(pt.get_child("mem_topology"), "m_mem_data");
    } else if (CONNECTIVITY == pSection->getSectionKind()) {
      pSection->getPayload(pt);
      connectivity = XUtil::as_vector<boost::property_tree::ptree>(pt.get_child("connectivity"), "m_connection");
    } else if (IP_LAYOUT == pSection->getSectionKind()) {
      pSection->getPayload(pt);
      ipLayout = XUtil::as_vector<boost::property_tree::ptree>(pt.get_child("ip_layout"), "m_ip_data");
    }
  }

  boost::property_tree::ptree& ptXclBin = _ptMetaData.get_child("xclbin");
  auto userRegions = XUtil::as_vector<boost::property_tree::ptree>(ptXclBin, "user_regions");
  for (auto& userRegion : userRegions) {
    auto kernels = XUtil::as_vector<boost::property_tree::ptree>(userRegion, "kernels");
    if (kernels.size() == 0)
      _ostream << "Kernel(s): <None Found>\n";

    for (auto& ptKernel : kernels) {
      XUtil::TRACE_PrintTree("Kernel", ptKernel);

      auto sKernel = ptKernel.get<std::string>("name");
      _ostream << boost::format("%s %s\n") % "Kernel:" % sKernel;

      auto ports = XUtil::as_vector<boost::property_tree::ptree>(ptKernel, "ports");
      auto arguments = XUtil::as_vector<boost::property_tree::ptree>(ptKernel, "arguments");
      auto instances = XUtil::as_vector<boost::property_tree::ptree>(ptKernel, "instances");

      _ostream << std::endl;

      // Definition
      {
        _ostream << "Definition\n";
        _ostream << "----------\n";

        _ostream << "   Signature: " << sKernel << " (";
        for (auto& ptArgument : arguments) {
          auto sType = ptArgument.get<std::string>("type");
          auto sName = ptArgument.get<std::string>("name");

          _ostream << sType << " " << sName;
          if (&ptArgument != &arguments.back())
            _ostream << ", ";
        }
        _ostream << ")\n";
      }

      _ostream << std::endl;

      // Ports
      {
        _ostream << "Ports\n";
        _ostream << "-----\n";

        for (auto& ptPort : ports) {
          auto sPort = ptPort.get<std::string>("name");
          auto sMode = ptPort.get<std::string>("mode");
          auto sRangeBytes = ptPort.get<std::string>("range");
          auto sDataWidthBits = ptPort.get<std::string>("data_width");
          auto sPortType = ptPort.get<std::string>("port_type");

          _ostream << boost::format("   %-14s %s\n") % "Port:" % sPort;
          _ostream << boost::format("   %-14s %s\n") % "Mode:" % sMode;
          _ostream << boost::format("   %-14s %s\n") % "Range (bytes):" % sRangeBytes;
          _ostream << boost::format("   %-14s %s bits\n") % "Data Width:" % sDataWidthBits;
          _ostream << boost::format("   %-14s %s\n") % "Port Type:" % sPortType;

          if (&ptPort != &ports.back()) {
            _ostream << std::endl;
          }
        }
      }

      _ostream << std::endl;

      // Instance
      for (auto& ptInstance : instances) {
        _ostream << "--------------------------\n";
        auto sInstance = ptInstance.get<std::string>("name");
        _ostream << boost::format("%-16s %s\n") % "Instance:" % sInstance;

        std::string sKernelInstance = sKernel + ":" + sInstance;

        // Base Address
        {
          std::string sBaseAddress = "--";
          for (auto& ptIPData : ipLayout) {
            if (ptIPData.get<std::string>("m_name") == sKernelInstance) {
              sBaseAddress = ptIPData.get<std::string>("m_base_address");
              break;
            }
          }
          _ostream << boost::format("   %-13s %s\n") % "Base Address:" % sBaseAddress;
        }

        _ostream << std::endl;

        // List the arguments
        for (unsigned int argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex) {
          boost::property_tree::ptree& ptArgument = arguments[argumentIndex];
          auto sArgument = ptArgument.get<std::string>("name");
          auto sOffset = ptArgument.get<std::string>("offset");
          auto sPort = ptArgument.get<std::string>("port");

          _ostream << boost::format("   %-18s %s\n") % "Argument:" % sArgument;
          _ostream << boost::format("   %-18s %s\n") % "Register Offset:" % sOffset;
          _ostream << boost::format("   %-18s %s\n") % "Port:" % sPort;

          // Find the memory connections
          bool bFoundMemConnection = false;
          for (auto& ptConnection : connectivity) {
            unsigned int ipIndex = ptConnection.get<unsigned int>("m_ip_layout_index");

            if (ipIndex >= ipLayout.size()) {
              auto errMsg = boost::format("ERROR: connectivity section 'm_ip_layout_index' (%d) exceeds the number of 'ip_layout' elements (%d).  This is usually an indication of curruptions in the xclbin archive.") % ipIndex % ipLayout.size();
              throw std::runtime_error(errMsg.str());
            }

            if (ipLayout[ipIndex].get<std::string>("m_name") == sKernelInstance) {
              if (ptConnection.get<unsigned int>("arg_index") == argumentIndex) {
                bFoundMemConnection = true;

                unsigned int memIndex = ptConnection.get<unsigned int>("mem_data_index");
                if (memIndex >= memTopology.size()) {
                  auto errMsg = boost::format("ERROR: connectivity section 'mem_data_index' (%d) exceeds the number of 'mem_topology' elements (%d).  This is usually an indication of curruptions in the xclbin archive.") % memIndex % memTopology.size();
                  throw std::runtime_error(errMsg.str());
                }

                auto sMemName = memTopology[memIndex].get<std::string>("m_tag");
                auto sMemType = memTopology[memIndex].get<std::string>("m_type");

                _ostream << boost::format("   %-18s %s (%s)\n") % "Memory:" % sMemName.c_str() % sMemType;
              }
            }
          }
          if (!bFoundMemConnection) {
            _ostream << boost::format("   %-18s <not applicable>\n") % "Memory:";
          }
          if (argumentIndex != (arguments.size() - 1)) {
            _ostream << std::endl;
          }
        }
        if (&ptInstance != &instances.back()) {
          _ostream << std::endl;
        }
      }
    }
  }
}

void
reportXOCC(std::ostream& _ostream,
           boost::property_tree::ptree& _ptMetaData)
{
  if (_ptMetaData.empty()) {
    _ostream << "   No information regarding the creation of the xclbin acceleration image.\n";
    return;
  }

  _ostream << "Generated By\n";
  _ostream << "------------\n";

  // Command
  auto sCommand = _ptMetaData.get<std::string>("xclbin.generated_by.name", "");

  if (sCommand.empty()) {
    _ostream << "   < Data not available >\n";
    return;
  }

  _ostream << boost::format("   %-14s %s\n") % "Command:" % sCommand;


  // Version
  {
    auto sVersion = _ptMetaData.get<std::string>("xclbin.generated_by.version", "--");
    auto sCL = _ptMetaData.get<std::string>("xclbin.generated_by.cl", "--");
    auto sTimeStamp = _ptMetaData.get<std::string>("xclbin.generated_by.time_stamp", "--");

    _ostream << boost::format("   %-14s %s - %s (SW BUILD: %s)\n") % "Version:" % sVersion % sTimeStamp % sCL;
  }

  auto sCommandLine = _ptMetaData.get<std::string>("xclbin.generated_by.options", "");

  // Command Line
  {
    std::string::size_type pos = sCommandLine.find(" ", 0);
    std::string sOptions;
    if (pos == std::string::npos) {
      sOptions = sCommandLine;
    } else {
      sOptions = sCommandLine.substr(pos + 1);
    }

    _ostream << boost::format("   %-14s %s %s\n") % "Command Line:" % sCommand % sOptions;
  }

  // Options
  {
    const std::string delimiters = " -";      // Our delimiter

    // Working variables
    std::string::size_type pos = 0;
    std::string::size_type lastPos = 0;
    std::vector<std::string> commandAndOptions;

    // Parse the string until the entire string has been parsed or 3 tokens have been found
    while (true)  {
      pos = sCommandLine.find(delimiters, lastPos);
      std::string token;

      if (pos == std::string::npos) {
        pos = sCommandLine.length();
        commandAndOptions.push_back(sCommandLine.substr(lastPos, pos - lastPos));
        break;
      }

      commandAndOptions.push_back(sCommandLine.substr(lastPos, pos - lastPos));
      lastPos = ++pos;
    }

    for (unsigned int index = 1; index < commandAndOptions.size(); ++index) {
      if (index == 1)
        _ostream << boost::format("   %-14s %s\n") % "Options:" % commandAndOptions[index];
      else
        _ostream << boost::format("   %-14s %s\n") % "" % commandAndOptions[index];
    }
  }
}

void
reportKeyValuePairs(std::ostream& _ostream,
                    const std::vector<Section*> _sections)
{
  _ostream << "User Added Key Value Pairs\n";
  _ostream << "--------------------------\n";

  std::vector<boost::property_tree::ptree> keyValues;

  for (Section* pSection : _sections) {
    if (pSection->getSectionKind() == KEYVALUE_METADATA) {
      boost::property_tree::ptree pt;
      pSection->getPayload(pt);
      keyValues = XUtil::as_vector<boost::property_tree::ptree>(pt.get_child("keyvalue_metadata"), "key_values");
      break;
    }
  }

  if (keyValues.empty()) {
    _ostream << "   <empty>\n";
    return;
  }

  for (unsigned int index = 0; index < keyValues.size(); ++index) {
    auto sKey = keyValues[index].get<std::string>("key");
    auto sValue = keyValues[index].get<std::string>("value");
    _ostream << boost::format("   %d) '%s' = '%s'\n") % (index + 1) %  sKey % sValue;
  }
}

void
reportAllJsonMetadata(std::ostream& _ostream,
                      const std::vector<Section*> _sections)
{
  _ostream << "JSON Metadata for Supported Sections\n";
  _ostream << "------------------------------------\n";

  boost::property_tree::ptree pt;
  for (Section* pSection : _sections) {
    std::string sectionName = pSection->getSectionKindAsString();
    XUtil::TRACE("Examining: '" + sectionName);
    pSection->getPayload(pt);
  }

  boost::property_tree::write_json(_ostream, pt, true /*Pretty print*/);
}


void
FormattedOutput::reportInfo(std::ostream& _ostream,
                            const std::string& _sInputFile,
                            const axlf& _xclBinHeader,
                            const std::vector<Section*> _sections,
                            bool _bVerbose)
{
  // Get the Metadata
  boost::property_tree::ptree ptMetaData;

  for (Section* pSection : _sections) {
    if (pSection->getSectionKind() == BUILD_METADATA) {
      boost::property_tree::ptree pt;
      pSection->getPayload(pt);
      ptMetaData = pt.get_child("build_metadata", pt);
      break;
    }
  }

  _ostream << std::endl << std::string(78, '=') << std::endl;

  reportBuildVersion(_ostream);
  _ostream << std::string(78, '=') << std::endl;

  if (ptMetaData.empty()) {
    _ostream << "The BUILD_METADATA section is not present. Reports will be limited.\n";
    _ostream << std::string(78, '=') << std::endl;
  }

  reportXclbinInfo(_ostream, _sInputFile, _xclBinHeader, ptMetaData, _sections);
  _ostream << std::string(78, '=') << std::endl;

  reportHardwarePlatform(_ostream, _xclBinHeader, ptMetaData);
  _ostream << std::endl;

  reportClocks(_ostream, _sections);
  _ostream << std::endl;

  reportMemoryConfiguration(_ostream, _sections);
  _ostream << std::string(78, '=') << std::endl;

  if (!ptMetaData.empty()) {
    reportKernels(_ostream, ptMetaData, _sections);
    _ostream << std::string(78, '=') << std::endl;

    reportXOCC(_ostream, ptMetaData);
    _ostream << std::string(78, '=') << std::endl;
  }

  reportKeyValuePairs(_ostream, _sections);
  _ostream << std::string(78, '=') << std::endl;

  if (_bVerbose) {
    reportAllJsonMetadata(_ostream, _sections);
    _ostream << std::string(78, '=') << std::endl;
  }

}


