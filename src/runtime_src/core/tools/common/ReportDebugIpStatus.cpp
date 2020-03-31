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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportDebugIpStatus.h"

#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/include/xrt.h"

#include "core/common/utils.h"
#include "core/common/error.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xcl_perfmon_parameters.h"
#include "core/include/xcl_axi_checker_codes.h"

// 3rd Party Library - Include Files
//#include <boost/program_options.hpp>
//#include <boost/format.hpp>
//namespace po = boost::program_options;

// System - Include Files
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>


// ----- C L A S S   M E T H O D S -------------------------------------------

const int8_t maxDebugIpType = TRACE_S2MM_FULL+1;

//bool debugIpOpt[maxDebugIpType];
//uint64_t debugIpNum[maxDebugIpType];
static const char* debugIpNames[maxDebugIpType] = {
  "unknown",
  "LAPC",
  "ILA",
  "AIM",
  "TraceFunnel",
  "TraceFifoLite",
  "TraceFifoFull",
  "AM",
  "ASM",
  "AxiStreamProtocolChecker",
  "TS2MM",
  "AxiDMA",
  "TS2MMFull"
};


class DebugIpStatusCollector
{
  debug_ip_layout* map = nullptr;

  uint64_t debugIpNum[maxDebugIpType] = {0};

  size_t   cuNameMaxStrLen[maxDebugIpType]   = {0};
  size_t   portNameMaxStrLen[maxDebugIpType] = {0};

  std::vector<std::string> cuNames[maxDebugIpType];
  std::vector<std::string> portNames[maxDebugIpType];

//  std::vector< std::vector<std::string> > cuNames;
//  std::vector< std::vector<std::string> > portNames;

  xclDebugCountersResults          aimResults;
  xclStreamingDebugCountersResults asmResults;
  xclAccelMonitorCounterResults    amResults;
  xclDebugCheckersResults          lapcResults;
  xclDebugStreamingCheckersResults spcResults;

  xclDeviceHandle handle;

public :
  DebugIpStatusCollector(xclDeviceHandle h, std::ostream& _output);

  ~DebugIpStatusCollector();

  void printOverview(std::ostream& _output);
  void collect(std::ostream& _output);

private :

  void initializeResults();

  void getDebugIpData();
  void getCuNamePortName(uint8_t type, std::string& dbgIpName, std::string& cuName, std::string& portName); 
  void getStreamName(uint8_t type, std::string& dbgIpName, std::string& masterName, std::string& slaveName); 

  void readAIMCounter(debug_ip_data*);
  void readAMCounter(debug_ip_data*);
  void readASMCounter(debug_ip_data*);
  void readLAPChecker(debug_ip_data*);
  void readSPChecker(debug_ip_data*);

  void printAIMResults(std::ostream&);
  void printAMResults(std::ostream&);
  void printASMResults(std::ostream&);
  void printLAPCResults(std::ostream&);
  void printSPCResults(std::ostream&);
};

DebugIpStatusCollector::DebugIpStatusCollector(xclDeviceHandle h, std::ostream& _output)
    : handle(h)
{
  initializeResults();

  size_t sz1 = 0, sectionSz = 0;
  // Get the size of full debug_ip_layout
  xclGetDebugIpLayout(handle, nullptr, sz1, &sectionSz);
  // Allocate buffer to retrieve debug_ip_layout information from loaded xclbin
  map = reinterpret_cast<debug_ip_layout*>(new char[sectionSz]);
  xclGetDebugIpLayout(handle, ((char*)map), sectionSz, &sz1);

  if (sectionSz == 0 || map->m_count <= 0) {
    _output << "INFO: Failed to find any debug IPs on the platform. "
            << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n"
            << std::endl;
    return;
  }

}

DebugIpStatusCollector::~DebugIpStatusCollector()
{
  delete [] ((char*)map);
  map = nullptr; 
}

void 
DebugIpStatusCollector::initializeResults()
{
  std::memset(&aimResults, 0, sizeof(aimResults));
  std::memset(&amResults,  0, sizeof(amResults));
  std::memset(&asmResults, 0, sizeof(asmResults));

  std::memset(&lapcResults, 0, sizeof(lapcResults));
  std::memset(&spcResults, 0, sizeof(spcResults));
}

void 
DebugIpStatusCollector::printOverview(std::ostream& _output)
{
  if(!map) {
    _output << " INFO: Debug IP Data not populated." << std::endl;
  }
  _output << "Number of IPs found :: " << map->m_count << std::endl;
  for(uint64_t i = 0; i < map->m_count; i++) {
    if (map->m_debug_ip_data[i].m_type > maxDebugIpType) {
      _output << "Found invalid IP in debug ip layout with type "
              << map->m_debug_ip_data[i].m_type << std::endl;
      return;
    }
    ++debugIpNum[map->m_debug_ip_data[i].m_type];
  }

  std::stringstream sstr;
  for(uint8_t i = 0; i < maxDebugIpType; i++) {
    if(0 == debugIpNum[i]) {
       continue;
    }
    sstr << debugIpNames[i] << "(" << debugIpNum[i] << ")  ";
  }

  _output << "IPs found [<ipname>(<count>)]: " << sstr.str() << std::endl;
}

void 
DebugIpStatusCollector::collect(std::ostream& _output)
{
  // reset all info
  // debugIpNum
  // results
  getDebugIpData();

  // update the numslots in results
  printAIMResults(_output);
  printAMResults(_output);
  printASMResults(_output);
  printLAPCResults(_output);
  printSPCResults(_output);
}

void 
DebugIpStatusCollector::getDebugIpData()
{
  if(!map) {
    std::cout << " INFO: Debug IP Data not populated." << std::endl;
  }
  // reset to zero
  std::memset((char*)debugIpNum, 0, sizeof(debugIpNum));
std::cout << " after reset debugIpNum " << std::endl;
for(uint64_t i = 0 ; i < maxDebugIpType ; i++) {
  std::cout << " debugIpNum [" << i << "] = " << debugIpNum[i] << std::endl; 
}
  for(uint64_t i = 0; i < map->m_count; i++) {
    std::cout << " reading count " << i << std::endl;
    switch(map->m_debug_ip_data[i].m_type)
    {
      case AXI_MM_MONITOR     : readAIMCounter(&(map->m_debug_ip_data[i]));
                                break;
      case ACCEL_MONITOR      : readAMCounter(&(map->m_debug_ip_data[i]));
                                break;
      case AXI_STREAM_MONITOR : readASMCounter(&(map->m_debug_ip_data[i]));
                                break;
      case LAPC               : readLAPChecker(&(map->m_debug_ip_data[i]));
                                break;
      case AXI_STREAM_PROTOCOL_CHECKER : readSPChecker(&(map->m_debug_ip_data[i]));
                                break;
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
  //Slotnames are of the format "/cuname/portname" or "cuname/portname", split them and return in separate vector
  //return max length of the cuname and port names
  char sep = '/';

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
  // Use strlen() instead of length() because the strings taken from debug_ip_layout
  // are always 128 in length, where the end is full of null characters  
  cuNameMaxStrLen[dbgIpType]   = std::max(strlen(cuName.c_str()), cuNameMaxStrLen[dbgIpType]);
  portNameMaxStrLen[dbgIpType] = std::max(strlen(portName.c_str()), portNameMaxStrLen[dbgIpType]);

  std::cout << " FOUND : dbgIpType " << dbgIpType << " cuName " << cuName << " portName " << portName << std::endl;
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
  uint64_t index = debugIpNum[AXI_MM_MONITOR];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, sizeof(dbgIpInfo->m_name));
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  std::string cuName;
  std::string portName;
  getCuNamePortName(dbgIpInfo->m_type, dbgIpName, cuName, portName);
  cuNames[AXI_MM_MONITOR].emplace_back(cuName);
  portNames[AXI_MM_MONITOR].emplace_back(portName);

  std::cout << " AXI_MM_MONITOR : index " << index << " name " << dbgIpName << std::endl;

  // read counter values
  size_t size = 0;

  uint64_t aim_offsets[] = {
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

  uint64_t aim_upper_offsets[] = {
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
  size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                    dbgIpInfo->m_base_address + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, sizeof(uint32_t));

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbgIpInfo->m_properties & XAIM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
      xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                 dbgIpInfo->m_base_address + aim_upper_offsets[c], &currData[c], sizeof(uint32_t)) ;
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
    size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, 
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

  (void)size;

  // increment debugIpNum
  ++debugIpNum[AXI_MM_MONITOR];
  aimResults.NumSlots = (unsigned int)debugIpNum[AXI_MM_MONITOR];
}

void 
DebugIpStatusCollector::printAIMResults(std::ostream& _output)
{  
  if(0 == aimResults.NumSlots) {
    return;
  }

  _output << "AXI Interface Monitor Counters\n";
  auto col1 = std::max(cuNameMaxStrLen[AXI_MM_MONITOR], strlen("Region or CU")) + 4;
  auto col2 = std::max(portNameMaxStrLen[AXI_MM_MONITOR], strlen("Type or Port"));

  std::streamsize ss = _output.precision();
  auto iosguard = xrt_core::utils::ios_restore(_output);
  _output << std::left
            << std::setw(col1) << "Region or CU"
            << " " << std::setw(col2) << "Type or Port"
            << "  " << std::setw(16)  << "Write kBytes"
            << "  " << std::setw(16)  << "Write Trans."
            << "  " << std::setw(16)  << "Read kBytes"
            << "  " << std::setw(16)  << "Read Tranx."
            << "  " << std::setw(16)  << "Outstanding Cnt"
            << "  " << std::setw(16)  << "Last Wr Addr"
            << "  " << std::setw(16)  << "Last Wr Data"
            << "  " << std::setw(16)  << "Last Rd Addr"
            << "  " << std::setw(16)  << "Last Rd Data"
            << std::endl;
  for (size_t i = 0; i< aimResults.NumSlots; ++i) {
    // NOTE: column 2 only aligns if we use c_str() instead of the string
    _output << std::left
              << std::setw(col1) << cuNames[AXI_MM_MONITOR][i]
              << " " << std::setw(col2) << portNames[AXI_MM_MONITOR][i]

              << "  " << std::setw(16) << std::fixed << std::setprecision(3)
              << static_cast<double>(aimResults.WriteBytes[i]) / 1000.0
              << std::scientific << std::setprecision(ss)

              << "  " << std::setw(16) << aimResults.WriteTranx[i]

              << "  " << std::setw(16) << std::fixed << std::setprecision(3)
              << static_cast<double>(aimResults.ReadBytes[i]) / 1000.0
              << std::scientific << std::setprecision(ss)

              << "  " << std::setw(16) << aimResults.ReadTranx[i]
              << "  " << std::setw(16) << aimResults.OutStandCnts[i]
              << std::hex
              << "  " << "0x" << std::setw(14) << aimResults.LastWriteAddr[i]
              << "  " << "0x" << std::setw(14) << aimResults.LastWriteData[i]
              << "  " << "0x" << std::setw(14) << aimResults.LastReadAddr[i]
              << "  " << "0x" << std::setw(14) << aimResults.LastReadData[i]
              << std::dec << std::endl;
  }
  return;

}


void 
DebugIpStatusCollector::readAMCounter(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  uint64_t index = debugIpNum[ACCEL_MONITOR];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, 128);
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  std::string cuName = dbgIpName;
  cuNames[ACCEL_MONITOR].emplace_back(cuName);
  portNames[ACCEL_MONITOR].emplace_back("Unknown");
  cuNameMaxStrLen[ACCEL_MONITOR] = std::max(strlen(cuName.c_str()), cuNameMaxStrLen[ACCEL_MONITOR]);
  
  size_t size = 0;

  uint64_t am_offsets[] = {
    XAM_ACCEL_EXECUTION_COUNT_OFFSET,
    XAM_ACCEL_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_STALL_INT_OFFSET,
    XAM_ACCEL_STALL_STR_OFFSET,
    XAM_ACCEL_STALL_EXT_OFFSET,
    XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET,
    XAM_ACCEL_TOTAL_CU_START_OFFSET
  };

  uint64_t am_upper_offsets[] = {
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
  size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
                  dbgIpInfo->m_base_address + XAM_SAMPLE_OFFSET,
                  &sampleInterval, 4);

  auto dbgIpVersion = std::make_pair(dbgIpInfo->m_major, dbgIpInfo->m_minor);
  auto refVersion   = std::make_pair((uint8_t)1, (uint8_t)1);

  bool hasDataflow = (dbgIpVersion > refVersion) ? true : false;

  // If applicable, read the upper 32-bits of the 64-bit debug counters
  if (dbgIpInfo->m_properties & XAM_64BIT_PROPERTY_MASK) {
    for (int c = 0 ; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
      xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
            dbgIpInfo->m_base_address + am_upper_offsets[c],
            &currData[c], 4) ;
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
      xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_BUSY_CYCLES_UPPER_OFFSET, &dfTmp[0], 4);
      xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET, &dfTmp[1], 4);

      amResults.CuBusyCycles[index]      = dfTmp[0] << 32;
      amResults.CuMaxParallelIter[index] = dfTmp[1] << 32;
    }
  }

  for (int c=0; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++)
    size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address+am_offsets[c], &currData[c], 4);

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
    xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_BUSY_CYCLES_OFFSET, &dfTmp[0], 4);
    xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON, dbgIpInfo->m_base_address + XAM_MAX_PARALLEL_ITER_OFFSET, &dfTmp[1], 4);

    amResults.CuBusyCycles[index]      |= dfTmp[0] << 32;
    amResults.CuMaxParallelIter[index] |= dfTmp[1] << 32;
  } else {
    amResults.CuBusyCycles[index]      = amResults.CuExecCycles[index];
    amResults.CuMaxParallelIter[index] = 1;
  }

  (void)size;

  // increment debugIpNum
  ++debugIpNum[ACCEL_MONITOR];
  amResults.NumSlots = (unsigned int)debugIpNum[ACCEL_MONITOR];
  std::cout << " Finish Read ACCEL_MONITOR index " << index << " now debugIpName[ACCEL_MONITOR] " << debugIpNum[ACCEL_MONITOR] << std::endl;
}

void 
DebugIpStatusCollector::printAMResults(std::ostream& _output)
{
  if(0 == amResults.NumSlots) {
    return;
  }

  _output << "Accelerator Monitor Counters (hex values are cycle count)\n";

  auto col1 = std::max(cuNameMaxStrLen[ACCEL_MONITOR], strlen("Compute Unit")) + 4;

  auto iosguard = xrt_core::utils::ios_restore(_output);
  _output << std::left
          << std::setw(col1) << "Compute Unit"
          << " " << std::setw(8) << "Ends"
          << "  " << std::setw(8)  << "Starts"
          << "  " << std::setw(16)  << "Max Parallel Itr"
          << "  " << std::setw(16)  << "Execution"
          << "  " << std::setw(16)  << "Memory Stall"
          << "  " << std::setw(16)  << "Pipe Stall"
          << "  " << std::setw(16)  << "Stream Stall"
          << "  " << std::setw(16)  << "Min Exec"
          << "  " << std::setw(16)  << "Max Exec"
          << std::endl;
  for (size_t i = 0; i < amResults.NumSlots; ++i) {
    _output << std::left
            << std::setw(col1) << cuNames[ACCEL_MONITOR][i]
            << " " << std::setw(8) << amResults.CuExecCount[i]
            << "  " << std::setw(8) << amResults.CuStartCount[i]
            << "  " << std::setw(16) << amResults.CuMaxParallelIter[i]
            << std::hex
            << "  " << "0x" << std::setw(14) << amResults.CuExecCycles[i]
            << "  " << "0x" << std::setw(14) << amResults.CuStallExtCycles[i]
            << "  " << "0x" << std::setw(14) << amResults.CuStallIntCycles[i]
            << "  " << "0x" << std::setw(14) << amResults.CuStallStrCycles[i]
            << "  " << "0x" << std::setw(14) << amResults.CuMinExecCycles[i]
            << "  " << "0x" << std::setw(14) << amResults.CuMaxExecCycles[i]
            << std::dec << std::endl;
  }
std::cout << " Finish PRINT ACCEL_MONITOR " << " now num " << amResults.NumSlots << std::endl;
}

void 
DebugIpStatusCollector::readASMCounter(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  uint64_t index = debugIpNum[AXI_STREAM_MONITOR];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, 128);
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Master and Slave Port Name
  std::string masterName;
  std::string slaveName;
  getStreamName(dbgIpInfo->m_type, dbgIpName, masterName, slaveName);
  cuNames[AXI_STREAM_MONITOR].emplace_back(masterName);
  portNames[AXI_STREAM_MONITOR].emplace_back(slaveName);

  std::cout << " AXI_STREAM_MONITOR : index " << index << " name " << dbgIpName << std::endl;



  size_t size = 0; // The amount of data read from the hardware

  // Fill up the portions of the return struct that are known by the runtime

  // Fill up the return structure with the values read from the hardware
  uint64_t asm_offsets[] = {
    XASM_NUM_TRANX_OFFSET,
    XASM_DATA_BYTES_OFFSET,
    XASM_BUSY_CYCLES_OFFSET,
    XASM_STALL_CYCLES_OFFSET,
    XASM_STARVE_CYCLES_OFFSET
  };

  uint32_t sampleInterval ;
  // Read sample interval register to latch the sampled metric counters
  size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
             dbgIpInfo->m_base_address + XASM_SAMPLE_OFFSET,
             &sampleInterval, sizeof(uint32_t));

  // Then read all the individual 64-bit counters
  unsigned long long int currData[XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] ;

  for (unsigned int j = 0 ; j < XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; ++j) {
    size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_PERFMON,
               dbgIpInfo->m_base_address + asm_offsets[j],
               &currData[j], sizeof(unsigned long long int));
  }
  asmResults.StrNumTranx[index] = currData[0] ;
  asmResults.StrDataBytes[index] = currData[1] ;
  asmResults.StrBusyCycles[index] = currData[2] ;
  asmResults.StrStallCycles[index] = currData[3] ;
  asmResults.StrStarveCycles[index] = currData[4] ;

  (void)size;

  // increment debugIpNum
  ++debugIpNum[AXI_STREAM_MONITOR];
  asmResults.NumSlots = (unsigned int)debugIpNum[AXI_STREAM_MONITOR];
  
  std::cout << " Finish Read AXI_STREAM_MONITOR index " << index << " now debugIpName[AXI_STREAM_MONITOR] " << debugIpNum[AXI_STREAM_MONITOR] << std::endl;
}

void 
DebugIpStatusCollector::printASMResults(std::ostream& _output)
{
  if(0 == asmResults.NumSlots) {
    return;
  }
  _output << "AXI Stream Monitor Counters\n";
  auto col1 = std::max(cuNameMaxStrLen[AXI_STREAM_MONITOR], strlen("Stream Master")) + 4;
  auto col2 = std::max(portNameMaxStrLen[AXI_STREAM_MONITOR], strlen("Stream Slave"));

  std::streamsize ss = _output.precision();
  auto iosguard = xrt_core::utils::ios_restore(_output);
  _output << std::left
          << std::setw(col1) << "Stream Master"
          << " " << std::setw(col2) << "Stream Slave"
          << "  " << std::setw(16)  << "Num Trans."
          << "  " << std::setw(16)  << "Data kBytes"
          << "  " << std::setw(16)  << "Busy Cycles"
          << "  " << std::setw(16)  << "Stall Cycles"
          << "  " << std::setw(16)  << "Starve Cycles"
          << std::endl;
  for (size_t i = 0; i < asmResults.NumSlots; ++i) {
    _output << std::left
            << std::setw(col1) << cuNames[AXI_STREAM_MONITOR][i]
            << " " << std::setw(col2) << portNames[AXI_STREAM_MONITOR][i]
            << "  " << std::setw(16) << asmResults.StrNumTranx[i]

            << "  " << std::setw(16)  << std::fixed << std::setprecision(3)
            << static_cast<double>(asmResults.StrDataBytes[i]) / 1000.0
            << std::scientific << std::setprecision(ss)

            << "  " << std::setw(16) << asmResults.StrBusyCycles[i]
            << "  " << std::setw(16) << asmResults.StrStallCycles[i]
            << "  " << std::setw(16) << asmResults.StrStarveCycles[i]
            << std::endl;
  }
  
}

void 
DebugIpStatusCollector::readLAPChecker(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  uint64_t index = debugIpNum[LAPC];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, 128);
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  std::string cuName;
  std::string portName;
  getCuNamePortName(dbgIpInfo->m_type, dbgIpName, cuName, portName);
  cuNames[LAPC].emplace_back(cuName);
  portNames[LAPC].emplace_back(portName);


  size_t size = 0;

  uint64_t statusRegisters[] = {
    LAPC_OVERALL_STATUS_OFFSET,

    LAPC_CUMULATIVE_STATUS_0_OFFSET, LAPC_CUMULATIVE_STATUS_1_OFFSET,
    LAPC_CUMULATIVE_STATUS_2_OFFSET, LAPC_CUMULATIVE_STATUS_3_OFFSET,

    LAPC_SNAPSHOT_STATUS_0_OFFSET, LAPC_SNAPSHOT_STATUS_1_OFFSET,
    LAPC_SNAPSHOT_STATUS_2_OFFSET, LAPC_SNAPSHOT_STATUS_3_OFFSET
  };

  uint32_t currData[XLAPC_STATUS_PER_SLOT];
  
  for (int c=0; c < XLAPC_STATUS_PER_SLOT; c++)
    size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_CHECKER, dbgIpInfo->m_base_address+statusRegisters[c], &currData[c], 4);

  lapcResults.OverallStatus[index]      = currData[XLAPC_OVERALL_STATUS];
  std::copy(currData+XLAPC_CUMULATIVE_STATUS_0, currData+XLAPC_SNAPSHOT_STATUS_0, lapcResults.CumulativeStatus[index]);
  std::copy(currData+XLAPC_SNAPSHOT_STATUS_0, currData+XLAPC_STATUS_PER_SLOT, lapcResults.SnapshotStatus[index]);

  (void)size;

  // increment debugIpNum
  ++debugIpNum[LAPC];
  lapcResults.NumSlots = (unsigned int)debugIpNum[LAPC];
}

void 
DebugIpStatusCollector::printLAPCResults(std::ostream& _output)
{ 
  if(0 == lapcResults.NumSlots) {
    return;
  }

  bool violations_found = false;
  bool invalid_codes = false;
  _output << "Light Weight AXI Protocol Checkers codes \n";
  auto col1 = std::max(cuNameMaxStrLen[LAPC], strlen("CU Name")) + 4;
  auto col2 = std::max(portNameMaxStrLen[LAPC], strlen("AXI Portname"));

  for (size_t i = 0; i < lapcResults.NumSlots; ++i) {
    if (!xclAXICheckerCodes::isValidAXICheckerCodes(lapcResults.OverallStatus[i],
                lapcResults.SnapshotStatus[i], lapcResults.CumulativeStatus[i])) {
      _output << "CU Name: " << cuNames[LAPC][i] << " AXI Port: " << portNames[LAPC][i] << "\n";
      _output << "  Invalid codes read, skip decoding\n";
      invalid_codes = true;
    } else if (lapcResults.OverallStatus[i]) {
      _output << "CU Name: " << cuNames[LAPC][i] << " AXI Port: " << portNames[LAPC][i] << "\n";
      _output << "  First violation: \n";
      _output << "    " <<  xclAXICheckerCodes::decodeAXICheckerCodes(lapcResults.SnapshotStatus[i]);
      //snapshot reflects first violation, cumulative has all violations
      unsigned int tCummStatus[4];
      std::transform(lapcResults.CumulativeStatus[i], lapcResults.CumulativeStatus[i]+4, lapcResults.SnapshotStatus[i], tCummStatus, std::bit_xor<unsigned int>());
      _output << "  Other violations: \n";
      std::string tstr = xclAXICheckerCodes::decodeAXICheckerCodes(tCummStatus);
      if (tstr == "") {
        _output << "    None";
      } else {
        _output << "    " <<  tstr;
      }
      violations_found = true;
    }
  }
  if (!violations_found && !invalid_codes)
    _output << "No AXI violations found \n";

  if (violations_found && /*aVerbose &&*/ !invalid_codes) {
    std::ofstream saveFormat;
    saveFormat.copyfmt(_output);

    _output << "\n";
    _output << std::left
              << std::setw(col1) << "CU Name"
              << " " << std::setw(col2) << "AXI Portname"
              << "  " << std::setw(16) << "Overall Status"
              << "  " << std::setw(16) << "Snapshot[0]"
              << "  " << std::setw(16) << "Snapshot[1]"
              << "  " << std::setw(16) << "Snapshot[2]"
              << "  " << std::setw(16) << "Snapshot[3]"
              << "  " << std::setw(16) << "Cumulative[0]"
              << "  " << std::setw(16) << "Cumulative[1]"
              << "  " << std::setw(16) << "Cumulative[2]"
              << "  " << std::setw(16) << "Cumulative[3]"
              << std::endl;
    for (size_t i = 0; i < lapcResults.NumSlots; ++i) {
      _output << std::left
              << std::setw(col1) << cuNames[LAPC][i]
              << " " << std::setw(col2) << portNames[LAPC][i]
              << std::hex
              << "  " << std::setw(16) << lapcResults.OverallStatus[i]
              << "  " << std::setw(16) << lapcResults.SnapshotStatus[i][0]
              << "  " << std::setw(16) << lapcResults.SnapshotStatus[i][1]
              << "  " << std::setw(16) << lapcResults.SnapshotStatus[i][2]
              << "  " << std::setw(16) << lapcResults.SnapshotStatus[i][3]
              << "  " << std::setw(16) << lapcResults.CumulativeStatus[i][0]
              << "  " << std::setw(16) << lapcResults.CumulativeStatus[i][1]
              << "  " << std::setw(16) << lapcResults.CumulativeStatus[i][2]
              << "  " << std::setw(16) << lapcResults.CumulativeStatus[i][3]
              << std::dec << std::endl;
    }
    // Restore formatting
    _output.copyfmt(saveFormat);
  }
}

void 
DebugIpStatusCollector::readSPChecker(debug_ip_data* dbgIpInfo)
{
  // index in results is debugIpNum
  uint64_t index = debugIpNum[AXI_STREAM_PROTOCOL_CHECKER];

  // Get Debug Ip Name
  std::string dbgIpName;
  // Fill up string with 128 characters (padded with null characters)
  dbgIpName.assign(dbgIpInfo->m_name, 128);
  // Strip away any extraneous null characters
  dbgIpName.assign(dbgIpName.c_str());

  // Get Cu and Port Name
  std::string cuName;
  std::string portName;
  getCuNamePortName(dbgIpInfo->m_type, dbgIpName, cuName, portName);
  cuNames[AXI_STREAM_PROTOCOL_CHECKER].emplace_back(cuName);
  portNames[AXI_STREAM_PROTOCOL_CHECKER].emplace_back(portName);


  size_t size = 0; // The amount of data read from the hardware


  uint32_t pc_asserted ;
  uint32_t current_pc ;
  uint32_t snapshot_pc ;

  size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_CHECKER,
              dbgIpInfo->m_base_address + XSPC_PC_ASSERTED_OFFSET,
              &pc_asserted, sizeof(uint32_t));
  size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_CHECKER,
              dbgIpInfo->m_base_address + XSPC_CURRENT_PC_OFFSET,
              &current_pc, sizeof(uint32_t));
  size += xclRead(handle, XCL_ADDR_SPACE_DEVICE_CHECKER,
              dbgIpInfo->m_base_address + XSPC_SNAPSHOT_PC_OFFSET,
              &snapshot_pc, sizeof(uint32_t));

  spcResults.PCAsserted[index] = pc_asserted;
  spcResults.CurrentPC[index]  = current_pc;
  spcResults.SnapshotPC[index] = snapshot_pc;

  (void)size;

  // increment debugIpNum
  ++debugIpNum[AXI_STREAM_PROTOCOL_CHECKER];
  lapcResults.NumSlots = (unsigned int)debugIpNum[AXI_STREAM_PROTOCOL_CHECKER];
}

void 
DebugIpStatusCollector::printSPCResults(std::ostream& _output)
{
  if(0 == spcResults.NumSlots) {
    return;
  }

  // Now print out all of the values (and their interpretations)
  _output << "AXI Streaming Protocol Checkers codes\n";
  bool invalid_codes = false ;
  bool violations_found = false ;

  for (size_t i = 0 ; i < spcResults.NumSlots; ++i) {
    _output << "CU Name: " << cuNames[AXI_STREAM_PROTOCOL_CHECKER][i] 
            << " AXI Port: " << portNames[AXI_STREAM_PROTOCOL_CHECKER][i] << std::endl;

    if (!xclStreamingAXICheckerCodes::isValidStreamingAXICheckerCodes(
                     spcResults.PCAsserted[i], spcResults.CurrentPC[i], 
                     spcResults.SnapshotPC[i])) {
      _output << "  Invalid codes read, skip decoding\n";
      invalid_codes = true ;
    } else {
      _output << "  First violation: " << std::endl
              << "    " << xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(spcResults.SnapshotPC[i])
              << "  Other violations: " << std::endl;
      std::string tstr = xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(spcResults.CurrentPC[i]);
      if (tstr == "") {
        _output << "    None";
      } else {
        _output << "    " <<  tstr;
      }
      violations_found = true;
    }
  }
  if (!violations_found && !invalid_codes)
    _output << "No AXI violations found \n";

  if (violations_found && /*aVerbose &&*/ !invalid_codes) {
    auto col1 = std::max(cuNameMaxStrLen[AXI_STREAM_PROTOCOL_CHECKER], strlen("CU Name")) + 4;
    auto col2 = std::max(portNameMaxStrLen[AXI_STREAM_PROTOCOL_CHECKER], strlen("AXI Portname"));

    std::ofstream saveFormat;
    saveFormat.copyfmt(_output);

    _output << "\n";
    _output << std::left
        << std::setw(col1) << "CU Name"
        << " " << std::setw(col2) << "AXI Portname"
        << "  " << std::setw(16) << "Overall Status"
        << "  " << std::setw(16) << "Snapshot"
        << "  " << std::setw(16) << "Current"
        << std::endl;
    for (size_t i = 0; i < spcResults.NumSlots; ++i) {
      _output << std::left
                << std::setw(col1) << cuNames[AXI_STREAM_PROTOCOL_CHECKER][i] 
                << " " << std::setw(col2) << portNames[AXI_STREAM_PROTOCOL_CHECKER][i]
                << "  " << std::setw(16) << std::hex << spcResults.PCAsserted[i]
                << "  " << std::setw(16) << std::hex << spcResults.SnapshotPC[i]
                << "  " << std::setw(16) << std::hex << spcResults.CurrentPC[i]
                << std::dec << std::endl;
    }
    // Restore formatting
    _output.copyfmt(saveFormat);
  }

  
}



void 
ReportDebugIpStatus::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                         boost::property_tree::ptree &_pt) const
{
  // Defer to the 20201 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20201(_pDevice, _pt);
}

void 
ReportDebugIpStatus::getPropertyTree20201( const xrt_core::device * /*_pDevice*/,
                                      boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  pt.put("Description","Status of Debug IPs present in xclbin loaded on device");

  // There can only be 1 root node
  _pt.add_child("debug-ip-status", pt);

  // AIM
  //_pt.add_child("aim",pt);
}


void 
ReportDebugIpStatus::writeReport( const xrt_core::device * _pDevice,
                             const std::vector<std::string> & /*_elementsFilter*/, 
                             std::iostream & _output) const
{
  auto handle = _pDevice->get_device_handle();

  DebugIpStatusCollector collector(handle, _output);
  collector.printOverview(_output);
  collector.collect(_output);


  //_output << "Run 'xbutil status' with option --<ipname> to get more information about the IP" << std::endl;
  _output << "INFO: xbutil2 status succeeded.\n";
  return;
}


