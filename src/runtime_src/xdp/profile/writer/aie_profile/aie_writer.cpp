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
#include "xdp/profile/database/static_info/aie_util.h"
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
    /*
     * HEADER
       File Version,1.0
       Target device,edge
       Hardware generation,2
       Clock frequency (MHz),1250
    */
    float fileVersion = 1.0;
    
    // Report HW generation to inform analysis how to interpret event IDs
    auto aieGeneration = (db->getStaticInfo()).getAIEGeneration(mDeviceIndex);

    fout << "HEADER"<<"\n";
    fout << "File Version," <<fileVersion << "\n";
    fout << "Target device," << mDeviceName << "\n";
    fout << "Hardware generation," << static_cast<int>(aieGeneration) << "\n";

    // Grab AIE clock freq from first counter in metadata
    // NOTE: Assumed the same for all tiles
    auto aie = (db->getStaticInfo()).getAIECounter(mDeviceIndex, 0);
    double aieClockFreqMhz = (aie != nullptr) ?  aie->clockFreqMhz : 1200.0;
    fout << "Clock frequency (MHz)," << aieClockFreqMhz << "\n";
  }


  /*
    METRIC_SETS
    # AIE tile metric sets:
    8,1,s2mm_throughputs
    # Memory tile metric sets:
    5,0,s2mm_channels_details
    # Interface tile metric sets:
    6,0,mm2s_stalls
  */
  void AIEProfilingWriter::writeMetricSettings()
  {

    auto validConfig = (db->getStats()).getProfileConfig();
    std::vector<std::string> aieTileMetrics, memTileMetrics, intfTileMetrics;
    for(auto &cfg : validConfig.configMetrics) {
      
      for(auto &elm : cfg) {
        auto type = aie::getModuleType(elm.first.row, (db->getStaticInfo()).getAIEmetadataReader()->getAIETileRowOffset());
        if(type == module_type::core) {
          aieTileMetrics.push_back(std::to_string(elm.first.col) + "," + std::to_string(elm.first.row)+ "," + elm.second);
        }else if (type == module_type::mem_tile) {
          memTileMetrics.push_back(std::to_string(elm.first.col) + "," + std::to_string(elm.first.row)+ "," + elm.second);
        } else if (type == module_type::shim) {
          intfTileMetrics.push_back(std::to_string(elm.first.col) + "," + std::to_string(elm.first.row)+ "," + elm.second);
        } else {
          // Log a debug message
        }
      }
    }
    fout << "METRIC_SETS" << "\n";
    fout << "# AIE tile metric sets:" <<"\n";
    for(auto &setting : aieTileMetrics)
      fout << setting << "\n";

    fout << "# Memory tile metric sets:" << "\n";
    for(auto &setting : memTileMetrics)
      fout << setting << "\n";

    fout << "# Interface tile metric sets:" << "\n";
    for(auto &setting : intfTileMetrics)
      fout << setting << "\n";

  }

  void AIEProfilingWriter::writerDataColumnHeader()
  {
    // Write data columns header
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
      this->writeMetricSettings();
      this->writerDataColumnHeader();
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
