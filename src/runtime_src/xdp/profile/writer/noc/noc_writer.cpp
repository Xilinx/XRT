/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include <vector>

#include "xdp/profile/writer/noc/noc_writer.h"
#include "xdp/profile/database/database.h"

#include <boost/algorithm/string.hpp>

namespace xdp {

  NOCProfilingWriter::NOCProfilingWriter(const char* fileName,
					     const char* deviceName, uint64_t deviceIndex) :
    VPWriter(fileName),
    mDeviceName(deviceName),
    mDeviceIndex(deviceIndex)
  {
    // TODO: calculate sample period based on requested value 
    //       and granularity of clock frequency
    mSamplePeriod = 565.13;
  }

  NOCProfilingWriter::~NOCProfilingWriter()
  {    
  }

  bool NOCProfilingWriter::write(bool openNewFile)
  {
    // Write header #1
    fout << "Target device: " << mDeviceName << std::endl;
    fout << "Sample period: " << mSamplePeriod << std::endl;
    fout << std::endl;

    // Write header #2
    fout << "name"                << ","
         << "alt_name"            << ","
         << "read_qos"            << ","
         << "read_traffic_class"  << ","
         << "write_qos"           << ","
         << "write_traffic_class" << ","
         << std::endl;

    XclbinInfo* currentXclbin = db->getStaticInfo().getCurrentlyLoadedXclbin(mDeviceIndex);
    auto numNOC = (db->getStaticInfo()).getNumNOC(mDeviceIndex, currentXclbin);
    for (uint64_t n=0; n < numNOC; n++) {
      auto noc = (db->getStaticInfo()).getNOC(mDeviceIndex, currentXclbin, n);

      // TODO: either make these members more generic or add new ones
      auto readTrafficClass = noc->cuIndex;
      auto writeTrafficClass = noc->memIndex;

      // Name = <master>-<NMU cell>-<read QoS>-<write QoS>-<NPI freq>-<AIE freq>
      std::vector<std::string> result; 
      boost::split(result, noc->name, boost::is_any_of("-"));
      std::string masterName = (result.size() > 0) ? result[0] : "";
      std::string cellName   = (result.size() > 1) ? result[1] : "";
      uint64_t readQos       = (result.size() > 2) ? std::stoull(result[2]) : 0;
      uint64_t writeQos      = (result.size() > 3) ? std::stoull(result[3]) : 0;

      fout << cellName          << ","
           << masterName        << ","
           << readQos           << ","
           << readTrafficClass  << ","
           << writeQos          << ","
           << writeTrafficClass << ","
           << std::endl;
    }

    // Write header #3
    fout << "timestamp"           << ","
         << "name"                << ","
         << "read_byte_count"     << ","
         << "read_burst_count"    << ","
         << "read_total_latency"  << ","
         << "read_min_latency"    << ","
         << "read_max_latency"    << ","
         << "write_byte_count"    << ","
         << "write_burst_count"   << ","
         << "write_total_latency" << ","
         << "write_min_latency"   << ","
         << "write_max_latency"   << ","
         << std::endl;

    // Write all data elements
    std::vector<VPDynamicDatabase::CounterSample> samples = 
      (db->getDynamicInfo()).getNOCSamples(mDeviceIndex);
    VPDynamicDatabase::CounterNames names = 
      (db->getDynamicInfo()).getNOCNames(mDeviceIndex);

    for (auto sample : samples) {
      fout << sample.first << ","; // Timestamp
      
      // Report NMU cell name for this sample
      auto iter = names.find(sample.first);
      std::string cellName = (iter == names.end()) ? "N/A" : iter->second;
      fout << cellName << ",";
      
      // Report all samples at this timestamp for this NMU cell
      for (auto value : sample.second) {
        fout << value << ",";
      }
      fout << std::endl;
    }
    return true;
  }

} // end namespace xdp
