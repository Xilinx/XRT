#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "xdp/profile/database/database.h"
#include "xdp/profile/device/device_trace_logger.h"
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
  char* ch = static_cast<char*>(&packet);

  while(fin.read(ch, 8)) {
    traceData.push_back(packet);
  }
  fin.close();

  // Add all of the events to the database
  xdp::DeviceTraceLogger logger(deviceId);
  uint64_t numBytes = sizeof(uint64_t)*traceData.size();
  logger.processTraceData(traceData.data(), numBytes);

  // Create a writer and have it write.
  xdp::DeviceTraceWriter writer("output.csv", deviceId, "", "", "", "");
  writer.write(false);

  fin.close();

  return 0;
}
