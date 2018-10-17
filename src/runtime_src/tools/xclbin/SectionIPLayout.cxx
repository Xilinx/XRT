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

#include "SectionIPLayout.h"

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

#include <iostream>

// Static Variables / Classes
SectionIPLayout::_init SectionIPLayout::_initializer;

SectionIPLayout::SectionIPLayout() {
  // Empty
}

SectionIPLayout::~SectionIPLayout() {
  // Empty
}


const std::string
SectionIPLayout::getIPTypeStr(enum IP_TYPE _ipType) const {
  switch (_ipType) {
    case IP_MB:
      return "IP_MB";
    case IP_KERNEL:
      return "IP_KERNEL";
    case IP_DNASC:
      return "IP_DNASC";
  }

  return XUtil::format("UNKNOWN (%d)", (unsigned int)_ipType);
}

enum IP_TYPE
SectionIPLayout::getIPType(std::string& _sIPType) const {
  if (_sIPType == "IP_MB") return IP_MB;
  if (_sIPType == "IP_KERNEL") return IP_KERNEL;
  if (_sIPType == "IP_DNASC") return IP_DNASC;

  std::string errMsg = "ERROR: Unknown IP type: '" + _sIPType + "'";
  throw std::runtime_error(errMsg);
}


void
SectionIPLayout::marshalToJSON(char* _pDataSection,
                               unsigned int _sectionSize,
                               boost::property_tree::ptree& _ptree) const {
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: IP_LAYOUT");
  XUtil::TRACE_BUF("Section Buffer", reinterpret_cast<const char*>(_pDataSection), _sectionSize);

  // Do we have enough room to overlay the header structure
  if (_sectionSize < sizeof(ip_layout)) {
    throw std::runtime_error(XUtil::format("ERROR: Section size (%d) is smaller than the size of the ip_layout structure (%d)",
                                           _sectionSize, sizeof(ip_layout)));
  }

  ip_layout* pHdr = (ip_layout*)_pDataSection;
  boost::property_tree::ptree ip_layout;

  XUtil::TRACE(XUtil::format("m_count: %d", pHdr->m_count));

  // Write out the entire structure except for the array structure
  XUtil::TRACE_BUF("ip_layout", reinterpret_cast<const char*>(pHdr), (unsigned long)&(pHdr->m_ip_data[0]) - (unsigned long)pHdr);
  ip_layout.put("m_count", XUtil::format("%d", (unsigned int)pHdr->m_count).c_str());

  unsigned int expectedSize = ((unsigned long)&(pHdr->m_ip_data[0]) - (unsigned long)pHdr) + (sizeof(ip_data) * pHdr->m_count);

  if (_sectionSize != expectedSize) {
    throw std::runtime_error(XUtil::format("ERROR: Section size (%d) does not match expected section size (%d).",
                                           _sectionSize, expectedSize));
  }

  boost::property_tree::ptree m_ip_data;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree ip_data;

    XUtil::TRACE(XUtil::format("[%d]: m_type: %s, properties: 0x%x, m_base_address: 0x%lx, m_name: '%s'",
                               index,
                               getIPTypeStr((enum IP_TYPE)pHdr->m_ip_data[index].m_type).c_str(),
                               pHdr->m_ip_data[index].properties,
                               pHdr->m_ip_data[index].m_base_address,
                               pHdr->m_ip_data[index].m_name));

    // Write out the entire structure
    XUtil::TRACE_BUF("ip_data", reinterpret_cast<const char*>(&(pHdr->m_ip_data[index])), sizeof(ip_data));

    ip_data.put("m_type", getIPTypeStr((enum IP_TYPE)pHdr->m_ip_data[index].m_type).c_str());
    ip_data.put("properties", XUtil::format("0x%x", pHdr->m_ip_data[index].properties).c_str());
    if ( pHdr->m_ip_data[index].m_base_address != ((uint64_t) -1) ) {
      ip_data.put("m_base_address", XUtil::format("0x%lx", pHdr->m_ip_data[index].m_base_address).c_str());
    } else {
      ip_data.put("m_base_address", "not_used");
    }
    ip_data.put("m_name", XUtil::format("%s", pHdr->m_ip_data[index].m_name).c_str());

    m_ip_data.add_child("ip_data", ip_data);
  }

  ip_layout.add_child("m_ip_data", m_ip_data);

  _ptree.add_child("ip_layout", ip_layout);
  XUtil::TRACE("-----------------------------");
}

void
SectionIPLayout::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                 std::ostringstream& _buf) const {
  const boost::property_tree::ptree& ptIPLayout = _ptSection.get_child("ip_layout");

  // Initialize the memory to zero's
  ip_layout ipLayoutHdr = (ip_layout){ 0 };

  // Read, store, and report mem_topology data
  ipLayoutHdr.m_count = ptIPLayout.get<uint32_t>("m_count");

  if (ipLayoutHdr.m_count == 0) {
    std::cout << "WARNING: Skipping IP_LAYOUT section for count size is zero." << std::endl;
    return;
  }

  XUtil::TRACE("IP_LAYOUT");
  XUtil::TRACE(XUtil::format("m_count: %d", ipLayoutHdr.m_count));

  // Write out the entire structure except for the mem_data structure
  XUtil::TRACE_BUF("ip_layout - minus ip_data", reinterpret_cast<const char*>(&ipLayoutHdr), (sizeof(ip_layout) - sizeof(ip_data)));
  _buf.write(reinterpret_cast<const char*>(&ipLayoutHdr), sizeof(ip_layout) - sizeof(ip_data));


  // Read, store, and report connection segments
  unsigned int count = 0;
  boost::property_tree::ptree ipDatas = ptIPLayout.get_child("m_ip_data");
  for (const auto& kv : ipDatas) {
    ip_data ipDataHdr = (ip_data){ 0 };
    boost::property_tree::ptree ptIPData = kv.second;

    std::string sm_type = ptIPData.get<std::string>("m_type");
    ipDataHdr.m_type = getIPType(sm_type);

    std::string sProperties = ptIPData.get<std::string>("properties");
    ipDataHdr.properties = (uint32_t)XUtil::stringToUInt64(sProperties);

    std::string sBaseAddress = ptIPData.get<std::string>("m_base_address");
    ipDataHdr.m_base_address = XUtil::stringToUInt64(sBaseAddress);

    if ( sBaseAddress != "not_used" ) {
      ipDataHdr.m_base_address = XUtil::stringToUInt64(sBaseAddress);
    }
    else {
      ipDataHdr.m_base_address = (uint64_t) -1;
    }

    std::string sm_name = ptIPData.get<std::string>("m_name");
    if (sm_name.length() >= sizeof(ip_data::m_name)) {
      std::string errMsg = XUtil::format("ERROR: The m_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'",
                                         (unsigned int)sm_name.length(), (unsigned int)sizeof(ip_data::m_name), sm_name);
      throw std::runtime_error(errMsg);
    }

    // We already know that there is enough room for this string
    memcpy(ipDataHdr.m_name, sm_name.c_str(), sm_name.length() + 1);

    XUtil::TRACE(XUtil::format("[%d]: m_type: %d, properties: 0x%x, m_base_address: 0x%lx, m_name: '%s'",
                               count,
                               (unsigned int)ipDataHdr.m_type,
                               (unsigned int)ipDataHdr.properties,
                               ipDataHdr.m_base_address,
                               ipDataHdr.m_name));

    // Write out the entire structure
    XUtil::TRACE_BUF("ip_data", reinterpret_cast<const char*>(&ipDataHdr), sizeof(ip_data));
    _buf.write(reinterpret_cast<const char*>(&ipDataHdr), sizeof(ip_data));
    count++;
  }

  // -- The counts should match --
  if (count != (unsigned int)ipLayoutHdr.m_count) {
    std::string errMsg = XUtil::format("ERROR: Number of connection sections (%d) does not match expected encoded value: %d",
                                       (unsigned int)count, (unsigned int)ipLayoutHdr.m_count);
    throw std::runtime_error(errMsg);
  }

  // -- Buffer needs to be less than 64K--
  unsigned int bufferSize = _buf.str().size();
  const unsigned int maxBufferSize = 64 * 1024;
  if ( bufferSize > maxBufferSize ) {
    std::string errMsg = XUtil::format("CRITICAL WARNING: The buffer size for the IP_LAYOUT section (%d) exceed the maximum size of %d.\nThis can result in lose of data in the driver.",
                                       (unsigned int) bufferSize, (unsigned int) maxBufferSize);
    std::cout << errMsg << std::endl;
    // throw std::runtime_error(errMsg);
  }
}


bool 
SectionIPLayout::doesSupportAddFormatType(FormatType _eFormatType) const
{
  if (_eFormatType == FT_JSON) {
    return true;
  }
  return false;
}

bool 
SectionIPLayout::doesSupportDumpFormatType(FormatType _eFormatType) const
{
    if ((_eFormatType == FT_JSON) ||
        (_eFormatType == FT_HTML) ||
        (_eFormatType == FT_RAW))
    {
      return true;
    }

    return false;
}

