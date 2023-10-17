/**
 * Copyright (C) 2018, 2020, 2022 Xilinx, Inc
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

#include "SectionIPLayout.h"

#include "XclBinUtilities.h"
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <iostream>

namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionIPLayout::init SectionIPLayout::initializer;

SectionIPLayout::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(IP_LAYOUT, "IP_LAYOUT", boost::factory<SectionIPLayout*>());
  sectionInfo->nodeName = "ip_layout";

  sectionInfo->supportedAddFormats.push_back(FormatType::json);

  sectionInfo->supportedDumpFormats.push_back(FormatType::json);
  sectionInfo->supportedDumpFormats.push_back(FormatType::html);
  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// ----------------------------------------------------------------------------

const std::string
SectionIPLayout::getIPTypeStr(IP_TYPE _ipType) const
{
  switch (_ipType) {
    case IP_MB:
      return "IP_MB";
    case IP_KERNEL:
      return "IP_KERNEL";
    case IP_DNASC:
      return "IP_DNASC";
    case IP_DDR4_CONTROLLER:
      return "IP_DDR4_CONTROLLER";
    case IP_MEM_DDR4:
      return "IP_MEM_DDR4";
    case IP_MEM_HBM:
      return "IP_MEM_HBM";
    case IP_MEM_HBM_ECC:
      return "IP_MEM_HBM_ECC";
    case IP_PS_KERNEL:
      return "IP_PS_KERNEL";
  }

  return (boost::format("UNKNOWN (%d)") % (unsigned int)_ipType).str();
}

IP_TYPE
SectionIPLayout::getIPType(std::string& _sIPType) const
{
  if (_sIPType == "IP_MB") return IP_MB;
  if (_sIPType == "IP_KERNEL") return IP_KERNEL;
  if (_sIPType == "IP_DNASC") return IP_DNASC;
  if (_sIPType == "IP_DDR4_CONTROLLER") return IP_DDR4_CONTROLLER;
  if (_sIPType == "IP_MEM_DDR4") return IP_MEM_DDR4;
  if (_sIPType == "IP_MEM_HBM") return IP_MEM_HBM;
  if (_sIPType == "IP_MEM_HBM_ECC") return IP_MEM_HBM_ECC;
  if (_sIPType == "IP_PS_KERNEL") return IP_PS_KERNEL;

  std::string errMsg = "ERROR: Unknown IP type: '" + _sIPType + "'";
  throw std::runtime_error(errMsg);
}

const std::string
SectionIPLayout::getIPControlTypeStr(IP_CONTROL _ipControlType) const
{
  switch (_ipControlType) {
    case AP_CTRL_HS:
      return "AP_CTRL_HS";
    case AP_CTRL_CHAIN:
      return "AP_CTRL_CHAIN";
    case AP_CTRL_ME:
      return "AP_CTRL_ME";
    case AP_CTRL_NONE:
      return "AP_CTRL_NONE";
    case ACCEL_ADAPTER:
      return "ACCEL_ADAPTER";
    case FAST_ADAPTER:
      return "FAST_ADAPTER";
  }

  return (boost::format("UNKNOWN (%d)") % (unsigned int)_ipControlType).str();
}


IP_CONTROL
SectionIPLayout::getIPControlType(std::string& _sIPControlType) const
{
  if (_sIPControlType == "AP_CTRL_HS") return AP_CTRL_HS;
  if (_sIPControlType == "AP_CTRL_CHAIN") return AP_CTRL_CHAIN;
  if (_sIPControlType == "AP_CTRL_ME") return AP_CTRL_ME;
  if (_sIPControlType == "AP_CTRL_NONE") return AP_CTRL_NONE;
  if (_sIPControlType == "ACCEL_ADAPTER") return ACCEL_ADAPTER;
  if (_sIPControlType == "FAST_ADAPTER") return FAST_ADAPTER;

  std::string errMsg = "ERROR: Unknown IP Control type: '" + _sIPControlType + "'";
  throw std::runtime_error(errMsg);
}

const std::string
SectionIPLayout::getFunctionalStr(PS_FUNCTIONAL eFunctional) const
{
  switch (eFunctional) {
    case FC_DPU:
      return "DPU";
    case FC_PREPOST:
      return "PrePost";
  }

  return (boost::format("UNKNOWN (%d)") % static_cast<unsigned int>(eFunctional)).str();
}

PS_FUNCTIONAL
SectionIPLayout::getFunctional(const std::string& sFunctional)
{
  if (sFunctional == "DPU") 
    return FC_DPU;
  if (sFunctional == "PrePost") 
    return FC_PREPOST;

  const std::string errMsg = "ERROR: Unknown Functional: '" + sFunctional + "'";
  throw std::runtime_error(errMsg);
}

std::string
SectionIPLayout::getFunctionalEnumStr(const std::string& sFunctional) 
{
  // sFunctional can either have string or numeric value
  // for string value (e.g. "DPU"), convert it to enum string (e.g. "0")
  // for numeric value, getFunctional() would throw exception, and in
  //   this case no conversion needed
  std::string sFunctionalEnum;
  try {
    PS_FUNCTIONAL eFunctional = getFunctional(sFunctional);
    sFunctionalEnum = (boost::format("%d") % static_cast<unsigned int>(eFunctional)).str();
  } catch (const std::runtime_error&) {
    // assume the sFunctional is already the enum string, ignore the exeception
    sFunctionalEnum = sFunctional;
  }
  return sFunctionalEnum;
}


const std::string
SectionIPLayout::getSubTypeStr(PS_SUBTYPE eSubType) const
{
  switch (eSubType) {
    case ST_PS:
      return "PS";
    case ST_DPU:
      return "DPU";
  }

  return (boost::format("UNKNOWN (%d)") % static_cast<unsigned int>(eSubType)).str();
}

PS_SUBTYPE
SectionIPLayout::getSubType(const std::string& sSubType)
{
  if (sSubType == "PS")
    return ST_PS;
  if (sSubType == "DPU")
    return ST_DPU;

  const std::string errMsg = "ERROR: Unknown SubType: '" + sSubType + "'";
  throw std::runtime_error(errMsg);
}

std::string
SectionIPLayout::getSubTypeEnumStr(const std::string& sSubType)
{
  // sSubType can either have string or numeric value
  // for string value (e.g. "DPU"), convert it to enum string (e.g. "1")
  // for numeric value, getSubType() would throw exception, and in this
  //   case no conversion needed
  std::string sSubTypeEnum;
  try {
    PS_SUBTYPE eSubType = getSubType(sSubType);
    sSubTypeEnum = (boost::format("%d") % static_cast<unsigned int>(eSubType)).str();
  } catch (const std::runtime_error&) {
    // assume the sSubType is already the enum string, ignore the exception
    sSubTypeEnum = sSubType;
  }
  return sSubTypeEnum;
}


void
SectionIPLayout::marshalToJSON(char* _pDataSection,
                               unsigned int _sectionSize,
                               boost::property_tree::ptree& _ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: IP_LAYOUT");
  XUtil::TRACE_BUF("Section Buffer", reinterpret_cast<const char*>(_pDataSection), _sectionSize);
  if (_sectionSize == 0) {
    XUtil::TRACE("IP_LAYOUT Section is empty.  Adding an empty entry.");
    boost::property_tree::ptree iplayout;
    iplayout.put("m_count", "0");
    boost::property_tree::ptree ipData;
    iplayout.add_child("m_ip_data", ipData);
    _ptree.add_child("ip_layout", iplayout);
    return;
  }

  // Do we have enough room to overlay the header structure
  if (_sectionSize < sizeof(ip_layout)) {
    auto errMsg = boost::format("ERROR: Section size (%d) is smaller than the size of the ip_layout structure (%d)")
        % _sectionSize % sizeof(ip_layout);
    throw std::runtime_error(errMsg.str());
  }

  ip_layout* pHdr = (ip_layout*)_pDataSection;
  boost::property_tree::ptree ptIPLayout;

  XUtil::TRACE(boost::format("m_count: %d") % pHdr->m_count);

  // Write out the entire structure except for the array structure
  XUtil::TRACE_BUF("ip_layout", reinterpret_cast<const char*>(pHdr), ((uint64_t)&(pHdr->m_ip_data[0]) - (uint64_t)pHdr));
  ptIPLayout.put("m_count", (boost::format("%d") % (unsigned int)pHdr->m_count).str());

  uint64_t expectedSize = ((uint64_t)&(pHdr->m_ip_data[0]) - (uint64_t)pHdr) + (sizeof(ip_data) * pHdr->m_count);

  if (_sectionSize != expectedSize) {
    auto errMsg = boost::format("ERROR: Section size (%d) does not match expected section size (%d).") % _sectionSize % expectedSize;
    throw std::runtime_error(errMsg.str());
  }

  boost::property_tree::ptree ptIPData;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree ptIPEntry;

    if (((IP_TYPE)pHdr->m_ip_data[index].m_type == IP_MEM_DDR4) ||
        ((IP_TYPE)pHdr->m_ip_data[index].m_type == IP_MEM_HBM) ||
        ((IP_TYPE)pHdr->m_ip_data[index].m_type == IP_MEM_HBM_ECC)) {

      XUtil::TRACE(boost::format("[%d]: m_type: %s, m_index: %d, m_pc_index: %d, m_base_address: 0x%lx, m_name: '%s'")
                   % index
                   % getIPTypeStr((IP_TYPE)pHdr->m_ip_data[index].m_type)
                   % pHdr->m_ip_data[index].indices.m_index
                   % pHdr->m_ip_data[index].indices.m_pc_index
                   % pHdr->m_ip_data[index].m_base_address
                   % pHdr->m_ip_data[index].m_name);
    } else if ((IP_TYPE)pHdr->m_ip_data[index].m_type == IP_KERNEL) {
      std::string sIPControlType = getIPControlTypeStr((IP_CONTROL)((pHdr->m_ip_data[index].properties & ((uint32_t)IP_CONTROL_MASK)) >> IP_CONTROL_SHIFT));
      XUtil::TRACE(boost::format("[%d]: m_type: %s, properties: 0x%x {m_ip_control: %s, m_interrupt_id: %d, m_int_enable: %d}, m_base_address: 0x%lx, m_name: '%s'")
                   % index
                   % getIPTypeStr((IP_TYPE)pHdr->m_ip_data[index].m_type)
                   % pHdr->m_ip_data[index].properties
                   % sIPControlType
                   % ((pHdr->m_ip_data[index].properties & ((uint32_t)IP_INTERRUPT_ID_MASK)) >> IP_INTERRUPT_ID_SHIFT)
                   % (pHdr->m_ip_data[index].properties & ((uint32_t)IP_INT_ENABLE_MASK))
                   % pHdr->m_ip_data[index].m_base_address
                   % pHdr->m_ip_data[index].m_name);
    } else {
      // IP_PS_KERNEL
      // if m_subtype is ST_DPU (i.e. fixed ps kernel), display "m_subtype", "m_functional" and "m_kernel_id"
      // else (non-fixed ps kernel), display "properties"
      if ((PS_SUBTYPE)pHdr->m_ip_data[index].ps_kernel.m_subtype == ST_DPU) { 
      XUtil::TRACE(boost::format("[%d]: m_type: %s, m_subtype: %s, m_functional: %s, m_kernel_id: 0x%x, m_base_address: 0x%lx, m_name: '%s'")
                   % index
                   % getIPTypeStr((IP_TYPE)pHdr->m_ip_data[index].m_type)
                   % getSubTypeStr((PS_SUBTYPE)pHdr->m_ip_data[index].ps_kernel.m_subtype)
                   % getFunctionalStr((PS_FUNCTIONAL)pHdr->m_ip_data[index].ps_kernel.m_functional)
                   % (unsigned int)pHdr->m_ip_data[index].ps_kernel.m_kernel_id
                   % pHdr->m_ip_data[index].m_base_address
                   % pHdr->m_ip_data[index].m_name);
      } else {
      XUtil::TRACE(boost::format("[%d]: m_type: %s, properties: 0x%x, m_base_address: 0x%lx, m_name: '%s'")
                   % index
                   % getIPTypeStr((IP_TYPE)pHdr->m_ip_data[index].m_type)
                   % pHdr->m_ip_data[index].properties
                   % pHdr->m_ip_data[index].m_base_address
                   % pHdr->m_ip_data[index].m_name);
      }
    }

    // Write out the entire structure
    XUtil::TRACE_BUF("ip_data", reinterpret_cast<const char*>(&(pHdr->m_ip_data[index])), sizeof(ip_data));

    ptIPEntry.put("m_type", getIPTypeStr((IP_TYPE)pHdr->m_ip_data[index].m_type).c_str());

    switch ((IP_TYPE)pHdr->m_ip_data[index].m_type) {
      case IP_MEM_DDR4:
      case IP_MEM_HBM:
      case IP_MEM_HBM_ECC:
      {
        ptIPEntry.put("m_index", (boost::format("%d") % (unsigned int)pHdr->m_ip_data[index].indices.m_index).str());
        ptIPEntry.put("m_pc_index", (boost::format("%d") % (unsigned int)pHdr->m_ip_data[index].indices.m_pc_index).str());
        break;
      }
      case IP_KERNEL:
      {
        ptIPEntry.put("m_int_enable", (boost::format("%d") % ((pHdr->m_ip_data[index].properties & ((uint32_t)IP_INT_ENABLE_MASK)))).str());
        ptIPEntry.put("m_interrupt_id", (boost::format("%d") % (((pHdr->m_ip_data[index].properties & ((uint32_t)IP_INTERRUPT_ID_MASK)) >> IP_INTERRUPT_ID_SHIFT))).str());
        std::string sIPControlType = getIPControlTypeStr((IP_CONTROL)((pHdr->m_ip_data[index].properties & ((uint32_t)IP_CONTROL_MASK)) >> IP_CONTROL_SHIFT));
        ptIPEntry.put("m_ip_control", sIPControlType.c_str());
        break;
      }
      case IP_PS_KERNEL:
      {
        // if m_subtype is ST_DPU (i.e. fixed ps kernel), display "m_subtype", "m_functional" and "m_kernel_id"
        // else (non-fixed ps kernel), display "properties"
        if ((PS_SUBTYPE)pHdr->m_ip_data[index].ps_kernel.m_subtype == ST_DPU) { 
          ptIPEntry.put("m_subtype", getSubTypeStr((PS_SUBTYPE)pHdr->m_ip_data[index].ps_kernel.m_subtype));
          ptIPEntry.put("m_functional", getFunctionalStr((PS_FUNCTIONAL)pHdr->m_ip_data[index].ps_kernel.m_functional));
          ptIPEntry.put("m_kernel_id", (boost::format("0x%x") % (unsigned int)pHdr->m_ip_data[index].ps_kernel.m_kernel_id).str());
        } else {
          ptIPEntry.put("properties", (boost::format("0x%x") % pHdr->m_ip_data[index].properties).str());
        }
        break;
      }
      default:
          ptIPEntry.put("properties", (boost::format("0x%x") % pHdr->m_ip_data[index].properties).str());
    }    

    if (pHdr->m_ip_data[index].m_base_address != ((uint64_t)-1)) {
      ptIPEntry.put("m_base_address", (boost::format("0x%lx") % pHdr->m_ip_data[index].m_base_address).str());
    } else {
      ptIPEntry.put("m_base_address", "not_used");
    }
    ptIPEntry.put("m_name", (boost::format("%s") % pHdr->m_ip_data[index].m_name).str());

    ptIPData.push_back({ "", ptIPEntry });   // Used to make an array of objects
  }

  ptIPLayout.add_child("m_ip_data", ptIPData);


  _ptree.add_child("ip_layout", ptIPLayout);
  XUtil::TRACE("-----------------------------");
}

void
SectionIPLayout::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                 std::ostringstream& _buf) const
{
  const boost::property_tree::ptree& ptIPLayout = _ptSection.get_child("ip_layout");

  // Initialize the memory to zero's
  ip_layout ipLayoutHdr = ip_layout{};

  // Read, store, and report mem_topology data
  ipLayoutHdr.m_count = ptIPLayout.get<uint32_t>("m_count");

  if (ipLayoutHdr.m_count == 0) {
    std::cout << "WARNING: Skipping IP_LAYOUT section for count size is zero." << std::endl;
    return;
  }

  XUtil::TRACE("IP_LAYOUT");
  XUtil::TRACE(boost::format("m_count: %d") % ipLayoutHdr.m_count);

  // Write out the entire structure except for the mem_data structure
  XUtil::TRACE_BUF("ip_layout - minus ip_data", reinterpret_cast<const char*>(&ipLayoutHdr), (sizeof(ip_layout) - sizeof(ip_data)));
  _buf.write(reinterpret_cast<const char*>(&ipLayoutHdr), sizeof(ip_layout) - sizeof(ip_data));


  // Read, store, and report connection segments
  unsigned int count = 0;
  boost::property_tree::ptree ipDatas = ptIPLayout.get_child("m_ip_data");
  for (const auto& kv : ipDatas) {
    ip_data ipDataHdr = ip_data{};
    boost::property_tree::ptree ptIPData = kv.second;

    auto sm_type = ptIPData.get<std::string>("m_type");
    ipDataHdr.m_type = getIPType(sm_type);

    // For these IPs, the struct indices needs to be initialized
    switch (ipDataHdr.m_type) {
      case IP_MEM_DDR4:
      case IP_MEM_HBM:
      case IP_MEM_HBM_ECC:
      {
        ipDataHdr.indices.m_index = ptIPData.get<uint16_t>("m_index");
        ipDataHdr.indices.m_pc_index = ptIPData.get<uint8_t>("m_pc_index", 0);
        break;
      }
      case IP_PS_KERNEL:
      {
        // m_subtype
        auto sSubType = ptIPData.get<std::string>("m_subtype", "");
        if (!sSubType.empty()) {
          // subtype and functinoal can either be string (e.g. "DPU") or numeric (e.g. "1")
          if (isdigit(sSubType[0])) {
            // convert sSubType from numeric to enum
            ipDataHdr.ps_kernel.m_subtype = std::stoul(sSubType);
          } else {
            // convert sSubType from string to enum
            ipDataHdr.ps_kernel.m_subtype = static_cast<unsigned int>(getSubType(sSubType));
          }
        }
  
        // m_functinoal
        auto sFunctional = ptIPData.get<std::string>("m_functional", "");
        if (!sFunctional.empty()) {
          if (isdigit(sFunctional[0])) {
            // convert sFunctional from numeric to enum
            ipDataHdr.ps_kernel.m_functional = std::stoul(sFunctional);
          } else {
            // convert sFunctional from string to enum
            ipDataHdr.ps_kernel.m_functional = static_cast<unsigned int>(getFunctional(sFunctional));
          }
        }
  
        // m_kernel_id
        auto sKernelId = ptIPData.get<std::string>("m_kernel_id", "");
        if (!sKernelId.empty()) {
          ipDataHdr.ps_kernel.m_kernel_id = XUtil::stringToUInt64(sKernelId);
        }
        break;
      }
      default:
      {
        // Get the properties value (if one is defined)
        auto sProperties = ptIPData.get<std::string>("properties", "0");
        // ipDataHdr.properties = (uint32_t)XUtil::stringToUInt64(sProperties);
        ipDataHdr.properties = static_cast<uint32_t>(XUtil::stringToUInt64(sProperties));
  
        // IP_KERNEL
        { // m_int_enable
          boost::optional<bool> bIntEnable;
          bIntEnable = ptIPData.get_optional<bool>("m_int_enable");
          if (bIntEnable.is_initialized()) {
            ipDataHdr.properties = ipDataHdr.properties & (~(uint32_t)IP_INT_ENABLE_MASK);  // Clear existing bit
            if (bIntEnable.get()) 
              ipDataHdr.properties = ipDataHdr.properties | ((uint32_t)IP_INT_ENABLE_MASK); // Set bit
          }
        }
  
        { // m_interrupt_id
          auto sInterruptID = ptIPData.get<std::string>("m_interrupt_id", "");
          if (!sInterruptID.empty()) {
            unsigned int interruptID = std::stoul(sInterruptID);
            unsigned int maxValue = ((unsigned int)IP_INTERRUPT_ID_MASK) >> IP_INTERRUPT_ID_SHIFT;
            if (interruptID > maxValue) {
              const auto errMsg = boost::format("ERROR: The m_interrupt_id (%d), exceeds maximum value (%d).") % interruptID % maxValue;
              throw std::runtime_error(errMsg.str());
            }
  
            unsigned int shiftValue = (interruptID << IP_INTERRUPT_ID_SHIFT);
            shiftValue = shiftValue & ((uint32_t)IP_INTERRUPT_ID_MASK);
            ipDataHdr.properties = ipDataHdr.properties & (~(uint32_t)IP_INTERRUPT_ID_MASK);  // Clear existing bits
            ipDataHdr.properties = ipDataHdr.properties | shiftValue;                          // Set bits
          }
        }
  
        { // m_ip_control
          boost::optional<std::string> bIPControl;
          bIPControl = ptIPData.get_optional<std::string>("m_ip_control");
          if (bIPControl.is_initialized()) {
            unsigned int ipControl = (unsigned int)getIPControlType(bIPControl.get());
  
            unsigned int maxValue = ((unsigned int)IP_CONTROL_MASK) >> IP_CONTROL_SHIFT;
            if (ipControl > maxValue) {
              const auto errMsg = boost::format("ERROR: The m_ip_control (%d), exceeds maximum value (%d).") % (unsigned int)ipControl % maxValue;
              throw std::runtime_error(errMsg.str());
            }
  
            unsigned int shiftValue = ipControl << IP_CONTROL_SHIFT;
            shiftValue = shiftValue & ((uint32_t)IP_CONTROL_MASK);
            ipDataHdr.properties = ipDataHdr.properties & (~(uint32_t)IP_CONTROL_MASK);  // Clear existing bits
            ipDataHdr.properties = ipDataHdr.properties | shiftValue;                          // Set bits
          }
        }
      }
    }

    auto sBaseAddress = ptIPData.get<std::string>("m_base_address");

    if (sBaseAddress != "not_used") {
      ipDataHdr.m_base_address = XUtil::stringToUInt64(sBaseAddress);
    } else {
      ipDataHdr.m_base_address = (uint64_t)-1;
    }

    auto sm_name = ptIPData.get<std::string>("m_name");
    if (sm_name.length() >= sizeof(ip_data::m_name)) {
      const auto errMsg = boost::format("ERROR: The m_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'")
          % (unsigned int)sm_name.length() % (unsigned int)sizeof(ip_data::m_name) % sm_name;
      throw std::runtime_error(errMsg.str());
    }

    // We already know that there is enough room for this string
    memcpy(ipDataHdr.m_name, sm_name.c_str(), sm_name.length() + 1);

    if ((ipDataHdr.m_type == IP_MEM_DDR4) ||
        (ipDataHdr.m_type == IP_MEM_HBM) ||
        (ipDataHdr.m_type == IP_MEM_HBM_ECC)) {
      XUtil::TRACE(boost::format("[%d]: m_type: %d, m_index: %d, m_pc_index: %d, m_base_address: 0x%lx, m_name: '%s'")
                   % count
                   % (unsigned int)ipDataHdr.m_type
                   % (unsigned int)ipDataHdr.indices.m_index
                   % (unsigned int)ipDataHdr.indices.m_pc_index
                   % ipDataHdr.m_base_address
                   % ipDataHdr.m_name);
    } else if (ipDataHdr.m_type == IP_KERNEL) {
      XUtil::TRACE(boost::format("[%d]: m_type: %d, properties: 0x%x, m_base_address: 0x%lx, m_name: '%s'")
                   % count
                   % (unsigned int)ipDataHdr.m_type
                   % (unsigned int)ipDataHdr.properties
                   % ipDataHdr.m_base_address
                   % ipDataHdr.m_name);
    } else {
      // IP_PS_KERNEL
      // if m_subtype is ST_DPU (i.e. fixed ps kernel), display "m_subtype", "m_functional" and "m_kernel_id"
      // else (non-fixed ps kernel), display "properties"
      if ((PS_SUBTYPE)ipDataHdr.ps_kernel.m_subtype == ST_DPU) { 
        XUtil::TRACE(boost::format("[%d]: m_type: %d, m_subtype: %d, m_functional: %d, m_kernel_id: 0x%x, m_base_address: 0x%lx, m_name: '%s'")
                   % count
                   % (unsigned int)ipDataHdr.m_type
                   % (unsigned int)ipDataHdr.ps_kernel.m_subtype
                   % (unsigned int)ipDataHdr.ps_kernel.m_functional
                   % (unsigned int)ipDataHdr.ps_kernel.m_kernel_id
                   % ipDataHdr.m_base_address
                   % ipDataHdr.m_name);
       } else {
         XUtil::TRACE(boost::format("[%d]: m_type: %d, properties: 0x%x, m_base_address: 0x%lx, m_name: '%s'")
                   % count
                   % (unsigned int)ipDataHdr.m_type
                   % (unsigned int)ipDataHdr.properties
                   % ipDataHdr.m_base_address
                   % ipDataHdr.m_name);
       }
    }

    // Write out the entire structure
    XUtil::TRACE_BUF("ip_data", reinterpret_cast<const char*>(&ipDataHdr), sizeof(ip_data));
    _buf.write(reinterpret_cast<const char*>(&ipDataHdr), sizeof(ip_data));
    count++;
  }

  // -- The counts should match --
  if (count != (unsigned int)ipLayoutHdr.m_count) {
    const auto errMsg = boost::format("ERROR: Number of connection sections (%d) does not match expected encoded value: %d")
        % (unsigned int)count % (unsigned int)ipLayoutHdr.m_count;
    throw std::runtime_error(errMsg.str());
  }

  // -- Buffer needs to be less than 64K--
  unsigned int bufferSize = (unsigned int)_buf.str().size();
  const unsigned int maxBufferSize = 64 * 1024;
  if (bufferSize > maxBufferSize) {
    const auto errMsg = boost::format("CRITICAL WARNING: The buffer size for the IP_LAYOUT section (%d) exceed the maximum size of %d.\nThis can result in lose of data in the driver.")
        % (unsigned int)bufferSize % (unsigned int)maxBufferSize;
    std::cout << errMsg << std::endl;
    // throw std::runtime_error(errMsg);
  }
}


template<typename T>
std::vector<T> as_vector(boost::property_tree::ptree const& pt,
                         boost::property_tree::ptree::key_type const& key)
{
  std::vector<T> r;
  for (auto& item : pt.get_child(key))
    r.push_back(item.second);
  return r;
}


void
SectionIPLayout::appendToSectionMetadata(const boost::property_tree::ptree& _ptAppendData,
                                         boost::property_tree::ptree& _ptToAppendTo)
{
  XUtil::TRACE_PrintTree("To Append To", _ptToAppendTo);
  XUtil::TRACE_PrintTree("Append data", _ptAppendData);


  auto ip_datas = as_vector<boost::property_tree::ptree>(_ptAppendData, "m_ip_data");
  unsigned int appendCount = _ptAppendData.get<unsigned int>("m_count");

  if (appendCount != ip_datas.size()) {
    auto errMsg = boost::format("ERROR: IP layout section append's count (%d) does not match the number of ip_data entries (%d).") % appendCount % ip_datas.size();
    throw std::runtime_error(errMsg.str());
  }

  if (appendCount == 0) {
    std::string errMsg = "WARNING: IP layout section doesn't contain any data to append.";
    std::cout << errMsg << std::endl;
    return;
  }

  // Now copy the data
  boost::property_tree::ptree& ptIPLayoutAppendTo = _ptToAppendTo.get_child("ip_layout");
  boost::property_tree::ptree& ptDest_m_ip_data = ptIPLayoutAppendTo.get_child("m_ip_data");

  for (auto ip_data : ip_datas) {
    boost::property_tree::ptree new_ip_data;
    auto sm_type = ip_data.get<std::string>("m_type");
    new_ip_data.put("m_type", sm_type);

    if ((getIPType(sm_type) == IP_MEM_DDR4) ||
        (getIPType(sm_type) == IP_MEM_HBM) ||
        (getIPType(sm_type) == IP_MEM_HBM_ECC)) {
      new_ip_data.put("m_index", ip_data.get<std::string>("m_index"));
      new_ip_data.put("m_pc_index", ip_data.get<std::string>("m_pc_index", "0"));
    } else {
      new_ip_data.put("properties", ip_data.get<std::string>("properties"));
    }
    new_ip_data.put("m_base_address", ip_data.get<std::string>("m_base_address"));
    new_ip_data.put("m_name", ip_data.get<std::string>("m_name"));

    ptDest_m_ip_data.push_back({ "", new_ip_data });   // Used to make an array of objects
  }

  // Update count
  {
    unsigned int count = ptIPLayoutAppendTo.get<unsigned int>("m_count");
    count += appendCount;
    ptIPLayoutAppendTo.put("m_count", count);
  }

  XUtil::TRACE_PrintTree("To Append To Done", _ptToAppendTo);
}

