/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/writer/aie_trace/aie_trace_timestamps_writer.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/database.h"

#include <vector>
#include <iostream>
#include <iomanip>

namespace xdp {

  AIETraceTimestampsWriter::AIETraceTimestampsWriter(const char* fileName,
                                                     const char* deviceName, 
                                                     uint64_t deviceIndex) :
    VPWriter(fileName),
    mDeviceName(deviceName),
    mDeviceIndex(deviceIndex)
  {
  }

  AIETraceTimestampsWriter::~AIETraceTimestampsWriter()
  {
  }

  bool AIETraceTimestampsWriter::write(bool)
  {
    // Report HW generation and clock frequency
    auto aieGeneration = (db->getStaticInfo()).getAIEGeneration(mDeviceIndex);
    double aieClockFreqMhz = (db->getStaticInfo()).getClockRateMHz(mDeviceIndex, false);

    std::ofstream fos;
    fos.open( getcurrentFileName() );

    // Write header
    fos << "Version: 1.0\n";
    fos << "Target device: " << mDeviceName << "\n";
    fos << "Hardware generation: " << static_cast<int>(aieGeneration) << "\n";
    fos << "Clock frequency (MHz): " << aieClockFreqMhz << "\n";
    fos << "timestamp1(nsec)" << ","
        << "timestamp2(nsec)" << ","
        << "column"           << ","
        << "row"              << ","
        << "timer(cycles)"    << ",\n";

    // Write all data elements
    std::vector<counters::DoubleSample> samples =
        db->getDynamicInfo().getAIETimerSamples(mDeviceIndex);

    for (auto& sample : samples) {
      fos << sample.timestamp1 << ","
          << sample.timestamp2 << ",";
      for (auto value : sample.values)
        fos << value << ",";
      fos << "\n";
    }

    fos.close();
    return true;
  }

} // end namespace xdp
