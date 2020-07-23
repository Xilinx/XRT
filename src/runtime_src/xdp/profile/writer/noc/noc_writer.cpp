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

  void NOCProfilingWriter::write(bool openNewFile)
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

    // TODO: write QoS and traffic class metadata from debug_ip_layout
    fout << "noc_nmu512_x0y0,/mm2s/m_axi_gmem,128,0,64,0," << std::endl;
    fout << "noc_nmu512_x0y1,/s2mm/m_axi_gmem,64,0,64,0," << std::endl;
    fout << std::endl;

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

    for (auto sample : samples) {
      fout << sample.first << ","; // Timestamp
      for (auto value : sample.second) {
        fout << value << ",";
      }
      fout << std::endl;
    }
  }

} // end namespace xdp
