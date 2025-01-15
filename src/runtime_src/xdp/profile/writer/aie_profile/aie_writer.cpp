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
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/writer/aie_profile/aie_writer.h"

namespace xdp {

  AIEProfilingWriter::AIEProfilingWriter(const char* fileName,
                                         const char* deviceName, uint64_t deviceIndex) :
    VPWriter(fileName),
    mDeviceName(deviceName),
    mDeviceIndex(deviceIndex), mHeaderWritten(false)
  {
  }

  void AIEProfilingWriter::writeHeader()
  {
    // Updated offsets for AIE mem, shim and mem_tile to 1000, 2000, 3000 respectively.
    float fileVersion = 1.1f;

    // Report HW generation to inform analysis how to interpret event IDs
    auto aieGeneration = (db->getStaticInfo()).getAIEGeneration(mDeviceIndex);

    fout << "HEADER"<<"\n";
    fout << "File Version: " <<fileVersion << "\n";
    fout << "Target device: " << mDeviceName << "\n";
    fout << "Hardware generation: " << static_cast<int>(aieGeneration) << "\n";

    // Grab AIE clock freq from first counter in metadata
    // NOTE: Assumed the same for all tiles
    auto aie = (db->getStaticInfo()).getAIECounter(mDeviceIndex, 0);
    double aieClockFreqMhz = (aie != nullptr) ?  aie->clockFreqMhz : 1200.0;
    fout << "Clock frequency (MHz): " << aieClockFreqMhz << "\n";
    fout << "\n"; 
  }

  void AIEProfilingWriter::writeMetricSettings()
  {
    auto metadataReader = (db->getStaticInfo()).getAIEmetadataReader();
    uint8_t col_shift = metadataReader->getPartitionOverlayStartCols().front();
    auto validConfig = (db->getStaticInfo()).getProfileConfig();

    std::map<module_type, std::vector<std::string>> filteredConfig;
    for(uint8_t i=0; i<static_cast<uint8_t>(module_type::num_types); i++)
      filteredConfig[static_cast<module_type>(i)] = std::vector<std::string>();

    const auto& configMetrics = validConfig.configMetrics;
    for(size_t i=0; i<configMetrics.size(); i++)
    {
      std::vector<std::string> metrics;

      const auto& validMetrics = configMetrics[i];
      for(auto &elm : validMetrics) {
        metrics.push_back(std::to_string(+(elm.first.col+col_shift)) + "," + \
                          aie::getRelativeRowStr(elm.first.row, validConfig.tileRowOffset) \
                          + "," + elm.second);
        if (i == module_type::shim && elm.second == METRIC_BYTE_COUNT) {
          if(validConfig.bytesTransferConfigMap.find(elm.first) != validConfig.bytesTransferConfigMap.end())
            metrics.back() += "," + std::to_string(+validConfig.bytesTransferConfigMap.at(elm.first));
        }
        else if (i == module_type::shim && elm.second == METRIC_LATENCY) {
          if(validConfig.latencyConfigMap.find(elm.first) != validConfig.latencyConfigMap.end())
            metrics.back() += "," + std::to_string(+validConfig.latencyConfigMap.at(elm.first).tranx_no);
        }
      }
      filteredConfig[static_cast<module_type>(i)] = metrics;
    }

    fout << "METRIC_SETS" << "\n";
    fout << "# AIE tile core module metric sets:" << "\n";
    for (const auto &setting : filteredConfig.at(module_type::core))
      fout << setting << "\n";

    fout << "# AIE tile memory module metric sets:" << "\n";
    for (const auto &setting : filteredConfig.at(module_type::dma))
      fout << setting << "\n";

    fout << "# Memory tile metric sets:" << "\n";
    for (const auto &setting : filteredConfig.at(module_type::mem_tile))
      fout << setting << "\n";

    fout << "# Interface tile metric sets:" << "\n";
    for (const auto &setting : filteredConfig.at(module_type::shim))
      fout << setting << "\n";

    fout << "# Microcontroller metric sets:" << "\n";
    for (const auto &setting : filteredConfig.at(module_type::uc))
      fout << setting << "\n";

    fout << "\n";
  }

  void AIEProfilingWriter::writerDataColumnHeader()
  {
    // Write data columns header
    fout << "METRIC_DATA" << "\n";
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
