/**
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
#include <fstream>
#include "xdp/profile/writer/aie_debug/aie_debug_writer.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/dynamic_event_database.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {
    AIEDebugWriter::AIEDebugWriter(const char* fileName, const char* deviceName,
                                   uint64_t deviceIndex):
      VPWriter(fileName), mDeviceName(deviceName), 
      mDeviceIndex(deviceIndex), mHeaderWritten(false) 
  {
    // Nothing to do
  }

  void AIEDebugWriter::writeHeader()
  {
    float fileVersion = 1.0;

    // Report HW generation to inform analysis how to interpret register addresses
    auto aieGeneration = (db->getStaticInfo()).getAIEGeneration(mDeviceIndex);

    fout << "HEADER"<<"\n";
    fout << "File Version: " << fileVersion << "\n";
    fout << "Target device: " << mDeviceName << "\n";
    fout << "Hardware generation: " << static_cast<int>(aieGeneration) << "\n";
  }

  void AIEDebugWriter::writerDataColumnHeader()
  {
    // Write data columns header
    fout << "Register Values" << "\n";
    fout << "Column"          << ","
         << "Row"             << ","
         << "Relative Offset" << ","
         << "Name"            << ","
         << "Value"           << ",\n";
  }

  bool AIEDebugWriter::write(bool /*openNewFile*/)
  {
    if (!mHeaderWritten) {
      this->writeHeader();
      this->writerDataColumnHeader();
      this->mHeaderWritten = true;
    }

    // Write all data elements
    std::vector<xdp::aie::AIEDebugDataType> samples =
      db->getDynamicInfo().moveAIEDebugSamples(mDeviceIndex);

    for (auto& sample : samples) {
      fout << +sample.col << ","
           << +sample.row << ","
           << "0x" << std::hex << sample.offset << ","
           << sample.name << ","
           << "0x" << std::hex << sample.value 
           << std::dec << "\n";
    }
    
    fout.flush();
    return true;
  }
} // end namespace xdp