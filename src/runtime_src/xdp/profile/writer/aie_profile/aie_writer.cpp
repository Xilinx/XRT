/**
 * Copyright (C) 2020-2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/writer/aie_profile/aie_writer.h"

namespace xdp {

  AIEProfilingWriter::AIEProfilingWriter(const char* fileName,
                                         const char* deviceName, uint64_t deviceIndex) :
    VPWriter(fileName),
    mDeviceName(deviceName),
    mDeviceIndex(deviceIndex), mHeaderWritten(false)
  {
  }

  AIEProfilingWriter::~AIEProfilingWriter()
  {    
  }


  void AIEProfilingWriter::writeHeader()
  {
    // Report HW generation to inform analysis how to interpret event IDs
    auto aieGeneration = (db->getStaticInfo()).getAIEGeneration(mDeviceIndex);

    // Grab AIE clock freq from first counter in metadata
    // NOTE: Assumed the same for all tiles
    auto aie = (db->getStaticInfo()).getAIECounter(mDeviceIndex, 0);
    double aieClockFreqMhz = (aie != nullptr) ?  aie->clockFreqMhz : 1200.0;

    // Write header
    fout << "Target device: " << mDeviceName << "\n";
    fout << "Hardware generation: " << static_cast<int>(aieGeneration) << "\n";
    fout << "Clock frequency (MHz): " << aieClockFreqMhz << "\n";
    fout << "timestamp"    << ","
         << "column"       << ","
         << "row"          << ","
         << "start"        << ","
         << "end"          << ","
         << "reset"        << ","
         << "value"        << ","
         << "timer"        << ","
         << "payload"      << ",\n";
  }

  bool AIEProfilingWriter::write(bool)
  {
    if(!mHeaderWritten) {
      this->writeHeader();
      this->mHeaderWritten = true;
    }
    
    // Write all data elements
    std::vector<counters::Sample> samples =
      db->getDynamicInfo().moveAIESamples(mDeviceIndex);

    for (auto& sample : samples) {
      fout << sample.timestamp << ",";
      for (auto value : sample.values) {
        fout << value << ",";
      }
      fout << "\n";
    }
    fout.flush();
    return true;
  }

} // end namespace xdp
