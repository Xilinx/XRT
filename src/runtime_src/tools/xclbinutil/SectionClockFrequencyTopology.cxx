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

#include "SectionClockFrequencyTopology.h"

#include "XclBinUtilities.h"
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <iostream>
#include <stdint.h>

namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionClockFrequencyTopology::init SectionClockFrequencyTopology::initializer;

SectionClockFrequencyTopology::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(CLOCK_FREQ_TOPOLOGY, "CLOCK_FREQ_TOPOLOGY", boost::factory<SectionClockFrequencyTopology*>());
  sectionInfo->nodeName = "clock_freq_topology";

  sectionInfo->supportedAddFormats.push_back(FormatType::json);

  sectionInfo->supportedDumpFormats.push_back(FormatType::json);
  sectionInfo->supportedDumpFormats.push_back(FormatType::html);
  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// ----------------------------------------------------------------------------

const std::string
SectionClockFrequencyTopology::getClockTypeStr(CLOCK_TYPE _clockType) const
{
  switch (_clockType) {
    case CT_UNUSED:
      return "UNUSED";
    case CT_DATA:
      return "DATA";
    case CT_KERNEL:
      return "KERNEL";
    case CT_SYSTEM:
      return "SYSTEM";
  }

  return (boost::format("UNKNOWN (%d) CLOCK_TYPE") % (unsigned int)_clockType).str();
}

CLOCK_TYPE
SectionClockFrequencyTopology::getClockType(std::string& _sClockType) const
{
  if (_sClockType == "UNUSED")
    return CT_UNUSED;

  if (_sClockType == "DATA")
    return CT_DATA;

  if (_sClockType == "KERNEL")
    return CT_KERNEL;

  if (_sClockType == "SYSTEM")
    return CT_SYSTEM;

  std::string errMsg = "ERROR: Unknown Clock Type: '" + _sClockType + "'";
  throw std::runtime_error(errMsg);
}


void
SectionClockFrequencyTopology::marshalToJSON(char* _pDataSection,
                                             unsigned int _sectionSize,
                                             boost::property_tree::ptree& _ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Marshalling to JSON: ClockFreqTopology");
  XUtil::TRACE_BUF("Section Buffer", reinterpret_cast<const char*>(_pDataSection), _sectionSize);

  // Do we have enough room to overlay the header structure
  if (_sectionSize < sizeof(clock_freq_topology)) {
    auto errMsg = boost::format("ERROR: Section size (%d) is smaller than the size of the clock_freq_topology structure (%d)")
        % _sectionSize % sizeof(clock_freq_topology);
    throw std::runtime_error(errMsg.str());
  }

  clock_freq_topology* pHdr = (clock_freq_topology*)_pDataSection;
  boost::property_tree::ptree clock_freq_topology;

  XUtil::TRACE(boost::format("m_count: %d") % (uint32_t)pHdr->m_count);

  // Write out the entire structure except for the array structure
  XUtil::TRACE_BUF("clock_freq", reinterpret_cast<const char*>(pHdr), ((uint64_t)&(pHdr->m_clock_freq[0]) - (uint64_t)pHdr));
  clock_freq_topology.put("m_count", (boost::format("%d") % (unsigned int)pHdr->m_count).str());

  clock_freq mydata = clock_freq{};

  XUtil::TRACE(boost::format("Size of clock_freq: %d\nSize of mydata: %d")
               % sizeof(clock_freq)
               % sizeof(mydata));
  uint64_t expectedSize = ((uint64_t)&(pHdr->m_clock_freq[0]) - (uint64_t)pHdr) + (sizeof(clock_freq) * (uint64_t)pHdr->m_count);

  if (_sectionSize != expectedSize) {
    auto errMsg = boost::format("ERROR: Section size (%d) does not match expected sections size (%d).") % _sectionSize % expectedSize;
    throw std::runtime_error(errMsg.str());
  }

  boost::property_tree::ptree m_clock_freq;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree clock_freq;

    XUtil::TRACE(boost::format("[%d]: m_freq_Mhz: %d, m_type: %d, m_name: '%s'")
                 % index
                 % (unsigned int)pHdr->m_clock_freq[index].m_freq_Mhz
                 % getClockTypeStr((CLOCK_TYPE)pHdr->m_clock_freq[index].m_type)
                 % pHdr->m_clock_freq[index].m_name);

    // Write out the entire structure
    XUtil::TRACE_BUF("clock_freq", reinterpret_cast<const char*>(&pHdr->m_clock_freq[index]), sizeof(clock_freq));

    clock_freq.put("m_freq_Mhz", (boost::format("%d") % (unsigned int)pHdr->m_clock_freq[index].m_freq_Mhz).str());
    clock_freq.put("m_type", getClockTypeStr((CLOCK_TYPE)pHdr->m_clock_freq[index].m_type).c_str());
    clock_freq.put("m_name", (boost::format("%s") % pHdr->m_clock_freq[index].m_name).str());

    m_clock_freq.push_back({ "", clock_freq });   // Used to make an array of objects
  }

  clock_freq_topology.add_child("m_clock_freq", m_clock_freq);

  _ptree.add_child("clock_freq_topology", clock_freq_topology);
  XUtil::TRACE("-----------------------------");
}



void
SectionClockFrequencyTopology::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                               std::ostringstream& _buf) const
{
  const boost::property_tree::ptree& ptClockFreqTopo = _ptSection.get_child("clock_freq_topology");

  // Initialize the memory to zero's
  clock_freq_topology clockFreqTopologyHdr = clock_freq_topology{};

  // Read, store, and report clock frequency topology data
  clockFreqTopologyHdr.m_count = ptClockFreqTopo.get<uint16_t>("m_count");

  XUtil::TRACE("CLOCK_FREQ_TOPOLOGY");
  XUtil::TRACE(boost::format("m_count: %d") % clockFreqTopologyHdr.m_count);

  if (clockFreqTopologyHdr.m_count == 0) {
    std::cout << "WARNING: Skipping CLOCK_FREQ_TOPOLOGY section for count size is zero." << std::endl;
    return;
  }

  // Write out the entire structure except for the mem_data structure
  XUtil::TRACE_BUF("clock_freq_topology- minus clock_freq", reinterpret_cast<const char*>(&clockFreqTopologyHdr), (sizeof(clock_freq_topology) - sizeof(clock_freq)));
  _buf.write(reinterpret_cast<const char*>(&clockFreqTopologyHdr), sizeof(clock_freq_topology) - sizeof(clock_freq));

  // Read, store, and report connection sections
  unsigned int count = 0;
  const boost::property_tree::ptree clockFreqs = ptClockFreqTopo.get_child("m_clock_freq");
  for (const auto& kv : clockFreqs) {
    clock_freq clockFreqHdr = clock_freq{};
    boost::property_tree::ptree ptClockFreq = kv.second;

    clockFreqHdr.m_freq_Mhz = ptClockFreq.get<uint16_t>("m_freq_Mhz");
    auto sm_type = ptClockFreq.get<std::string>("m_type");
    clockFreqHdr.m_type = (uint8_t)getClockType(sm_type);

    auto sm_name = ptClockFreq.get<std::string>("m_name");
    if (sm_name.length() >= sizeof(clock_freq::m_name)) {
      auto errMsg = boost::format("ERROR: The m_name entry length (%d), exceeds the allocated space (%d). Name: '%s'")
          % (int)sm_name.length() % (unsigned int)sizeof(clock_freq::m_name) % sm_name;
      throw std::runtime_error(errMsg.str());
    }

    // We already know that there is enough room for this string
    memcpy(clockFreqHdr.m_name, sm_name.c_str(), sm_name.length() + 1);

    XUtil::TRACE(boost::format("[%d]: m_freq_Mhz: %d, m_type: %d, m_name: '%s'")
                 % count
                 % (unsigned int)clockFreqHdr.m_freq_Mhz
                 % (unsigned int)clockFreqHdr.m_type
                 % clockFreqHdr.m_name);

    // Write out the entire structure
    XUtil::TRACE_BUF("clock_freq", reinterpret_cast<const char*>(&clockFreqHdr), sizeof(clock_freq));
    _buf.write(reinterpret_cast<const char*>(&clockFreqHdr), sizeof(clock_freq));
    count++;
  }

  // -- The counts should match --
  if (count != (unsigned int)clockFreqTopologyHdr.m_count) {
    auto errMsg = boost::format("ERROR: Number of connection sections (%d) does not match expected encoded value: %d")
        % (unsigned int)count % (unsigned int)clockFreqTopologyHdr.m_count;
    throw std::runtime_error(errMsg.str());
  }
}

