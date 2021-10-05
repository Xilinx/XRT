/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportDebugIpStatus.h"

#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/include/xrt.h"

#include "core/common/system.h"
#include "core/common/utils.h"
#include "core/common/error.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xcl_perfmon_parameters.h"
#include "core/include/xcl_axi_checker_codes.h"
#include "core/include/experimental/xrt-next.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>

// System - Include Files
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>


namespace {

void
xclReadWrapper(xclDeviceHandle handle, enum xclAddressSpace space,
        uint64_t offset, void *hostbuf, size_t size)
{
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  (void)xclRead(handle, space, offset, hostbuf, size);
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif
}

static const char* debugIpNames[DEBUG_IP_TYPE_MAX] = {
  "unknown",
  "Light Weight AXI Protocol Checker (lapc)",
  "Integrated Logic Analyzer (ila)",
  "AXI Interface Monitor (aim)",
  "TraceFunnel",
  "TraceFifoLite",
  "Trace FIFO (fifo)",
  "Accelerator Monitor (am)",
  "AXI Stream Monitor (asm)",
  "AXI Stream Protocol Checker (spc)",
  "Trace Stream to Memory (ts2mm)",
  "AxiDMA",
  "TS2MMFull",
  "AxiNOC",
  "Accelerator Deadlock Detector (accel_deadlock_detector)"
};

static size_t cuNameMaxStrLen[DEBUG_IP_TYPE_MAX] = {0};
static size_t portNameMaxStrLen[DEBUG_IP_TYPE_MAX] = {0};

class DebugIpStatusCollector
{

  xclDeviceHandle handle;

  std::string infoMessage ;
  std::vector<char> map;

  uint64_t debugIpNum[DEBUG_IP_TYPE_MAX];
  bool     debugIpOpt[DEBUG_IP_TYPE_MAX];

  std::vector<std::string> cuNames[DEBUG_IP_TYPE_MAX];
  std::vector<std::string> portNames[DEBUG_IP_TYPE_MAX];

  xclDebugCountersResults          aimResults;
  xclStreamingDebugCountersResults asmResults;
  xclAccelMonitorCounterResults    amResults;
  xclDebugCheckersResults          lapcResults;
  xclDebugStreamingCheckersResults spcResults;
  xclAccelDeadlockDetectorResults  accelDeadlockResults;

public :
  DebugIpStatusCollector(xclDeviceHandle h, bool jsonFormat, std::ostream& _output = std::cout);
  ~DebugIpStatusCollector() {}

  inline std::string getInfoMessage() { return infoMessage ; }

  void collect();

  void populateOverview(boost::property_tree::ptree &_pt);
  void populateAllResults(boost::property_tree::ptree &_pt);

private :

  debug_ip_layout* getDebugIpLayout();

  void getDebugIpData();
  void getCuNamePortName(uint8_t type, std::string& dbgIpName, std::string& cuName, std::string& portName); 
  void getStreamName(uint8_t type, std::string& dbgIpName, std::string& masterName, std::string& slaveName); 

  void readAIMCounter(debug_ip_data*);
  void readAMCounter(debug_ip_data*);
  void readASMCounter(debug_ip_data*);
  void readLAPChecker(debug_ip_data*);
  void readSPChecker(debug_ip_data*);
  void readAccelDeadlockDetector(debug_ip_data*);

  void populateAIMResults(boost::property_tree::ptree &_pt);
  void populateAMResults(boost::property_tree::ptree &_pt);
  void populateASMResults(boost::property_tree::ptree &_pt);
  void populateFIFOResults(boost::property_tree::ptree &_pt);
  void populateTS2MMResults(boost::property_tree::ptree &_pt);
  void populateLAPCResults(boost::property_tree::ptree &_pt);
  void populateSPCResults(boost::property_tree::ptree &_pt);
  void populateILAResults(boost::property_tree::ptree &_pt);
  void populateAccelDeadlockResults(boost::property_tree::ptree &_pt);

};


DebugIpStatusCollector::DebugIpStatusCollector(xclDeviceHandle h,
					       bool jsonFormat,
					       std::ostream& _output)
    : handle(h)
    , infoMessage("")
    , debugIpNum{0}
    , debugIpOpt{false}
    , aimResults{0}
    , asmResults{0}
    , amResults{0}
    , lapcResults{0}
    , spcResults{0}
    , accelDeadlockResults{0}
{
  // By default, enable status collection for all Debug IP types
  std::fill(debugIpOpt, debugIpOpt + DEBUG_IP_TYPE_MAX, true);

#ifdef _WIN32
  size_t sz1 = 0, sectionSz = 0;
  // Get the size of full debug_ip_layout
  xclGetDebugIpLayout(handle, nullptr, sz1, &sectionSz);
  if(sectionSz == 0) {
    if (jsonFormat) {
      infoMessage = "Failed to find any Debug IP Layout section in the bitstream loaded on device. Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded." ;
    } else {
      _output << "  INFO: Failed to find any Debug IP Layout section in the bitstream loaded on device. "
		          << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n"
		          << std::endl;
    }
   return;
  }
  // Allocate buffer to retrieve debug_ip_layout information from loaded xclbin
  map.resize(sectionSz);
  xclGetDebugIpLayout(handle, map.data(), sectionSz, &sz1);
#else
  std::vector<char> layoutPath;
  layoutPath.resize(512);
  xclGetDebugIPlayoutPath(handle, layoutPath.data(), 512);
  std::string path(layoutPath.data());

  if(path.empty()) {
    _output << "  INFO: Failed to find path to Debug IP Layout. "
            << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n"
            << std::endl;
    return;
  }

  std::ifstream ifs(path.c_str(), std::ifstream::binary);
  if(!ifs) {
    return;
  }

  // debug_ip_layout max size is 65536
  map.resize(65536);
  ifs.read(map.data(), 65536);

  if (ifs.gcount() <= 0) {
    if (jsonFormat) {
      infoMessage = "Failed to find any Debug IP Layout section in the bitstream loaded on device. Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded." ;
    } else {
      _output << "INFO: Failed to find any Debug IP Layout section in the bitstream loaded on device. "
              << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n"
              << std::endl;
    }
  }

#endif
}


debug_ip_layout*
DebugIpStatusCollector::getDebugIpLayout()
{
  if(0 == map.size()) {
    std::cout << " INFO: Debug IP Data is not populated." << std::endl;
    return nullptr;
  }
  debug_ip_layout* dbgIpLayout = reinterpret_cast<debug_ip_layout*>(map.data());
  if(0 == dbgIpLayout->m_count) {
    //std::cout << "INFO: Failed to find any Debug IPs in the bitstream loaded on device." << std::endl;
    return nullptr;
  }
  return dbgIpLayout;
}

void 
DebugIpStatusCollector::collect()
{ 
  getDebugIpData();
}

void 
DebugIpStatusCollector::getDebugIpData()
{
  debug_ip_layout* dbgIpLayout = getDebugIpLayout();
  if(nullptr == dbgIpLayout)
    return;

  // reset debugIpNum to zero
  std::memset((char*)debugIpNum, 0, sizeof(debugIpNum));

  for(uint64_t i = 0; i < dbgIpLayout->m_count; i++) {
    switch(dbgIpLayout->m_debug_ip_data[i].m_type)
    {
      case AXI_MM_MONITOR : 
      {
        if(debugIpOpt[AXI_MM_MONITOR])
          readAIMCounter(&(dbgIpLayout->m_debug_ip_data[i]));
        break;
      }
      case ACCEL_MONITOR : 
      {
        if(debugIpOpt[ACCEL_MONITOR])
          readAMCounter(&(dbgIpLayout->m_debug_ip_data[i]));
        break;
      }
      case AXI_STREAM_MONITOR : 
      {
        if(debugIpOpt[AXI_STREAM_MONITOR])
          readASMCounter(&(dbgIpLayout->m_debug_ip_data[i]));
        break;
      }
      case AXI_MONITOR_FIFO_FULL :
      {
        if(debugIpOpt[AXI_MONITOR_FIFO_FULL])
          ++debugIpNum[AXI_MONITOR_FIFO_FULL];
        break;
      }
      case TRACE_S2MM :
      {
        if(debugIpOpt[TRACE_S2MM])
          ++debugIpNum[TRACE_S2MM];
        break;
      }
      case LAPC :
      {
        if(debugIpOpt[LAPC])
          readLAPChecker(&(dbgIpLayout->m_debug_ip_data[i]));
        break;
      }
      case AXI_STREAM_PROTOCOL_CHECKER :
      {
        if(debugIpOpt[AXI_STREAM_PROTOCOL_CHECKER])
          readSPChecker(&(dbgIpLayout->m_debug_ip_data[i]));
        break;
      }
      case ILA :
      {
        if(debugIpOpt[ILA])
          ++debugIpNum[ILA];
        break;
      }
      case ACCEL_DEADLOCK_DETECTOR : 
      {
        if(debugIpOpt[ACCEL_DEADLOCK_DETECTOR])
          readAccelDeadlockDetector(&(dbgIpLayout->m_debug_ip_data[i]));
        break;
      }
      default: break;
    }
  }
}

void 
DebugIpStatusCollector::getCuNamePortName(uint8_t dbgIpType,
                          std::string& dbgIpName, 
                          std::string& cuName, 
                          std::string& portName)
{
  //Slotnames are of the format "/cuname/portname" or "cuname/portname", split them and populate cuname and portName
  const char sep = '/';

  size_t start = 0;
  size_t sepPos = dbgIpName.find(sep, start);
  if(0 == sepPos) {
    //the cuname starts with a '/'
    start = 1;
    sepPos = dbgIpName.find(sep, start);
  }
  if(sepPos != std::string::npos) {
    cuName   = dbgIpName.substr(start, sepPos-start);
    portName = dbgIpName.substr(sepPos+1);
  } else {
    cuName   = "Unknown";
    portName = "Unknown";
  }
  if(cuName.find("interconnect_host_aximm") != std::string::npos) {
    cuName   = "XDMA";
    portName = "N/A";
  }
  cuNameMaxStrLen[dbgIpType]   = std::max(strlen(cuName.c_str()), cuNameMaxStrLen[dbgIpType]);
  portNameMaxStrLen[dbgIpType] = std::max(strlen(portName.c_str()), portNameMaxStrLen[dbgIpType]);
}

void 
DebugIpStatusCollector::getStreamName(uint8_t dbgIpType,
                        std::string& dbgIpName, 
                        std::string& masterName, 
                        std::string& slaveName)
{
  // Slotnames are of the format "Master-Slave", split them and store in masterName and slaveName
  size_t sepPos = dbgIpName.find(IP_LAYOUT_SEP, 0);
  if(sepPos != std::string::npos) {
    masterName = dbgIpName.substr(0, sepPos);
    slaveName  = dbgIpName.substr(sepPos+1);
  } else {
    masterName = "Unknown";
    slaveName  = "Unknown";
  }
  cuNameMaxStrLen[dbgIpType] = std::max(masterName.length(), cuNameMaxStrLen[dbgIpType]);
  portNameMaxStrLen[dbgIpType] = std::max(slaveName.length(), portNameMaxStrLen[dbgIpType]);
}


void 
DebugIpStatusCollector::readAIMCounter(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  const uint64_t index = debugIpNum[AXI_MM_MONITOR];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, sizeof(dbgIpInfo->m_name));
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  {
    std::string cuName;
    std::string portName;
    getCuNamePortName(dbgIpInfo->m_type, dbgIpName, cuName, portName);
    cuNames[AXI_MM_MONITOR].emplace_back(std::move(cuName));
    portNames[AXI_MM_MONITOR].emplace_back(std::move(portName));
  }

  // increment debugIpNum
  ++debugIpNum[AXI_MM_MONITOR];
  aimResults.NumSlots = (unsigned int)debugIpNum[AXI_MM_MONITOR];

#ifndef _WIN32
  // read counter values
  xrt_core::system::monitor_access_type accessType = xrt_core::get_monitor_access_type();
  if(xrt_core::system::monitor_access_type::ioctl == accessType) {
    std::string aimName("aximm_mon_");
    aimName = aimName + std::to_string(dbgIpInfo->m_base_address);

    std::vector<char> nameSysfsPath;
    nameSysfsPath.resize(512);
    xclGetSysfsPath(handle, aimName.c_str(), "name", nameSysfsPath.data(), 512);
    std::string namePath(nameSysfsPath.data());

    std::size_t pos = namePath.find_last_of('/');
    std::string path = namePath.substr(0, pos+1);
    path += "counters";

    std::ifstream ifs(path.c_str());
    if(!ifs) {
      return;
    }

    const size_t sz = 256;
    char buffer[sz];
    std::memset(buffer, 0, sz);
    ifs.getline(buffer, sz);

    std::vector<uint64_t> valBuf;

    while(!ifs.eof()) {
      valBuf.push_back(strtoull((const char*)(&buffer), NULL, 10));
      std::memset(buffer, 0, sz);
      ifs.getline(buffer, sz);
    }

    if(valBuf.size() < 13) {
      std::cout << "\nERROR: Incomplete AIM counter data in " << path << std::endl;
      ifs.close();
      return;
    }

    aimResults.WriteBytes[index]      = valBuf[0];
    aimResults.WriteTranx[index]      = valBuf[1];
    aimResults.ReadBytes[index]       = valBuf[4];
    aimResults.ReadTranx[index]       = valBuf[5];
    aimResults.OutStandCnts[index]    = valBuf[8];
    aimResults.LastWriteAddr[index]   = valBuf[9];
    aimResults.LastWriteData[index]   = valBuf[10];
    aimResults.LastReadAddr[index]    = valBuf[11];
    aimResults.LastReadData[index]    = valBuf[12];

    ifs.close();

    return;
  }
#endif

  // read counter values
  static const uint64_t aim_offsets[] = {
    XAIM_SAMPLE_WRITE_BYTES_OFFSET,
    XAIM_SAMPLE_WRITE_TRANX_OFFSET,
    XAIM_SAMPLE_READ_BYTES_OFFSET,
    XAIM_SAMPLE_READ_TRANX_OFFSET,
    XAIM_SAMPLE_OUTSTANDING_COUNTS_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_DATA_OFFSET,
    XAIM_SAMPLE_LAST_READ_ADDRESS_OFFSET,
    XAIM_SAMPLE_LAST_READ_DATA_OFFSET
  };

  static const uint64_t aim_upper_offsets[] = {
    XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET,
    XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET,
    XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET,
    XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET,
    XAIM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET,
    XAIM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET
  };


  uint32_t currData[XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT];

  uint32_t sampleInterval;
  // Read sample interval register to latch the sampled metric counters
  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                    dbgIpInfo->m_base_address + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, sizeof(uint32_t));

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbgIpInfo->m_properties & XAIM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
      xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                 dbgIpInfo->m_base_address + aim_upper_offsets[c], &currData[c], sizeof(uint32_t));
    }
    aimResults.WriteBytes[index]    = ((uint64_t)(currData[0])) << 32 ;
    aimResults.WriteTranx[index]    = ((uint64_t)(currData[1])) << 32 ;
    aimResults.ReadBytes[index]     = ((uint64_t)(currData[2])) << 32 ;
    aimResults.ReadTranx[index]     = ((uint64_t)(currData[3])) << 32 ;
    aimResults.OutStandCnts[index]  = ((uint64_t)(currData[4])) << 32 ;
    aimResults.LastWriteAddr[index] = ((uint64_t)(currData[5])) << 32 ;
    aimResults.LastWriteData[index] = ((uint64_t)(currData[6])) << 32 ;
    aimResults.LastReadAddr[index]  = ((uint64_t)(currData[7])) << 32 ;
    aimResults.LastReadData[index]  = ((uint64_t)(currData[8])) << 32 ;
  }

  for (int c=0; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++) {
    xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, 
                       dbgIpInfo->m_base_address + aim_offsets[c], &currData[c], sizeof(uint32_t));
  }

  aimResults.WriteBytes[index]    |= currData[0];
  aimResults.WriteTranx[index]    |= currData[1];
  aimResults.ReadBytes[index]     |= currData[2];
  aimResults.ReadTranx[index]     |= currData[3];
  aimResults.OutStandCnts[index]  |= currData[4];
  aimResults.LastWriteAddr[index] |= currData[5];
  aimResults.LastWriteData[index] |= currData[6];
  aimResults.LastReadAddr[index]  |= currData[7];
  aimResults.LastReadData[index]  |= currData[8];
}


void 
DebugIpStatusCollector::readAMCounter(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  const uint64_t index = debugIpNum[ACCEL_MONITOR];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, sizeof(dbgIpInfo->m_name));
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  {
    std::string cuName = dbgIpName;
    cuNameMaxStrLen[ACCEL_MONITOR] = std::max(strlen(cuName.c_str()), cuNameMaxStrLen[ACCEL_MONITOR]);
    cuNames[ACCEL_MONITOR].emplace_back(std::move(cuName));
    portNames[ACCEL_MONITOR].emplace_back("N/A");
  }

  // increment debugIpNum
  ++debugIpNum[ACCEL_MONITOR];
  amResults.NumSlots = (unsigned int)debugIpNum[ACCEL_MONITOR];

#ifndef _WIN32
  // read counter values
  xrt_core::system::monitor_access_type accessType = xrt_core::get_monitor_access_type();
  if(xrt_core::system::monitor_access_type::ioctl == accessType) {
    std::string amName("accel_mon_");
    amName = amName + std::to_string(dbgIpInfo->m_base_address);

    std::vector<char> nameSysfsPath;
    nameSysfsPath.resize(512);
    xclGetSysfsPath(handle, amName.c_str(), "name", nameSysfsPath.data(), 512);
    std::string namePath(nameSysfsPath.data());

    std::size_t pos = namePath.find_last_of('/');
    std::string path = namePath.substr(0, pos+1);
    path += "counters";

    std::ifstream ifs(path.c_str());
    if(!ifs) {
      return; 
    }

    const size_t sz = 256;
    char buffer[sz];
    std::memset(buffer, 0, sz);
    ifs.getline(buffer, sz);

    std::vector<uint64_t> valBuf;

    while(!ifs.eof()) {
      valBuf.push_back(strtoull((const char*)(&buffer), NULL, 10));
      std::memset(buffer, 0, sz);
      ifs.getline(buffer, sz);
    }

    if(valBuf.size() < 10) {
      std::cout << "\nERROR: Incomplete AM counter data in " << path << std::endl;
      ifs.close();
      return; 
    }

    amResults.CuExecCount[index]        = valBuf[0];
    amResults.CuStartCount[index]       = valBuf[1];
    amResults.CuExecCycles[index]       = valBuf[2];

    amResults.CuStallIntCycles[index]   = valBuf[3];
    amResults.CuStallStrCycles[index]   = valBuf[4];
    amResults.CuStallExtCycles[index]   = valBuf[5];

    amResults.CuBusyCycles[index]       = valBuf[6];
    amResults.CuMaxParallelIter[index]  = valBuf[7];
    amResults.CuMaxExecCycles[index]    = valBuf[8];
    amResults.CuMinExecCycles[index]    = valBuf[9];

    ifs.close();
    return; 
  } 
#endif

 // read counter values
  static const uint64_t am_offsets[] = {
    XAM_ACCEL_EXECUTION_COUNT_OFFSET,
    XAM_ACCEL_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_STALL_INT_OFFSET,
    XAM_ACCEL_STALL_STR_OFFSET,
    XAM_ACCEL_STALL_EXT_OFFSET,
    XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_TOTAL_CU_START_OFFSET
  };

  static const uint64_t am_upper_offsets[] = {
    XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET,
    XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_STALL_INT_UPPER_OFFSET,
    XAM_ACCEL_STALL_STR_UPPER_OFFSET,
    XAM_ACCEL_STALL_EXT_UPPER_OFFSET,
    XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET,
    XAM_ACCEL_TOTAL_CU_START_UPPER_OFFSET
  };

  // Read all metric counters
  uint32_t currData[XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] = {0};

  uint32_t sampleInterval;
  // Read sample interval register to latch the sampled metric counters
  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                  dbgIpInfo->m_base_address + XAM_SAMPLE_OFFSET,
                  &sampleInterval, sizeof(uint32_t));

  auto dbgIpVersion = std::make_pair(dbgIpInfo->m_major, dbgIpInfo->m_minor);
  auto refVersion   = std::make_pair((uint8_t)1, (uint8_t)1);

  bool hasDataflow = (dbgIpVersion > refVersion) ? true : false;

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbgIpInfo->m_properties & XAM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
      xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
            dbgIpInfo->m_base_address + am_upper_offsets[c],
            &currData[c], sizeof(uint32_t));
    }
    amResults.CuExecCount[index]      = ((uint64_t)(currData[0])) << 32;
    amResults.CuExecCycles[index]     = ((uint64_t)(currData[1])) << 32;
    amResults.CuStallExtCycles[index] = ((uint64_t)(currData[2])) << 32;
    amResults.CuStallIntCycles[index] = ((uint64_t)(currData[3])) << 32;
    amResults.CuStallStrCycles[index] = ((uint64_t)(currData[4])) << 32;
    amResults.CuMinExecCycles[index]  = ((uint64_t)(currData[5])) << 32;
    amResults.CuMaxExecCycles[index]  = ((uint64_t)(currData[6])) << 32;
    amResults.CuStartCount[index]     = ((uint64_t)(currData[7])) << 32;

    if(hasDataflow) {
      uint64_t dfTmp[2] = {0};
      xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_BUSY_CYCLES_UPPER_OFFSET, &dfTmp[0], sizeof(uint32_t));
      xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET, &dfTmp[1], sizeof(uint32_t));

      amResults.CuBusyCycles[index]      = dfTmp[0] << 32;
      amResults.CuMaxParallelIter[index] = dfTmp[1] << 32;
    }
  }

  for (int c=0; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++) {
    xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address+am_offsets[c], &currData[c], sizeof(uint32_t));
  }

  amResults.CuExecCount[index]      |= currData[0];
  amResults.CuExecCycles[index]     |= currData[1];
  amResults.CuStallExtCycles[index] |= currData[2];
  amResults.CuStallIntCycles[index] |= currData[3];
  amResults.CuStallStrCycles[index] |= currData[4];
  amResults.CuMinExecCycles[index]  |= currData[5];
  amResults.CuMaxExecCycles[index]  |= currData[6];
  amResults.CuStartCount[index]     |= currData[7];

  if(hasDataflow) {
    uint64_t dfTmp[2] = {0};
    xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_BUSY_CYCLES_OFFSET, &dfTmp[0], sizeof(uint32_t));
    xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_MAX_PARALLEL_ITER_OFFSET, &dfTmp[1], sizeof(uint32_t));

    amResults.CuBusyCycles[index]      |= dfTmp[0] << 32;
    amResults.CuMaxParallelIter[index] |= dfTmp[1] << 32;
  } else {
    amResults.CuBusyCycles[index]      = amResults.CuExecCycles[index];
    amResults.CuMaxParallelIter[index] = 1;
  }
}


void 
DebugIpStatusCollector::readASMCounter(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  const uint64_t index = debugIpNum[AXI_STREAM_MONITOR];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, sizeof(dbgIpInfo->m_name));
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Master and Slave Port Name
  {
    std::string masterName;
    std::string slaveName;
    getStreamName(dbgIpInfo->m_type, dbgIpName, masterName, slaveName);
    cuNames[AXI_STREAM_MONITOR].emplace_back(std::move(masterName));
    portNames[AXI_STREAM_MONITOR].emplace_back(std::move(slaveName));
  }

  // increment debugIpNum
  ++debugIpNum[AXI_STREAM_MONITOR];
  asmResults.NumSlots = (unsigned int)debugIpNum[AXI_STREAM_MONITOR];

#ifndef _WIN32
  // read counter values
  xrt_core::system::monitor_access_type accessType = xrt_core::get_monitor_access_type();
  if(xrt_core::system::monitor_access_type::ioctl == accessType) {
    std::string asmName("axistream_mon_");
    asmName = asmName + std::to_string(dbgIpInfo->m_base_address);

    std::vector<char> nameSysfsPath;
    nameSysfsPath.resize(512);
    xclGetSysfsPath(handle, asmName.c_str(), "name", nameSysfsPath.data(), 512);
    std::string namePath(nameSysfsPath.data());

    std::size_t pos = namePath.find_last_of('/');
    std::string path = namePath.substr(0, pos+1);
    path += "counters";

    std::ifstream ifs(path.c_str());
    if(!ifs) {
      return;
    }

    const size_t sz = 256;
    char buffer[sz];
    std::memset(buffer, 0, sz);
    ifs.getline(buffer, sz);

    std::vector<uint64_t> valBuf;

    while(!ifs.eof()) {
      valBuf.push_back(strtoull((const char*)(&buffer), NULL, 10));
      std::memset(buffer, 0, sz);
      ifs.getline(buffer, sz);
    }

    if(valBuf.size() < 5) {
      std::cout << "\nERROR: Incomplete ASM counter data in " << path << std::endl;
      ifs.close();
      return;
    }

    asmResults.StrNumTranx[index]     = valBuf[0];
    asmResults.StrDataBytes[index]    = valBuf[1];
    asmResults.StrBusyCycles[index]   = valBuf[2];
    asmResults.StrStallCycles[index]  = valBuf[3];
    asmResults.StrStarveCycles[index] = valBuf[4];

    ifs.close();
    return;
  }
#endif

  // Fill up the portions of the return struct that are known by the runtime

  // Fill up the return structure with the values read from the hardware
  static const uint64_t asm_offsets[] = {
    XASM_NUM_TRANX_OFFSET,
    XASM_DATA_BYTES_OFFSET,
    XASM_BUSY_CYCLES_OFFSET,
    XASM_STALL_CYCLES_OFFSET,
    XASM_STARVE_CYCLES_OFFSET
  };

  uint32_t sampleInterval ;
  // Read sample interval register to latch the sampled metric counters
  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
             dbgIpInfo->m_base_address + XASM_SAMPLE_OFFSET,
             &sampleInterval, sizeof(uint32_t));

  // Then read all the individual 64-bit counters
  unsigned long long int currData[XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] ;

  for (unsigned int j = 0 ; j < XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; ++j) {
    xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
               dbgIpInfo->m_base_address + asm_offsets[j],
               &currData[j], sizeof(unsigned long long int));
  }
  asmResults.StrNumTranx[index] = currData[0] ;
  asmResults.StrDataBytes[index] = currData[1] ;
  asmResults.StrBusyCycles[index] = currData[2] ;
  asmResults.StrStallCycles[index] = currData[3] ;
  asmResults.StrStarveCycles[index] = currData[4] ;
}


void 
DebugIpStatusCollector::readLAPChecker(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  const uint64_t index = debugIpNum[LAPC];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, sizeof(dbgIpInfo->m_name));
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  {
    std::string cuName;
    std::string portName;
    getCuNamePortName(dbgIpInfo->m_type, dbgIpName, cuName, portName);
    cuNames[LAPC].emplace_back(std::move(cuName));
    portNames[LAPC].emplace_back(std::move(portName));
  }

  // increment debugIpNum
  ++debugIpNum[LAPC];
  lapcResults.NumSlots = (unsigned int)debugIpNum[LAPC];

#ifndef _WIN32
  xrt_core::system::monitor_access_type accessType = xrt_core::get_monitor_access_type();
  if(xrt_core::system::monitor_access_type::ioctl == accessType) {
    std::string lapcName("lapc_");
    lapcName = lapcName + std::to_string(dbgIpInfo->m_base_address);

    std::vector<char> nameSysfsPath;
    nameSysfsPath.resize(512);
    xclGetSysfsPath(handle, lapcName.c_str(), "name", nameSysfsPath.data(), 512);
    std::string namePath(nameSysfsPath.data());

    std::size_t pos = namePath.find_last_of('/');
    std::string path = namePath.substr(0, pos+1);
    path += "status";

    std::ifstream ifs(path.c_str());
    if(!ifs) {
      return;
    }

    const size_t sz = 256;
    char buffer[sz];
    std::memset(buffer, 0, sz);
    ifs.getline(buffer, sz);

    std::vector<uint64_t> valBuf;

    while(!ifs.eof()) {
      valBuf.push_back(strtoull((const char*)(&buffer), NULL, 10));
      std::memset(buffer, 0, sz);
      ifs.getline(buffer, sz);
    }

    if(valBuf.size() < 9) {
      std::cout << "\nERROR: Incomplete LAPC data in " << path << std::endl;
      ifs.close();
      return;
    }

    lapcResults.OverallStatus[index]       = valBuf[0];

    lapcResults.CumulativeStatus[index][0] = valBuf[1];
    lapcResults.CumulativeStatus[index][1] = valBuf[2];
    lapcResults.CumulativeStatus[index][2] = valBuf[3];
    lapcResults.CumulativeStatus[index][3] = valBuf[4];

    lapcResults.SnapshotStatus[index][0]   = valBuf[5];
    lapcResults.SnapshotStatus[index][1]   = valBuf[6];
    lapcResults.SnapshotStatus[index][2]   = valBuf[7];
    lapcResults.SnapshotStatus[index][3]   = valBuf[8];

    ifs.close();
    return;
  }
#endif

  static const uint64_t statusRegisters[] = {
    LAPC_OVERALL_STATUS_OFFSET,

    LAPC_CUMULATIVE_STATUS_0_OFFSET, LAPC_CUMULATIVE_STATUS_1_OFFSET,
    LAPC_CUMULATIVE_STATUS_2_OFFSET, LAPC_CUMULATIVE_STATUS_3_OFFSET,

    LAPC_SNAPSHOT_STATUS_0_OFFSET, LAPC_SNAPSHOT_STATUS_1_OFFSET,
    LAPC_SNAPSHOT_STATUS_2_OFFSET, LAPC_SNAPSHOT_STATUS_3_OFFSET
  };

  uint32_t currData[XLAPC_STATUS_PER_SLOT];
  
  for (int c=0; c < XLAPC_STATUS_PER_SLOT; c++) {
    xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_CHECKER, dbgIpInfo->m_base_address+statusRegisters[c], &currData[c], sizeof(uint32_t));
  }

  lapcResults.OverallStatus[index]      = currData[XLAPC_OVERALL_STATUS];
  std::copy(currData+XLAPC_CUMULATIVE_STATUS_0, currData+XLAPC_SNAPSHOT_STATUS_0, lapcResults.CumulativeStatus[index]);
  std::copy(currData+XLAPC_SNAPSHOT_STATUS_0, currData+XLAPC_STATUS_PER_SLOT, lapcResults.SnapshotStatus[index]);
}


void 
DebugIpStatusCollector::readSPChecker(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  const uint64_t index = debugIpNum[AXI_STREAM_PROTOCOL_CHECKER];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, sizeof(dbgIpInfo->m_name));
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  {
    std::string cuName;
    std::string portName;
    getCuNamePortName(dbgIpInfo->m_type, dbgIpName, cuName, portName);
    cuNames[AXI_STREAM_PROTOCOL_CHECKER].emplace_back(std::move(cuName));
    portNames[AXI_STREAM_PROTOCOL_CHECKER].emplace_back(std::move(portName));
  }

  // increment debugIpNum
  ++debugIpNum[AXI_STREAM_PROTOCOL_CHECKER];
  spcResults.NumSlots = (unsigned int)debugIpNum[AXI_STREAM_PROTOCOL_CHECKER];

#ifndef _WIN32
  xrt_core::system::monitor_access_type accessType = xrt_core::get_monitor_access_type();
  if(xrt_core::system::monitor_access_type::ioctl == accessType) {
    std::string spcName("spc_");
    spcName = spcName + std::to_string(dbgIpInfo->m_base_address);

    std::vector<char> nameSysfsPath;
    nameSysfsPath.resize(512);
    xclGetSysfsPath(handle, spcName.c_str(), "name", nameSysfsPath.data(), 512);
    std::string namePath(nameSysfsPath.data());

    std::size_t pos = namePath.find_last_of('/');
    std::string path = namePath.substr(0, pos+1);
    path += "status";

    std::ifstream ifs(path.c_str());
    if(!ifs) {
      return;
    }

    const size_t sz = 256;
    char buffer[sz];
    std::memset(buffer, 0, sz);
    ifs.getline(buffer, sz);

    std::vector<uint64_t> valBuf;

    while(!ifs.eof()) {
      valBuf.push_back(strtoull((const char*)(&buffer), NULL, 10));
      std::memset(buffer, 0, sz);
      ifs.getline(buffer, sz);
    }

    if(valBuf.size() < 3) {
      std::cout << "\nERROR: Incomplete SPC data in " << path << std::endl;
      ifs.close();
      return;
    }

    spcResults.PCAsserted[index] = valBuf[0];
    spcResults.CurrentPC[index]  = valBuf[1];
    spcResults.SnapshotPC[index] = valBuf[2];

    ifs.close();
    return;
  }
#endif

  uint32_t pc_asserted ;
  uint32_t current_pc ;
  uint32_t snapshot_pc ;

  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_CHECKER,
              dbgIpInfo->m_base_address + XSPC_PC_ASSERTED_OFFSET,
              &pc_asserted, sizeof(uint32_t));
  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_CHECKER,
              dbgIpInfo->m_base_address + XSPC_CURRENT_PC_OFFSET,
              &current_pc, sizeof(uint32_t));
  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_CHECKER,
              dbgIpInfo->m_base_address + XSPC_SNAPSHOT_PC_OFFSET,
              &snapshot_pc, sizeof(uint32_t));

  spcResults.PCAsserted[index] = pc_asserted;
  spcResults.CurrentPC[index]  = current_pc;
  spcResults.SnapshotPC[index] = snapshot_pc;
}


void 
DebugIpStatusCollector::readAccelDeadlockDetector(debug_ip_data* dbgIpInfo)
{
  // increment debugIpNum
  ++debugIpNum[ACCEL_DEADLOCK_DETECTOR];    // only 1 per xclbin 
  accelDeadlockResults.Num = (unsigned int)debugIpNum[ACCEL_DEADLOCK_DETECTOR];

#ifndef _WIN32
  // read counter values
  xrt_core::system::monitor_access_type accessType = xrt_core::get_monitor_access_type();
  if(xrt_core::system::monitor_access_type::ioctl == accessType) {
    std::string monName("accel_deadlock_");
    monName = monName + std::to_string(dbgIpInfo->m_base_address);

    std::vector<char> nameSysfsPath;
    nameSysfsPath.resize(512);
    xclGetSysfsPath(handle, monName.c_str(), "name", nameSysfsPath.data(), 512);
    std::string namePath(nameSysfsPath.data());

    std::size_t pos = namePath.find_last_of('/');
    std::string path = namePath.substr(0, pos+1);
    path += "status";

    std::ifstream ifs(path.c_str());
    if(!ifs) {
      return;
    }

    const size_t sz = 256;
    char buffer[sz];
    std::memset(buffer, 0, sz);
    ifs.getline(buffer, sz);

    if(!ifs.eof()) {
      accelDeadlockResults.DeadlockStatus = strtoull((const char*)(&buffer), NULL, 10);
    } else {
      std::cout << "\nERROR: Incomplete Accelerator Deadlock detector status in " << path << std::endl;
      ifs.close();
      return;
    }
    ifs.close();

    return;
  }
#endif

  xclReadWrapper(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                  dbgIpInfo->m_base_address + 0x0,
                  &(accelDeadlockResults.DeadlockStatus), sizeof(uint32_t));
}


void 
DebugIpStatusCollector::populateOverview(boost::property_tree::ptree &_pt)
{
  debug_ip_layout* dbgIpLayout = getDebugIpLayout();
  if(nullptr == dbgIpLayout)
    return;

  uint64_t count = 0;
  for(uint64_t i = 0; i < dbgIpLayout->m_count; i++) {
    switch(dbgIpLayout->m_debug_ip_data[i].m_type) {
      case LAPC:
      case ILA:
      case AXI_MM_MONITOR:
      case AXI_MONITOR_FIFO_FULL:
      case ACCEL_MONITOR:
      case AXI_STREAM_MONITOR:
      case AXI_STREAM_PROTOCOL_CHECKER:
      case TRACE_S2MM:
      case ACCEL_DEADLOCK_DETECTOR:
        ++count;
        ++debugIpNum[dbgIpLayout->m_debug_ip_data[i].m_type];
        break;
      case UNDEFINED:
      case AXI_TRACE_FUNNEL:
      case AXI_MONITOR_FIFO_LITE:
      case AXI_DMA:
      case TRACE_S2MM_FULL:
      case AXI_NOC:
        // No need to show these Debug IP types
        continue;
      default:
        std::cout << "Found invalid IP in debug ip layout with type "
                << dbgIpLayout->m_debug_ip_data[i].m_type << std::endl;
        return;
    }
  }

  _pt.put("total_num_debug_ips", count); // Total count with the IPs actually shown

  boost::property_tree::ptree dbg_ip_list_pt;
  for(uint8_t i = 0; i < DEBUG_IP_TYPE_MAX; i++) {
    if(0 == debugIpNum[i]) {
       continue;
    }
    boost::property_tree::ptree entry;
    entry.put("name", debugIpNames[i]);
    entry.put("count", debugIpNum[i]);

    dbg_ip_list_pt.push_back(std::make_pair("", entry));
  }
  _pt.add_child("debug_ips", dbg_ip_list_pt);
}


void 
DebugIpStatusCollector::populateAllResults(boost::property_tree::ptree &_pt)
{
  populateAIMResults(_pt);
  populateAMResults(_pt);
  populateASMResults(_pt);
  populateFIFOResults(_pt);
  populateTS2MMResults(_pt);
  populateLAPCResults(_pt);
  populateSPCResults(_pt);
  populateILAResults(_pt);
  populateAccelDeadlockResults(_pt);

}


void 
DebugIpStatusCollector::populateAIMResults(boost::property_tree::ptree &_pt)
{
  if(0 == aimResults.NumSlots) {
    return;
  }

  boost::property_tree::ptree aim_pt;

  for(size_t i = 0; i < aimResults.NumSlots; ++i) {
    boost::property_tree::ptree entry;

    entry.put("name", std::string(cuNames[AXI_MM_MONITOR][i] + "/" + portNames[AXI_MM_MONITOR][i]));
    entry.put("region_or_cu", cuNames[AXI_MM_MONITOR][i]);
    entry.put("type_or_port", portNames[AXI_MM_MONITOR][i]);
    entry.put("write_kBytes", boost::str(boost::format("%.3f") % (static_cast<double>(aimResults.WriteBytes[i])/1000.0)) );
    entry.put("write_trans",  aimResults.WriteTranx[i]);
    entry.put("read_kBytes",  boost::str(boost::format("%.3f") % (static_cast<double>(aimResults.ReadBytes[i])/1000.0)) );
    entry.put("read_tranx",   aimResults.ReadTranx[i]);
    entry.put("outstanding_count", aimResults.OutStandCnts[i]);
    entry.put("last_write_addr", boost::str(boost::format("0x%x") % aimResults.LastWriteAddr[i]) );
    entry.put("last_write_data", boost::str(boost::format("0x%x") % aimResults.LastWriteData[i]) );
    entry.put("last_read_addr", boost::str(boost::format("0x%x") % aimResults.LastReadAddr[i]) );
    entry.put("last_read_data", boost::str(boost::format("0x%x") % aimResults.LastReadData[i]) );

    aim_pt.push_back(std::make_pair("", entry));
  }

  _pt.add_child("axi_interface_monitor_counters", aim_pt); 
}

void 
DebugIpStatusCollector::populateAMResults(boost::property_tree::ptree &_pt)
{
  if(0 == amResults.NumSlots) {
    return;
  }

  boost::property_tree::ptree am_pt;

  for(size_t i = 0; i < amResults.NumSlots; ++i) {
    boost::property_tree::ptree entry;

    entry.put("name", cuNames[ACCEL_MONITOR][i]);
    entry.put("compute_unit", cuNames[ACCEL_MONITOR][i]);
    entry.put("ends", amResults.CuExecCount[i]);
    entry.put("starts", amResults.CuStartCount[i]);
    entry.put("max_parallel_itr", amResults.CuMaxParallelIter[i]);
    entry.put("execution", boost::str(boost::format("0x%x") % amResults.CuExecCycles[i]) );
    entry.put("memory_stall", boost::str(boost::format("0x%x") % amResults.CuStallExtCycles[i]) );
    entry.put("pipe_stall", boost::str(boost::format("0x%x") % amResults.CuStallIntCycles[i]) );
    entry.put("stream_stall", boost::str(boost::format("0x%x") % amResults.CuStallStrCycles[i]) );
    entry.put("min_exec", boost::str(boost::format("0x%x") % amResults.CuMinExecCycles[i]) );
    entry.put("max_exec", boost::str(boost::format("0x%x") % amResults.CuMaxExecCycles[i]) );

    am_pt.push_back(std::make_pair("", entry));
  }

  _pt.add_child("accelerator_monitor_counters", am_pt);
}

void 
DebugIpStatusCollector::populateASMResults(boost::property_tree::ptree &_pt)
{
  if(0 == asmResults.NumSlots) {
    return;
  }

  boost::property_tree::ptree asm_pt;

  for(size_t i = 0; i < asmResults.NumSlots; ++i) {
    boost::property_tree::ptree entry;

    entry.put("name", std::string(cuNames[AXI_STREAM_MONITOR][i] + "/" + portNames[AXI_STREAM_MONITOR][i]));
    entry.put("stream_master", cuNames[AXI_STREAM_MONITOR][i]);
    entry.put("stream_slave", portNames[AXI_STREAM_MONITOR][i]);
    entry.put("num_trans", asmResults.StrNumTranx[i]); 
    entry.put("data_kBytes", boost::str(boost::format("%.3f") % (static_cast<double>(asmResults.StrDataBytes[i])/1000.0)) );
    entry.put("busy_cycles", asmResults.StrBusyCycles[i]);
    entry.put("stall_cycles", asmResults.StrStallCycles[i]);
    entry.put("starve_cycles", asmResults.StrStarveCycles[i]);

    asm_pt.push_back(std::make_pair("", entry));
  }

  _pt.add_child("axi_stream_monitor_counters", asm_pt);
}

void 
DebugIpStatusCollector::populateFIFOResults(boost::property_tree::ptree &_pt)
{
  if(0 == debugIpNum[AXI_MONITOR_FIFO_FULL]) {
    return;
  }

  boost::property_tree::ptree fifo_pt;
  fifo_pt.put("description", "FIFO on PL that stores trace events from all monitors");
  fifo_pt.put("count",debugIpNum[AXI_MONITOR_FIFO_FULL]);

  _pt.add_child("Trace FIFO", fifo_pt);
}

void 
DebugIpStatusCollector::populateTS2MMResults(boost::property_tree::ptree &_pt)
{
  if(0 == debugIpNum[TRACE_S2MM]) {
    return;
  }

  boost::property_tree::ptree ts2mm_pt;
  ts2mm_pt.put("description", "Offloads trace events from all monitors to a memory resource (DDR, HBM, PLRAM)");
  ts2mm_pt.put("count",debugIpNum[TRACE_S2MM]);

  _pt.add_child("Trace Stream to Memory", ts2mm_pt);
}

void 
DebugIpStatusCollector::populateLAPCResults(boost::property_tree::ptree &_pt)
{
  if(0 == lapcResults.NumSlots) {
    return;
  }

  boost::property_tree::ptree lapc_pt;

  for(size_t i = 0; i < lapcResults.NumSlots; ++i) {
    boost::property_tree::ptree entry;

    entry.put("name", std::string(cuNames[LAPC][i] + "/" + portNames[LAPC][i]));
    entry.put("cu_name", cuNames[LAPC][i]);
    entry.put("axi_port", portNames[LAPC][i]);
    entry.put("overall_status", lapcResults.OverallStatus[i]);

    boost::property_tree::ptree snapshot_pt;
    for(size_t j = 0; j < XLAPC_STATUS_REG_NUM; j++) {
      boost::property_tree::ptree e_pt;
      e_pt.put_value(lapcResults.SnapshotStatus[i][j]);
      snapshot_pt.push_back(std::make_pair("", e_pt));
    }
    entry.add_child("snapshot_status", snapshot_pt); 

    boost::property_tree::ptree cumulative_pt;
    for(size_t j = 0; j < XLAPC_STATUS_REG_NUM; j++) {
      boost::property_tree::ptree e_pt;
      e_pt.put_value(lapcResults.CumulativeStatus[i][j]);
      cumulative_pt.push_back(std::make_pair("", e_pt));
    }
    entry.add_child("cumulative_status", cumulative_pt); 

    lapc_pt.push_back(std::make_pair("", entry));
  }

  _pt.add_child("light_weight_axi_protocol_checkers", lapc_pt);

}

void 
DebugIpStatusCollector::populateSPCResults(boost::property_tree::ptree &_pt)
{
  if(0 == spcResults.NumSlots) {
    return;
  }

  boost::property_tree::ptree spc_pt;

  for(size_t i = 0; i < spcResults.NumSlots; ++i) {
    boost::property_tree::ptree entry;

    entry.put("name", std::string(cuNames[AXI_STREAM_PROTOCOL_CHECKER][i] + "/" + portNames[AXI_STREAM_PROTOCOL_CHECKER][i]));
    entry.put("cu_name", cuNames[AXI_STREAM_PROTOCOL_CHECKER][i]);
    entry.put("axi_port", portNames[AXI_STREAM_PROTOCOL_CHECKER][i]);
    entry.put("pc_asserted", spcResults.PCAsserted[i]);
    entry.put("current_pc", spcResults.CurrentPC[i]);
    entry.put("snapshot_pc", spcResults.SnapshotPC[i]);

    spc_pt.push_back(std::make_pair("", entry));
  }

  _pt.add_child("axi_streaming_protocol_checkers", spc_pt);
}

void 
DebugIpStatusCollector::populateILAResults(boost::property_tree::ptree &_pt)
{
  if(0 == debugIpNum[ILA]) {
    return;
  }

  boost::property_tree::ptree ila_pt;
  ila_pt.put("description", "Enables debugging and performance monitoring of kernel running on hardware");
  ila_pt.put("count",debugIpNum[ILA]);

  _pt.add_child("Integrated Logic Analyzer", ila_pt);

}

void 
DebugIpStatusCollector::populateAccelDeadlockResults(boost::property_tree::ptree &_pt)
{
  if(0 == accelDeadlockResults.Num) {
    return;
  }

  // Only 1 Accelerator Deadlock Detector Ip per design
  boost::property_tree::ptree accel_deadlock_pt;

  accel_deadlock_pt.put("is_deadlocked", accelDeadlockResults.DeadlockStatus);

  _pt.add_child("accel_deadlock_detector_status", accel_deadlock_pt); 
}

// ----- Supporting Functions -------------------------------------------

void
reportOverview(std::ostream& _output, const boost::property_tree::ptree& _dbgIpStatus_pt)
{
  _output << "Debug IP Status \n  Number of IPs found :: " 
          << _dbgIpStatus_pt.get<uint64_t>("total_num_debug_ips") << std::endl; // Total count with the IPs actually shown

  try {
    const boost::property_tree::ptree& dbgIps_pt = _dbgIpStatus_pt.get_child("debug_ips");
    std::string header("IP Name (element filter option)");
    _output << "\n  IPs found :"
            << "\n    " << header << " : Count" << std::endl;

    boost::format valueFormat("    %-"+std::to_string(header.length())+"s : %llu");
    for(auto& ip : dbgIps_pt) {
      const boost::property_tree::ptree& entry = ip.second;
      _output << valueFormat % entry.get<std::string>("name") % entry.get<uint64_t>("count") << std::endl;
     }
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
  _output << std::endl;
}

void
reportAIM(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("axi_interface_monitor_counters");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for AIM enabled but currently loaded xclbin does not have any AIM. So, AIM status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  const boost::property_tree::ptree& aim_pt = child.get();

  _output << "\nAXI Interface Monitor Counters\n";

  const auto col1 = std::max(cuNameMaxStrLen[AXI_MM_MONITOR], strlen("Region or CU")) + 4;
  const auto col2 = std::max(portNameMaxStrLen[AXI_MM_MONITOR], strlen("Type or Port"));

  boost::format header("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s %-16s");
  _output << header % "Region or CU" % "Type or Port" %  "Write kBytes" % "Write Trans." % "Read kBytes" % "Read Tranx."
                    % "Outstanding Cnt" % "Last Wr Addr" % "Last Wr Data" % "Last Rd Addr" % "Last Rd Data"
          << std::endl;

  boost::format valueFormat("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16.3f  %-16llu  %-16.3f  %-16llu  %-16llu  %-16x  %-16x  %-16x %-16x");

  try {
    for(auto& ip : aim_pt) {
      const boost::property_tree::ptree& entry = ip.second;
      _output << valueFormat
                   % entry.get<std::string>("region_or_cu")     % entry.get<std::string>("type_or_port")
                   % entry.get<std::string>("write_kBytes")     % entry.get<uint64_t>("write_trans")
                   % entry.get<std::string>("read_kBytes")      % entry.get<uint64_t>("read_tranx")
                   % entry.get<uint64_t>("outstanding_count")
                   % entry.get<std::string>("last_write_addr")  % entry.get<std::string>("last_write_data")
                   % entry.get<std::string>("last_read_addr")   % entry.get<std::string>("last_read_data")
              << std::endl;
     }
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
  _output << std::endl;
}

void
reportAM(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("accelerator_monitor_counters");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << std::endl 
              << "INFO: Element filter for AM enabled but currently loaded xclbin does not have any AM. So, AM status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  const boost::property_tree::ptree& am_pt = child.get();

  _output << "\nAccelerator Monitor Counters (hex values are cycle count)\n";

  const auto col1 = std::max(cuNameMaxStrLen[ACCEL_MONITOR], strlen("Compute Unit")) + 4;

  boost::format header("  %-"+std::to_string(col1)+"s %-8s  %-8s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s");
  _output << header % "Compute Unit"  % "Ends" % "Starts" % "Max Parallel Itr" % "Execution" % "Memory Stall" % "Pipe Stall" % "Stream Stall" % "Min Exec" % "Max Exec"
          << std::endl;

  boost::format valueFormat("  %-"+std::to_string(col1)+"s %-8llu  %-8llu  %-16llu  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x");
  try {
    for(auto& ip : am_pt) {
      const boost::property_tree::ptree& entry = ip.second;
      _output << valueFormat
                   % entry.get<std::string>("compute_unit")
                   % entry.get<uint64_t>("ends") % entry.get<uint64_t>("starts") % entry.get<uint64_t>("max_parallel_itr")
                   % entry.get<std::string>("execution")
                   % entry.get<std::string>("memory_stall") % entry.get<std::string>("pipe_stall") % entry.get<std::string>("stream_stall")
                   % entry.get<std::string>("min_exec") % entry.get<std::string>("max_exec") 
              << std::endl;
    }
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
  _output << std::endl;
}

void
reportASM(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("axi_stream_monitor_counters");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for ASM enabled but currently loaded xclbin does not have any ASM. So, ASM status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  const boost::property_tree::ptree& asm_pt = child.get();

  _output << "\nAXI Stream Monitor Counters\n";

  const auto col1 = std::max(cuNameMaxStrLen[AXI_STREAM_MONITOR], strlen("Stream Master")) + 4;
  const auto col2 = std::max(portNameMaxStrLen[AXI_STREAM_MONITOR], strlen("Stream Slave"));

  boost::format header("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s  %-16s  %-16s");
  _output << header % "Stream Master" % "Stream Slave" % "Num Trans." % "Data kBytes" % "Busy Cycles" % "Stall Cycles" % "Starve Cycles"
          << std::endl;

  boost::format valueFormat("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16llu  %-16.3f  %-16llu  %-16llu  %-16llu");
  try {
    for(auto& ip : asm_pt) {
      const boost::property_tree::ptree& entry = ip.second;
      _output << valueFormat
                   % entry.get<std::string>("stream_master") % entry.get<std::string>("stream_slave")
                   % entry.get<uint64_t>("num_trans")
                   % entry.get<std::string>("data_kBytes")
                   % entry.get<uint64_t>("busy_cycles") % entry.get<uint64_t>("stall_cycles") % entry.get<uint64_t>("starve_cycles")
              << std::endl;
    }
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
  _output << std::endl;
}

void
reportFIFO(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("Trace FIFO");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for Trace FIFO enabled but currently loaded xclbin does not have any Trace FIFO. So, Trace FIFO status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  const boost::property_tree::ptree& fifo_pt = child.get();

  _output << "\nTrace FIFO" << std::endl
          << "  " << fifo_pt.get<std::string>("description") << std::endl
          << "  Found : " << fifo_pt.get<uint64_t>("count") << std::endl;
  return;
}

void
reportTS2MM(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("Trace Stream to Memory");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for TraceS2MM enabled but currently loaded xclbin does not have any TraceS2MM. So, TraceS2MM status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  const boost::property_tree::ptree& ts2mm_pt = child.get();

  _output << "\nTrace Stream to Memory" << std::endl
          << "  " << ts2mm_pt.get<std::string>("description") << std::endl
          << "  Found : " << ts2mm_pt.get<uint64_t>("count") << std::endl;
}

void
reportLAPC(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("light_weight_axi_protocol_checkers");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for LAPC enabled but currently loaded xclbin does not have any LAPC. So, LAPC status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  const boost::property_tree::ptree& lapc_pt = child.get();

  _output << "\nLight Weight AXI Protocol Checkers codes \n";

  const auto col1 = std::max(cuNameMaxStrLen[LAPC], strlen("CU Name")) + 4;
  const auto col2 = std::max(portNameMaxStrLen[LAPC], strlen("AXI Portname"));

  bool violations_found = false;
  bool invalid_codes = false;

  try {
    for(auto& ip : lapc_pt) {
      const boost::property_tree::ptree& entry = ip.second;
      unsigned int snapshotStatus[XLAPC_STATUS_REG_NUM]   = {0};
      unsigned int cumulativeStatus[XLAPC_STATUS_REG_NUM] = {0};

      const boost::property_tree::ptree& snapshot_pt = entry.get_child("snapshot_status");
      size_t idx = 0;
      for(auto& e : snapshot_pt) {
        snapshotStatus[idx] = e.second.get_value<unsigned int>();
        ++idx;
        if(idx >= XLAPC_STATUS_REG_NUM) {
          break;
        }  
      }

      const boost::property_tree::ptree& cumulative_pt = entry.get_child("cumulative_status");
      idx = 0;
      for(auto& e : cumulative_pt) {
        cumulativeStatus[idx] = e.second.get_value<unsigned int>();
        ++idx;
        if(idx >= XLAPC_STATUS_REG_NUM) {
          break;
        }  
      }

      if (!xclAXICheckerCodes::isValidAXICheckerCodes(entry.get<unsigned int>("overall_status"),
                snapshotStatus, cumulativeStatus)) {

        invalid_codes = true;
        _output << boost::format("CU Name: %s AXI Port: %s \n  Invalid codes read, skip decoding")
                    % entry.get<std::string>("cu_name") % entry.get<std::string>("axi_port")
                << std::endl;

      } else if(entry.get<unsigned int>("overall_status")) {

        violations_found = true;
        _output << boost::format("CU Name: %s AXI Port: %s \n  First violation: \n    %s")
                    % entry.get<std::string>("cu_name") % entry.get<std::string>("axi_port")
                    % xclAXICheckerCodes::decodeAXICheckerCodes(snapshotStatus) 
                << std::endl;

        // Snapshot reflects first violation, Cumulative has all violations
        unsigned int transformedStatus[XLAPC_STATUS_REG_NUM] = {0};
        std::transform(cumulativeStatus, cumulativeStatus+XLAPC_STATUS_REG_NUM /*past the last element*/,
                       snapshotStatus,
                       transformedStatus,
                       std::bit_xor<unsigned int>());
        std::string tStr = xclAXICheckerCodes::decodeAXICheckerCodes(transformedStatus);
        _output << boost::format("  Other violations: \n    %s") % (("" == tStr) ? "None" : tStr)
                << std::endl;
      }
    }
    if(!violations_found && !invalid_codes) {
      _output << "No AXI violations found" << std::endl;
    }

    if (violations_found && !invalid_codes) {
      boost::format header("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s");

      _output << header % "CU Name" % "AXI Portname" % "Overall Status" 
                        % "Snapshot[0]" % "Snapshot[1]" % "Snapshot[2]" % "Snapshot[3]"
                        % "Cumulative[0]" % "Cumulative[1]" % "Cumulative[2]" % "Cumulative[3]"
            << std::endl;

      boost::format valueFormat("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x");


      for(auto& ip : lapc_pt) {
        const boost::property_tree::ptree& entry = ip.second;
        unsigned int snapshotStatus[XLAPC_STATUS_REG_NUM]   = {0};
        unsigned int cumulativeStatus[XLAPC_STATUS_REG_NUM] = {0};
  
        const boost::property_tree::ptree& snapshot_pt = entry.get_child("snapshot_status");
        size_t idx = 0;
        for(auto& e : snapshot_pt) {
          snapshotStatus[idx] = e.second.get_value<unsigned int>(); 
          ++idx;
          if(idx >= XLAPC_STATUS_REG_NUM) {
            break;
          }  
        }

        const boost::property_tree::ptree& cumulative_pt = entry.get_child("cumulative_status");
        idx = 0;
        for(auto& e : cumulative_pt) {
          cumulativeStatus[idx] = e.second.get_value<unsigned int>(); 
          ++idx;  
          if(idx >= XLAPC_STATUS_REG_NUM) {
            break;
          }  
        }

        _output << valueFormat
                     % entry.get<std::string>("cu_name") % entry.get<std::string>("axi_port")
                     % entry.get<unsigned int>("overall_status")
                     % snapshotStatus[0]   % snapshotStatus[1]   % snapshotStatus[2]   % snapshotStatus[3]
                     % cumulativeStatus[0] % cumulativeStatus[1] % cumulativeStatus[2] % cumulativeStatus[3]
                << std::endl;
      }
    }
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
  _output << std::endl;
}

void
reportSPC(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("axi_streaming_protocol_checkers");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for SPC enabled but currently loaded xclbin does not have any SPC. So, SPC status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  const boost::property_tree::ptree& spc_pt = child.get();

  // Now print out all of the values (and their interpretations)
  _output << "\nAXI Streaming Protocol Checkers codes\n";

  bool invalid_codes = false ;
  bool violations_found = false ;

  try {
    for(auto& ip : spc_pt) {
      const boost::property_tree::ptree& entry = ip.second;
      _output << boost::format("CU Name: %s AXI Port: %s")
                  % entry.get<std::string>("cu_name") % entry.get<std::string>("axi_port")
              << std::endl;

      if(!xclStreamingAXICheckerCodes::isValidStreamingAXICheckerCodes(entry.get<unsigned int>("pc_asserted"),
                       entry.get<unsigned int>("current_pc"), entry.get<unsigned int>("snapshot_pc"))) {
        invalid_codes = true;
        _output << "  Invalid codes read, skip decoding" << std::endl;
      } else {
        violations_found = true;
        _output << boost::format("  First violation: \n    %s")
                    % xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(entry.get<unsigned int>("snapshot_pc"))
                << std::endl;

        std::string tStr = xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(entry.get<unsigned int>("current_pc"));
        _output << boost::format("  Other violations: \n    %s") % (("" == tStr) ? "None" : tStr)
                << std::endl;
      }

    }
    if (!violations_found && !invalid_codes) {
      _output << "No AXI violations found " << std::endl;
    }

    if (violations_found && /*aVerbose &&*/ !invalid_codes) {
      const auto col1 = std::max(cuNameMaxStrLen[AXI_STREAM_PROTOCOL_CHECKER], strlen("CU Name")) + 4;
      const auto col2 = std::max(portNameMaxStrLen[AXI_STREAM_PROTOCOL_CHECKER], strlen("AXI Portname"));

      boost::format header("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s");
      _output << "\n"
              << header % "CU Name" % "AXI Portname" % "Overall Status" % "Snapshot" % "Current"
              << std::endl;

      boost::format valueFormat("  %-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16x  %-16x  %-16x");
      for(auto& ip : spc_pt) {
        const boost::property_tree::ptree& entry = ip.second;
        _output << valueFormat
                     % entry.get<std::string>("cu_name") % entry.get<std::string>("axi_port")
                     % entry.get<unsigned int>("pc_asserted") 
                     % entry.get<unsigned int>("snapshot_pc") 
                     % entry.get<unsigned int>("current_pc")
                << std::endl;
      }
    }
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
  _output << std::endl;
}

void
reportILA(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("Integrated Logic Analyzer");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for ILA enabled but currently loaded xclbin does not have any ILA. So, ILA status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  try {
    const boost::property_tree::ptree& ila_pt = child.get();
    _output << boost::format("\nIntegrated Logic Analyzer\n  %s\n  Found : %llu") 
                % ila_pt.get<std::string>("description") % ila_pt.get<uint64_t>("count")
            << std::endl;
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
}

void
reportAccelDeadlock(std::ostream& _output, const boost::property_tree::ptree& _pt, bool _gen_not_found_info)
{
  boost::optional<const boost::property_tree::ptree&> child = _pt.get_child_optional("accel_deadlock_detector_status");
  if(boost::none == child) {
    if(true == _gen_not_found_info) {
      _output << "\nINFO: Element filter for Accelerator Deadlock Detector enabled but currently loaded xclbin does not have any Accelerator Deadlock Detector. So, Accelerator Deadlock Detector status report will NOT be generated." 
              << std::endl;
    }
    return;
  }
  try {
    const boost::property_tree::ptree& accel_deadlock_pt = child.get();
    _output << "\nAccelerator Deadlock Detector IP status :" 
            << ((0 == accel_deadlock_pt.get<uint64_t>("is_deadlocked")) ? " No " : " ")
            << "deadlock detected." << std::endl;
  }
  catch(std::exception const& e) {
    _output << "\nWARNING: " <<  e.what() << std::endl;
  }
}

void 
processElementFilter(bool *debugIpOpt, const std::vector<std::string> & _elementsFilter)
{
  // reset debugIpOpt to all "false" and then process given element filter
  std::fill(debugIpOpt, debugIpOpt + DEBUG_IP_TYPE_MAX, false);

  for(auto& itr : _elementsFilter) {
    if(itr == "aim") {
      debugIpOpt[AXI_MM_MONITOR] = true;
    } else if(itr == "am") {
      debugIpOpt[ACCEL_MONITOR] = true;
    } else if(itr == "asm") {
      debugIpOpt[AXI_STREAM_MONITOR] = true;
    } else if(itr == "lapc") {
      debugIpOpt[LAPC] = true;
    } else if(itr == "spc") {
      debugIpOpt[AXI_STREAM_PROTOCOL_CHECKER] = true;
    } else if(itr == "fifo") {
      debugIpOpt[AXI_MONITOR_FIFO_FULL] = true;
    } else if(itr == "ts2mm") {
      debugIpOpt[TRACE_S2MM] = true;
    } else if(itr == "ila") {
      debugIpOpt[ILA] = true;
    } else if(itr == "accel_deadlock_detector") {
      debugIpOpt[ACCEL_DEADLOCK_DETECTOR] = true;
    }
  }
}

};

// ----- ReportDebugIpStatus C L A S S   M E T H O D S -------------------------------------------


void 
ReportDebugIpStatus::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                         boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportDebugIpStatus::getPropertyTree20202( const xrt_core::device * _pDevice,
                                      boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  pt.put("description","Status of Debug IPs present in xclbin loaded on device");
  auto handle = _pDevice->get_device_handle();

  DebugIpStatusCollector collector(handle, true);
  if (collector.getInfoMessage() != "") {
    pt.put("info", collector.getInfoMessage().c_str()) ;
  } else {
    collector.populateOverview(pt);
    collector.collect();
    collector.populateAllResults(pt);
  }

  // There can only be 1 root node
  _pt.add_child("debug_ip_status", pt);
  
}


void 
ReportDebugIpStatus::writeReport( const xrt_core::device* /*_pDevice*/,
                                  const boost::property_tree::ptree& _pt, 
                                  const std::vector<std::string>& _elementsFilter,
                                  std::ostream & _output) const
{

  const boost::property_tree::ptree& dbgIpStatus_pt = _pt.get_child("debug_ip_status");

  //Print Overview
  reportOverview(_output, dbgIpStatus_pt);

  // Process Element Filter
  bool debugIpOpt[DEBUG_IP_TYPE_MAX];
  // By default, enable status collection for all Debug IP types
  std::fill(debugIpOpt, debugIpOpt + DEBUG_IP_TYPE_MAX, true);

  bool filter = false;
  if(_elementsFilter.size()) {
    filter = true;
    processElementFilter(debugIpOpt, _elementsFilter);
  }

  // Results
  if (true == debugIpOpt[AXI_MM_MONITOR]) {
    reportAIM(  _output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[ACCEL_MONITOR]) {
    reportAM(   _output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[AXI_STREAM_MONITOR]) {
    reportASM(  _output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[AXI_MONITOR_FIFO_FULL]) {
    reportFIFO( _output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[TRACE_S2MM]) {
    reportTS2MM(_output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[LAPC]) {
    reportLAPC( _output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[AXI_STREAM_PROTOCOL_CHECKER]) {
    reportSPC( _output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[ILA]) {
    reportILA( _output, dbgIpStatus_pt, filter);
  }
  if (true == debugIpOpt[ACCEL_DEADLOCK_DETECTOR]) {
    reportAccelDeadlock( _output, dbgIpStatus_pt, filter);
  }

  return;
}


