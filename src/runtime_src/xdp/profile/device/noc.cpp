/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include "noc.h"
#include "tracedefs.h"
#include <vector>
#include <iomanip>

#include <boost/algorithm/string.hpp>

namespace xdp {

NOC::NOC(Device* handle, uint64_t index, debug_ip_data* data)
    : ProfileIP(handle, index, data),
      mMajorVersion(0),
      mMinorVersion(0),
      mReadTrafficClass(0),
      mWriteTrafficClass(0),
      mReadQos(0),
      mWriteQos(0),
      mNpiClockFreqMhz(299.997009)
{
  if (data) {
    mMajorVersion = data->m_major;
    mMinorVersion = data->m_minor;

    parseProperties(data->m_properties);
    parseName(data->m_name);
  }
}

void NOC::parseProperties(uint8_t properties)
{  // Properties = (read class << 2) + (write class)
  mReadTrafficClass  = properties >> 2;
  mWriteTrafficClass = properties & 0x3;
}

void NOC::parseName(std::string name)
{
  // Name = <master>-<NMU cell>-<read QoS>-<write QoS>-<NPI freq>
  std::vector<std::string> result; 
  boost::split(result, name, boost::is_any_of("-"));
  
  try {mMasterName = result.at(0);} catch (...) {mMasterName = "";}
  try {mCellName = result.at(1);} catch (...) {mCellName = "";}
  try {mReadQos = std::stoull(result.at(2));} catch (...) {mReadQos = 0;}
  try {mWriteQos = std::stoull(result.at(3));} catch (...) {mWriteQos = 0;}
  try {mNpiClockFreqMhz = std::stod(result.at(4));} catch (...) {mNpiClockFreqMhz = 299.997009;}
}

inline void NOC::write32(uint64_t offset, uint32_t val)
{
  write(offset, 4, &val);
}

void NOC::init()
{
  // TBD
}

void NOC::showProperties()
{
  std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
  (*outputStream) << " Noc " << std::endl;
  ProfileIP::showProperties();
}

void NOC::showStatus()
{
  // TBD
}

}   // namespace xdp
