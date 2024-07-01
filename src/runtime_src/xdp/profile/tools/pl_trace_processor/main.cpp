/**
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#include <iostream>
#include <string>
#include <vector>

#include "xdp/profile/database/database.h"
#include "xdp/profile/device/pl_device_trace_logger.h"
#include "xdp/profile/writer/device_trace/device_trace_writer.h"

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <Raw Trace File> <Xclbin>\n";
    return 0;
  }

  std::string traceFile  = argv[1];
  std::string xclbinFile = argv[2];

  std::ifstream fin(traceFile, std::ios::binary|std::ios::in);
  if (!fin) {
    std::cerr << "Cannot open raw trace file " << traceFile << std::endl;
    return 0;
  }

  // Create a database to store and interpret the events
  xdp::VPDatabase* db = xdp::VPDatabase::Instance();

  // Add metadata to the database from the xclbin
  auto deviceId = db->addDevice("local");

  db->getStaticInfo().updateDevice(deviceId, xclbinFile);

  std::vector<uint64_t> traceData;

  uint64_t packet = 0;
  char* ch = reinterpret_cast<char*>(&packet);

  while(fin.read(ch, 8)) {
    traceData.push_back(packet);
  }
  fin.close();

  // Add all of the events to the database
  xdp::PLDeviceTraceLogger logger(deviceId);
  uint64_t numBytes = sizeof(uint64_t)*traceData.size();
  logger.processTraceData(traceData.data(), numBytes);

  // Create a writer and have it write.
  xdp::DeviceTraceWriter writer("output.csv", deviceId, "1.1", xdp::getCurrentDateTime(), xdp::getXRTVersion(), xdp::getToolVersion());
  writer.write(false);

  fin.close();

  return 0;
}

