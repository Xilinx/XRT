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

const uint32_t maxDebugIpType = TRACE_S2MM_FULL+1;

static const char* debugIpNames[maxDebugIpType] = {
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
  "TS2MMFull"
};


class DebugIpStatusCollector
{

  xclDeviceHandle handle;

  std::vector<char> map;

  uint64_t debugIpNum[maxDebugIpType];
  bool     debugIpOpt[maxDebugIpType];

  size_t   cuNameMaxStrLen[maxDebugIpType];
  size_t   portNameMaxStrLen[maxDebugIpType];

  std::vector<std::string> cuNames[maxDebugIpType];
  std::vector<std::string> portNames[maxDebugIpType];

  xclDebugCountersResults          aimResults;
  xclStreamingDebugCountersResults asmResults;
  xclAccelMonitorCounterResults    amResults;
  xclDebugCheckersResults          lapcResults;
  xclDebugStreamingCheckersResults spcResults;

public :
  DebugIpStatusCollector(xclDeviceHandle h);
  ~DebugIpStatusCollector() {}

  void collect();
  void collect(const std::vector<std::string> & _elementsFilter);

  void printOverview(std::ostream& _output);
  void printAllResults(std::ostream& _output);

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

  void printAIMResults(std::ostream&);
  void printAMResults(std::ostream&);
  void printASMResults(std::ostream&);
  void printFIFOResults(std::ostream&);
  void printTS2MMResults(std::ostream&);
  void printLAPCResults(std::ostream&);
  void printSPCResults(std::ostream&);
  void printILAResults(std::ostream&);

  void populateAIMResults(boost::property_tree::ptree &_pt);
  void populateAMResults(boost::property_tree::ptree &_pt);
  void populateASMResults(boost::property_tree::ptree &_pt);
  void populateFIFOResults(boost::property_tree::ptree &_pt);
  void populateTS2MMResults(boost::property_tree::ptree &_pt);
  void populateLAPCResults(boost::property_tree::ptree &_pt);
  void populateSPCResults(boost::property_tree::ptree &_pt);
  void populateILAResults(boost::property_tree::ptree &_pt);

  void processElementFilter(const std::vector<std::string> & _elementsFilter);
};



DebugIpStatusCollector::DebugIpStatusCollector(xclDeviceHandle h)
    : handle(h)
    , debugIpNum{0}
    , debugIpOpt{false}
    , cuNameMaxStrLen{0}
    , portNameMaxStrLen{0}
    , aimResults{0}
    , asmResults{0}
    , amResults{0}
    , lapcResults{0}
    , spcResults{0}
{
  // By default, enable status collection for all Debug IP types
  std::fill(debugIpOpt, debugIpOpt + maxDebugIpType, true);

#ifdef _WIN32
  size_t sz1 = 0, sectionSz = 0;
  // Get the size of full debug_ip_layout
  xclGetDebugIpLayout(handle, nullptr, sz1, &sectionSz);
  if(sectionSz == 0) {
    std::cout << "INFO: Failed to find any Debug IP Layout section in the bitstream loaded on device. "
              << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n"
              << std::endl;
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
    std::cout << "INFO: Failed to find path to Debug IP Layout. "
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
    std::cout << "INFO: Failed to find any Debug IP Layout section in the bitstream loaded on device. "
              << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n"
              << std::endl;
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
    std::cout << "INFO: Failed to find any Debug IPs in the bitstream loaded on device." << std::endl;
    return nullptr;
  }
  return dbgIpLayout;
}


void 
DebugIpStatusCollector::printOverview(std::ostream& _output)
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
        _output << "Found invalid IP in debug ip layout with type "
                << dbgIpLayout->m_debug_ip_data[i].m_type << std::endl;
        return;
    }
  }

  _output << "Number of IPs found :: " << count << std::endl; // Total count with the IPs actually shown

  std::stringstream sstr;
  for(uint32_t i = 0; i < maxDebugIpType; i++) {
    if(0 == debugIpNum[i]) {
       continue;
    }
    sstr << debugIpNames[i] << " : " << debugIpNum[i] << std::endl;
  }

  _output << "IPs found [<ipname <(element filter option)>> :<count>)]: " << std::endl << sstr.str() << std::endl;
}

void 
DebugIpStatusCollector::collect()
{ 
  getDebugIpData();
}

void 
DebugIpStatusCollector::collect(const std::vector<std::string> & _elementsFilter)
{
  if(_elementsFilter.size()) {
    processElementFilter(_elementsFilter);
  }
  collect();
}

void 
DebugIpStatusCollector::processElementFilter(const std::vector<std::string> & _elementsFilter)
{
  // reset debugIpOpt to all "false" and then process given element filter
  std::fill(debugIpOpt, debugIpOpt + maxDebugIpType, false);

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
    }
  }
}

void 
DebugIpStatusCollector::printAllResults(std::ostream& _output)
{
  printAIMResults(_output);
  printAMResults(_output);
  printASMResults(_output);
  printFIFOResults(_output);
  printTS2MMResults(_output);
  printLAPCResults(_output);
  printSPCResults(_output);
  printILAResults(_output);
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
      std::cout << "ERROR: Incomplete AIM counter data in " << path << std::endl;
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
DebugIpStatusCollector::printAIMResults(std::ostream& _output)
{  
  if(0 == aimResults.NumSlots) {
    return;
  }

  _output << "\nAXI Interface Monitor Counters\n";
  auto col1 = std::max(cuNameMaxStrLen[AXI_MM_MONITOR], strlen("Region or CU")) + 4;
  auto col2 = std::max(portNameMaxStrLen[AXI_MM_MONITOR], strlen("Type or Port"));

  boost::format header("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s %-16s");
  _output << header % "Region or CU" % "Type or Port" %  "Write kBytes" % "Write Trans." % "Read kBytes" % "Read Tranx."
                    % "Outstanding Cnt" % "Last Wr Addr" % "Last Wr Data" % "Last Rd Addr" % "Last Rd Data"
          << std::endl;

  boost::format valueFormat("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16.3f  %-16llu  %-16.3f  %-16llu  %-16llu  0x%-14x  0x%-14x  0x%-14x 0x%-14x");
  for (size_t i = 0; i< aimResults.NumSlots; ++i) {
    _output << valueFormat
                  % cuNames[AXI_MM_MONITOR][i] % portNames[AXI_MM_MONITOR][i]
                  % (static_cast<double>(aimResults.WriteBytes[i]) / 1000.0) % aimResults.WriteTranx[i]
                  % (static_cast<double>(aimResults.ReadBytes[i]) / 1000.0)  % aimResults.ReadTranx[i]
                  % aimResults.OutStandCnts[i]
                  % aimResults.LastWriteAddr[i] % aimResults.LastWriteData[i]
                  % aimResults.LastReadAddr[i]  % aimResults.LastReadData[i]
            << std::endl;
  }
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
      std::cout << "ERROR: Incomplete AM counter data in " << path << std::endl;
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
DebugIpStatusCollector::printAMResults(std::ostream& _output)
{
  if(0 == amResults.NumSlots) {
    return;
  }

  _output << "\nAccelerator Monitor Counters (hex values are cycle count)\n";

  auto col1 = std::max(cuNameMaxStrLen[ACCEL_MONITOR], strlen("Compute Unit")) + 4;

  boost::format header("%-"+std::to_string(col1)+"s %-8s  %-8s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s");
  _output << header % "Compute Unit"  % "Ends" % "Starts" % "Max Parallel Itr" % "Execution" % "Memory Stall" % "Pipe Stall" % "Stream Stall" % "Min Exec" % "Max Exec"
          << std::endl;

  boost::format valueFormat("%-"+std::to_string(col1)+"s %-8llu  %-8llu  %-16llu  0x%-14x  0x%-14x  0x%-14x  0x%-14x  0x%-14x  0x%-14x");
  for (size_t i = 0; i < amResults.NumSlots; ++i) {
    _output << valueFormat
                  % cuNames[ACCEL_MONITOR][i] 
                  % amResults.CuExecCount[i] % amResults.CuStartCount[i] % amResults.CuMaxParallelIter[i] 
                  % amResults.CuExecCycles[i] 
                  % amResults.CuStallExtCycles[i] % amResults.CuStallIntCycles[i] % amResults.CuStallStrCycles[i]
                  % amResults.CuMinExecCycles[i]  % amResults.CuMaxExecCycles[i]
            << std::endl;
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
      std::cout << "ERROR: Incomplete ASM counter data in " << path << std::endl;
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
DebugIpStatusCollector::printASMResults(std::ostream& _output)
{
  if(0 == asmResults.NumSlots) {
    return;
  }
  _output << "\nAXI Stream Monitor Counters\n";
  auto col1 = std::max(cuNameMaxStrLen[AXI_STREAM_MONITOR], strlen("Stream Master")) + 4;
  auto col2 = std::max(portNameMaxStrLen[AXI_STREAM_MONITOR], strlen("Stream Slave"));

  boost::format header("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s  %-16s  %-16s");
  _output << header % "Stream Master" % "Stream Slave" % "Num Trans." % "Data kBytes" % "Busy Cycles" % "Stall Cycles" % "Starve Cycles"
          << std::endl;

  boost::format valueFormat("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16llu  %-16.3f  %-16llu  %-16llu %-16llu");
  for (size_t i = 0; i < asmResults.NumSlots; ++i) {
    _output << valueFormat
                  % cuNames[AXI_STREAM_MONITOR][i] % portNames[AXI_STREAM_MONITOR][i]
                  % asmResults.StrNumTranx[i]
                  % (static_cast<double>(asmResults.StrDataBytes[i]) / 1000.0)
                  % asmResults.StrBusyCycles[i] % asmResults.StrStallCycles[i] % asmResults.StrStarveCycles[i]
            << std::endl;
  }
}

void 
DebugIpStatusCollector::printFIFOResults(std::ostream& _output)
{
  if(0 == debugIpNum[AXI_MONITOR_FIFO_FULL]) {
    return;
  }

  _output << "\nTrace FIFO" << std::endl
          << "FIFO on PL that stores trace events from all monitors" << std::endl
          << "Found : " << debugIpNum[AXI_MONITOR_FIFO_FULL] << std::endl;
  return;
}

void 
DebugIpStatusCollector::printTS2MMResults(std::ostream& _output)
{
  if(0 == debugIpNum[TRACE_S2MM]) {
    return;
  }

  _output << "\nTrace Stream to Memory" << std::endl
          << "Offloads trace events from all monitors to a memory resource (DDR, HBM, PLRAM)" << std::endl
          << "Found : " << debugIpNum[TRACE_S2MM] << std::endl;
  return;
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
      std::cout << "ERROR: Incomplete LAPC data in " << path << std::endl;
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
DebugIpStatusCollector::printLAPCResults(std::ostream& _output)
{ 
  if(0 == lapcResults.NumSlots) {
    return;
  }

  bool violations_found = false;
  bool invalid_codes = false;
  _output << "\nLight Weight AXI Protocol Checkers codes \n";
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
    boost::format header("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s  %-16s");
    _output << header % "CU Name" % "AXI Portname" % "Overall Status" % "Snapshot[0]" % "Snapshot[1]" % "Snapshot[2]" % "Snapshot[3]"
                      % "Cumulative[0]" % "Cumulative[1]" % "Cumulative[2]" % "Cumulative[3]"
            << std::endl;

    boost::format valueFormat("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x  %-16x");
    for (size_t i = 0; i < lapcResults.NumSlots; ++i) {
      _output << valueFormat
                    % cuNames[LAPC][i] % portNames[LAPC][i]
                    % lapcResults.OverallStatus[i]
                    % lapcResults.SnapshotStatus[i][0] 
                    % lapcResults.SnapshotStatus[i][1] 
                    % lapcResults.SnapshotStatus[i][2] 
                    % lapcResults.SnapshotStatus[i][3]
                    % lapcResults.CumulativeStatus[i][0] 
                    % lapcResults.CumulativeStatus[i][1] 
                    % lapcResults.CumulativeStatus[i][2] 
                    % lapcResults.CumulativeStatus[i][3] 
              << std::endl;
    }
  }
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
      std::cout << "ERROR: Incomplete SPC data in " << path << std::endl;
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
DebugIpStatusCollector::printSPCResults(std::ostream& _output)
{
  if(0 == spcResults.NumSlots) {
    return;
  }

  // Now print out all of the values (and their interpretations)
  _output << "\nAXI Streaming Protocol Checkers codes\n";
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

    boost::format header("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16s  %-16s  %-16s");
    _output << std::endl
            << header % "CU Name" % "AXI Portname" % "Overall Status" % "Snapshot" % "Current"
            << std::endl;

    boost::format valueFormat("%-"+std::to_string(col1)+"s %-"+std::to_string(col2)+"s  %-16x  %-16x  %-16x");
    for (size_t i = 0; i < spcResults.NumSlots; ++i) {
      _output << valueFormat
                    % cuNames[AXI_STREAM_PROTOCOL_CHECKER][i] % portNames[AXI_STREAM_PROTOCOL_CHECKER][i]
                    % spcResults.PCAsserted[i] % spcResults.SnapshotPC[i] % spcResults.CurrentPC[i]
              << std::endl;
    }
  }
}

void 
DebugIpStatusCollector::printILAResults(std::ostream& _output)
{
  if(0 == debugIpNum[ILA]) {
    return;
  }

  _output << "\nIntegrated Logic Analyzer" << std::endl
          << "Enables debugging and performance monitoring of kernel running on hardware"
          << "Found : " << debugIpNum[ILA] << std::endl;
  return;
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
  for(uint8_t i = 0; i < maxDebugIpType; i++) {
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

    // To Do : Handle violations

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

    // To Do : Handle violations

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


};

// ----- ReportDebugIpStatus C L A S S   M E T H O D S -------------------------------------------


void 
ReportDebugIpStatus::getPropertyTreeInternal( const xrt_core::device * _pDevice,
                                         boost::property_tree::ptree &_pt) const
{
  // Defer to the 20201 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20201(_pDevice, _pt);
}

void 
ReportDebugIpStatus::getPropertyTree20201( const xrt_core::device * _pDevice,
                                      boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  pt.put("description","Status of Debug IPs present in xclbin loaded on device");

  auto handle = _pDevice->get_device_handle();

  DebugIpStatusCollector collector(handle);
  collector.populateOverview(pt);
  collector.collect();
  collector.populateAllResults(pt);

  // There can only be 1 root node
  _pt.add_child("debug_ip_status", pt);
  
}


void 
ReportDebugIpStatus::writeReport( const xrt_core::device * _pDevice,
                             const std::vector<std::string> & _elementsFilter, 
                             std::iostream & _output) const
{
  auto handle = _pDevice->get_device_handle();

  DebugIpStatusCollector collector(handle);
  collector.printOverview(_output);
  collector.collect(_elementsFilter);
  collector.printAllResults(_output);

  return;
}


