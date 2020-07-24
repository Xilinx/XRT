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

#include "xdp/profile/writer/aie/aie_writer.h"
#include "xdp/profile/database/database.h"

namespace xdp {

  AIEProfilingWriter::AIEProfilingWriter(const char* fileName,
					     const char* deviceName, uint64_t deviceIndex) :
    VPWriter(fileName),
    mDeviceName(deviceName),
    mDeviceIndex(deviceIndex)
  {
  }

  AIEProfilingWriter::~AIEProfilingWriter()
  {    
  }

  void AIEProfilingWriter::write(bool openNewFile)
  {
    // TODO: get AIE clock freq from metadata
    float aieClockFreqMhz = 1000.0;

    // Write header
    fout << "Target device: " << mDeviceName << std::endl;
    fout << "Clock frequency (MHz): " << aieClockFreqMhz << std::endl;
    fout << "timestamp"    << ","
         << "column"       << ","
         << "row"          << ",";
         
    for (uint32_t c = 0; c < NUM_AIE_COUNTERS; ++c) {
      fout << "start" << c << ","
           << "end"   << c << ","
           << "reset" << c << ","
           << "value" << c << ",";
    }
    
	  fout << std::endl;

    // Write all data elements
    std::vector<VPDynamicDatabase::CounterSample> samples = 
      (db->getDynamicInfo()).getAIESamples(mDeviceIndex);

    for (auto sample : samples) {
      fout << sample.first << ","; // Timestamp
      for (auto value : sample.second) {
        fout << value << ",";
      }
      fout << std::endl;
    }
  }

} // end namespace xdp
