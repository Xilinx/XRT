/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "SubCmdStatus.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/include/xrt.h"

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/xrt_profiling.h"
#include "core/include/xcl_axi_checker_codes.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>


// ----- C L A S S   M E T H O D S -------------------------------------------

const int8_t maxDebugIpType = TRACE_S2MM_FULL+1;

bool debugIpOpt[maxDebugIpType];
uint64_t debugIpNum[maxDebugIpType];
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


uint32_t 
getDebugIpData(debug_ip_layout* map, int type,
        std::vector<uint64_t> *baseAddress, std::vector<std::string> * portNames) 
{
  uint32_t count = 0;
  for(uint64_t i = 0; i < map->m_count; i++) {
    if(map->m_debug_ip_data[i].m_type != type)
      continue;

    if (baseAddress)
      baseAddress->push_back(map->m_debug_ip_data[i].m_base_address);
    if(portNames) {
      std::string portName;
      // Fill up string with 128 characters (padded with null characters)
      portName.assign(map->m_debug_ip_data[i].m_name, 128);
      // Strip away any extraneous null characters
      portName.assign(portName.c_str());
      portNames->push_back(portName);
    }
    ++count;
  }
  return count;
}


std::pair<size_t, size_t> 
getCUNamePortName (std::vector<std::string>& aSlotNames,
        std::vector< std::pair<std::string, std::string> >& aCUNamePortNames) 
{
  //Slotnames are of the format "/cuname/portname" or "cuname/portname", split them and return in separate vector
  //return max length of the cuname and port names
  size_t max1 = 0, max2 = 0;
  char sep = '/';
  for (auto &slotName: aSlotNames) {
    size_t start = 0;
    size_t found1 = slotName.find(sep, 0);
    if (found1 == 0) {
      //if the cuname starts with a '/'
      start = 1;
      found1 = slotName.find(sep, 1);
    }
    if (found1 != std::string::npos) {
      aCUNamePortNames.emplace_back(slotName.substr(start, found1-start), slotName.substr(found1+1));
    }
    else {
      aCUNamePortNames.emplace_back("Unknown", "Unknown");
    }
    //Replace the name of the host-AIM to something simple
    if (aCUNamePortNames.back().first.find("interconnect_host_aximm") != std::string::npos) {
      aCUNamePortNames.pop_back();
      aCUNamePortNames.emplace_back("XDMA", "N/A");
    }
    // Use strlen() instead of length() because the strings taken from debug_ip_layout
    // are always 128 in length, where the end is full of null characters
    max1 = std::max(strlen(aCUNamePortNames.back().first.c_str()), max1);
    max2 = std::max(strlen(aCUNamePortNames.back().second.c_str()), max2);
  }
  return std::pair<size_t, size_t>(max1, max2);
}

std::pair<size_t, size_t> 
getStreamName (const std::vector<std::string>& aSlotNames,
        std::vector< std::pair<std::string, std::string> >& aStreamNames) 
{
  //Slotnames are of the format "Master-Slave", split them and return in separate vector
  //return max length of the Master and Slave port names
  size_t max1 = 0, max2 = 0;
  for (auto &s: aSlotNames) {
    size_t found;
    found = s.find(IP_LAYOUT_SEP, 0);
    if (found != std::string::npos)
      aStreamNames.emplace_back(s.substr(0, found), s.substr(found+1));
    else
      aStreamNames.emplace_back("Unknown", "Unknown");
    max1 = std::max(aStreamNames.back().first.length(), max1);
    max2 = std::max(aStreamNames.back().second.length(), max2);
  }
  return std::pair<size_t, size_t>(max1, max2);
}


void 
readAIMCounters(xclDeviceHandle hdl, debug_ip_layout* map) 
{
  std::vector<std::string> slotNames;
  if(0 == getDebugIpData(map, AXI_MM_MONITOR, nullptr, &slotNames)) {
    std::cout << "ERROR: AXI Interface Monitor IP does not exist on the platform" << std::endl;
    return;
  }

  xclDebugCountersResults debugResults = {0};
  std::vector< std::pair<std::string, std::string> > cuNameportNames;
  std::pair<size_t, size_t> widths = getCUNamePortName(slotNames, cuNameportNames);
  xclDebugReadIPStatus(hdl, XCL_DEBUG_READ_TYPE_AIM, &debugResults);

  std::cout << "AXI Interface Monitor Counters\n";
  auto col1 = std::max(widths.first, strlen("Region or CU")) + 4;
  auto col2 = std::max(widths.second, strlen("Type or Port"));

  std::streamsize ss = std::cout.precision();
  std::ios_base::fmtflags f(std::cout.flags());
  std::cout << std::left
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
  for (size_t i = 0; i<debugResults.NumSlots; ++i) {
    // NOTE: column 2 only aligns if we use c_str() instead of the string
    std::cout << std::left
              << std::setw(col1) << cuNameportNames[i].first
              << " " << std::setw(col2) << cuNameportNames[i].second.c_str()

              << "  " << std::setw(16) << std::fixed << std::setprecision(3)
              << static_cast<double>(debugResults.WriteBytes[i]) / 1000.0
              << std::scientific << std::setprecision(ss)

              << "  " << std::setw(16) << debugResults.WriteTranx[i]

              << "  " << std::setw(16) << std::fixed << std::setprecision(3)
              << static_cast<double>(debugResults.ReadBytes[i]) / 1000.0
              << std::scientific << std::setprecision(ss)

              << "  " << std::setw(16) << debugResults.ReadTranx[i]
              << "  " << std::setw(16) << debugResults.OutStandCnts[i]
              << std::hex
              << "  " << "0x" << std::setw(14) << debugResults.LastWriteAddr[i]
              << "  " << "0x" << std::setw(14) << debugResults.LastWriteData[i]
              << "  " << "0x" << std::setw(14) << debugResults.LastReadAddr[i]
              << "  " << "0x" << std::setw(14) << debugResults.LastReadData[i]
              << std::dec << std::endl;
  }
  std::cout.flags(f);
  return;
}

void 
readAMCounters(xclDeviceHandle hdl, debug_ip_layout* map)
{
  std::vector<std::string> slotNames;
  if(0 == getDebugIpData(map, ACCEL_MONITOR, nullptr, &slotNames)) {
    std::cout << "ERROR: Accelerator Monitor IP does not exist on the platform" << std::endl;
    return;
  }

  xclAccelMonitorCounterResults debugResults = {0};
  xclDebugReadIPStatus(hdl, XCL_DEBUG_READ_TYPE_AM, &debugResults);

  std::cout << "Accelerator Monitor Counters (hex values are cycle count)\n";

  size_t maxWidth = 0;
  for (auto &mon : slotNames)
    maxWidth = std::max(strlen(mon.c_str()), maxWidth);

  auto col1 = std::max(maxWidth, strlen("Compute Unit")) + 4;

  std::ios_base::fmtflags f(std::cout.flags());
  std::cout << std::left
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
  for (size_t i = 0; i<debugResults.NumSlots; ++i) {
    std::cout << std::left
              << std::setw(col1) << slotNames[i]
              << " " << std::setw(8) << debugResults.CuExecCount[i]
              << "  " << std::setw(8) << debugResults.CuStartCount[i]
              << "  " << std::setw(16) << debugResults.CuMaxParallelIter[i]
              << std::hex
              << "  " << "0x" << std::setw(14) << debugResults.CuExecCycles[i]
              << "  " << "0x" << std::setw(14) << debugResults.CuStallExtCycles[i]
              << "  " << "0x" << std::setw(14) << debugResults.CuStallIntCycles[i]
              << "  " << "0x" << std::setw(14) << debugResults.CuStallStrCycles[i]
              << "  " << "0x" << std::setw(14) << debugResults.CuMinExecCycles[i]
              << "  " << "0x" << std::setw(14) << debugResults.CuMaxExecCycles[i]
              << std::dec << std::endl;
  }
  std::cout.flags(f);
  return;
}

void 
readASMCounters(xclDeviceHandle hdl, debug_ip_layout* map) 
{
  std::vector<std::string> slotNames;
  if(0 == getDebugIpData(map, AXI_STREAM_MONITOR, nullptr, &slotNames)) {
    std::cout << "ERROR: AXI Stream Monitor IP does not exist on the platform" << std::endl;
    return;
  }

  xclStreamingDebugCountersResults debugResults = {0};
  std::vector< std::pair<std::string, std::string> > cuNameportNames;
  std::pair<size_t, size_t> widths = getStreamName(slotNames, cuNameportNames);
  xclDebugReadIPStatus(hdl, XCL_DEBUG_READ_TYPE_ASM, &debugResults);

  std::cout << "AXI Stream Monitor Counters\n";
  auto col1 = std::max(widths.first, strlen("Stream Master")) + 4;
  auto col2 = std::max(widths.second, strlen("Stream Slave"));

  std::streamsize ss = std::cout.precision();
  std::ios_base::fmtflags f(std::cout.flags());
  std::cout << std::left
            << std::setw(col1) << "Stream Master"
            << " " << std::setw(col2) << "Stream Slave"
            << "  " << std::setw(16)  << "Num Trans."
            << "  " << std::setw(16)  << "Data kBytes"
            << "  " << std::setw(16)  << "Busy Cycles"
            << "  " << std::setw(16)  << "Stall Cycles"
            << "  " << std::setw(16)  << "Starve Cycles"
            << std::endl;
  for (size_t i = 0; i<debugResults.NumSlots; ++i) {
    std::cout << std::left
            << std::setw(col1) << cuNameportNames[i].first
            << " " << std::setw(col2) << cuNameportNames[i].second.c_str()
            << "  " << std::setw(16) << debugResults.StrNumTranx[i]

            << "  " << std::setw(16)  << std::fixed << std::setprecision(3)
            << static_cast<double>(debugResults.StrDataBytes[i]) / 1000.0
            << std::scientific << std::setprecision(ss)

            << "  " << std::setw(16) << debugResults.StrBusyCycles[i]
            << "  " << std::setw(16) << debugResults.StrStallCycles[i]
            << "  " << std::setw(16) << debugResults.StrStarveCycles[i]
            << std::endl;
  }
  std::cout.flags(f);
  return;
}


void 
readLAPCheckers(xclDeviceHandle hdl, debug_ip_layout* map, int aVerbose)
{
  std::vector<std::string> lapcSlotNames;  
  if (0 == getDebugIpData(map, LAPC, nullptr, &lapcSlotNames)) {
    std::cout << "ERROR: LAPC IP does not exist on the platform" << std::endl;
    return;
  }
  xclDebugCheckersResults debugResults = {0};
  std::vector< std::pair<std::string, std::string> > cuNameportNames;
  std::pair<size_t, size_t> widths = getCUNamePortName(lapcSlotNames, cuNameportNames);
  xclDebugReadIPStatus(hdl, XCL_DEBUG_READ_TYPE_LAPC, &debugResults);

  bool violations_found = false;
  bool invalid_codes = false;
  std::cout << "Light Weight AXI Protocol Checkers codes \n";
  auto col1 = std::max(widths.first, strlen("CU Name")) + 4;
  auto col2 = std::max(widths.second, strlen("AXI Portname"));

  for (size_t i = 0; i<debugResults.NumSlots; ++i) {
    if (!xclAXICheckerCodes::isValidAXICheckerCodes(debugResults.OverallStatus[i],
                debugResults.SnapshotStatus[i], debugResults.CumulativeStatus[i])) {
      std::cout << "CU Name: " << cuNameportNames[i].first << " AXI Port: " << cuNameportNames[i].second << "\n";
      std::cout << "  Invalid codes read, skip decoding\n";
      invalid_codes = true;
    } else if (debugResults.OverallStatus[i]) {
      std::cout << "CU Name: " << cuNameportNames[i].first << " AXI Port: " << cuNameportNames[i].second << "\n";
      std::cout << "  First violation: \n";
      std::cout << "    " <<  xclAXICheckerCodes::decodeAXICheckerCodes(debugResults.SnapshotStatus[i]);
      //snapshot reflects first violation, cumulative has all violations
      unsigned int tCummStatus[4];
      std::transform(debugResults.CumulativeStatus[i], debugResults.CumulativeStatus[i]+4, debugResults.SnapshotStatus[i], tCummStatus, std::bit_xor<unsigned int>());
      std::cout << "  Other violations: \n";
      std::string tstr = xclAXICheckerCodes::decodeAXICheckerCodes(tCummStatus);
      if (tstr == "") {
        std::cout << "    None";
      } else {
        std::cout << "    " <<  tstr;
      }
      violations_found = true;
    }
  }
  if (!violations_found && !invalid_codes)
    std::cout << "No AXI violations found \n";

  if (violations_found && aVerbose && !invalid_codes) {
    std::ofstream saveFormat;
    saveFormat.copyfmt(std::cout);

    std::cout << "\n";
    std::cout << std::left
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
    for (size_t i = 0; i<debugResults.NumSlots; ++i) {
      std::cout << std::left
              << std::setw(col1) << cuNameportNames[i].first
              << " " << std::setw(col2) << cuNameportNames[i].second
              << std::hex
              << "  " << std::setw(16) << debugResults.OverallStatus[i]
              << "  " << std::setw(16) << debugResults.SnapshotStatus[i][0]
              << "  " << std::setw(16) << debugResults.SnapshotStatus[i][1]
              << "  " << std::setw(16) << debugResults.SnapshotStatus[i][2]
              << "  " << std::setw(16) << debugResults.SnapshotStatus[i][3]
              << "  " << std::setw(16) << debugResults.CumulativeStatus[i][0]
              << "  " << std::setw(16) << debugResults.CumulativeStatus[i][1]
              << "  " << std::setw(16) << debugResults.CumulativeStatus[i][2]
              << "  " << std::setw(16) << debugResults.CumulativeStatus[i][3]
              << std::dec << std::endl;
    }
    // Restore formatting
    std::cout.copyfmt(saveFormat);
  }
  return;
}

void 
readStreamingCheckers(xclDeviceHandle hdl, debug_ip_layout* map, int aVerbose)
{
  std::vector<std::string> streamingCheckerSlotNames ;

  if (0 == getDebugIpData(map, AXI_STREAM_PROTOCOL_CHECKER,
                          nullptr, &streamingCheckerSlotNames)) {
    std::cout << "ERROR: AXI Streaming Protocol Checkers do not exist on the platform" << std::endl ;
    return;
  }

  std::vector< std::pair<std::string, std::string> > cuNameportNames;
  std::pair<size_t, size_t> widths = getCUNamePortName(streamingCheckerSlotNames, cuNameportNames);

  xclDebugStreamingCheckersResults debugResults = {0};
  xclDebugReadIPStatus(hdl, XCL_DEBUG_READ_TYPE_SPC, &debugResults);

  // Now print out all of the values (and their interpretations)

  std::cout << "AXI Streaming Protocol Checkers codes\n";
  bool invalid_codes = false ;
  bool violations_found = false ;

  for (size_t i = 0 ; i < debugResults.NumSlots; ++i) {
    std::cout << "CU Name: " << cuNameportNames[i].first 
              << " AXI Port: " << cuNameportNames[i].second << "\n";

    if (!xclStreamingAXICheckerCodes::isValidStreamingAXICheckerCodes(
                     debugResults.PCAsserted[i], debugResults.CurrentPC[i], 
                     debugResults.SnapshotPC[i])) {
      std::cout << "  Invalid codes read, skip decoding\n";
      invalid_codes = true ;
    } else {
      std::cout << "  First violation: \n";
      std::cout << "    " << xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(debugResults.SnapshotPC[i]);
      std::cout << "  Other violations: \n";
      std::string tstr = xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(debugResults.CurrentPC[i]);
      if (tstr == "") {
        std::cout << "    None";
      } else {
        std::cout << "    " <<  tstr;
      }
      violations_found = true;
    }
  }
  if (!violations_found && !invalid_codes)
    std::cout << "No AXI violations found \n";

  if (violations_found && aVerbose && !invalid_codes) {
    auto col1 = std::max(widths.first, strlen("CU Name")) + 4;
    auto col2 = std::max(widths.second, strlen("AXI Portname"));

    std::ofstream saveFormat;
    saveFormat.copyfmt(std::cout);

    std::cout << "\n";
    std::cout << std::left
        << std::setw(col1) << "CU Name"
        << " " << std::setw(col2) << "AXI Portname"
        << "  " << std::setw(16) << "Overall Status"
        << "  " << std::setw(16) << "Snapshot"
        << "  " << std::setw(16) << "Current"
        << std::endl;
    for (size_t i = 0; i<debugResults.NumSlots; ++i) {
      std::cout << std::left
                << std::setw(col1) << cuNameportNames[i].first
                << " " << std::setw(col2) << cuNameportNames[i].second
                << "  " << std::setw(16) << std::hex << debugResults.PCAsserted[i]
                << "  " << std::setw(16) << std::hex << debugResults.SnapshotPC[i]
                << "  " << std::setw(16) << std::hex << debugResults.CurrentPC[i]
                << std::dec << std::endl;
    }
    // Restore formatting
    std::cout.copyfmt(saveFormat);
  }
  return;
}


SubCmdStatus::SubCmdStatus(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("status", 
             "List the debug IPs available on the acceleration program loaded on the given device")
{
  const std::string longDescription = "List the debug IPs available on the acceleration program loaded on the given device and show their status";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdStatus::execute(const SubCmdOptions& _options) const
// Reference Command:  [-d card] [--debug_ip_name]
//                     Get status of AIM monitor IP on the xclbin loaded on card 1
//                     xbutil status -d 1 --aim
{
  XBU::verbose("SubCommand: status");
  // -- Retrieve and parse the subcommand options -----------------------------
  unsigned int card = 0;
  bool help = false;
  po::options_description statusDesc("status options");

  statusDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<unsigned int>(&card), "Card to be examined")
    ("aim", boost::program_options::bool_switch(&debugIpOpt[AXI_MM_MONITOR]), "Status of AXI Interface Monitor")
    ("accelmonitor", boost::program_options::bool_switch(&debugIpOpt[ACCEL_MONITOR]), "Status of Accelerator Monitor")
    ("asm", boost::program_options::bool_switch(&debugIpOpt[AXI_STREAM_MONITOR]), "Status of AXI Stream Monitor")
    ("lapc", boost::program_options::bool_switch(&debugIpOpt[LAPC]), "Status of Light Weight AXI Protocol Checkers")
    ("spc", boost::program_options::bool_switch(&debugIpOpt[AXI_STREAM_PROTOCOL_CHECKER]), "Status of AXI Streaming Protocol Checkers")
//    (",tracefunnel", boost::program_options::value<unsigned int>(&debugIpOpt[AXI_TRACE_FUNNEL]), "")
//    (",monitorfifolite", boost::program_options::value<unsigned int>(&debugIpOpt[AXI_MONITOR_FIFO_LITE]), "")
//    (",monitorfifofull", boost::program_options::value<unsigned int>(&debugIpOpt[AXI_MONITOR_FIFO_FULL]), "")
  
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(statusDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    xrt_core::send_exception_message(e.what(), "XBUTIL");
    printHelp(statusDesc);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(statusDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Card: %ld") % card));

  auto device = xrt_core::get_userpf_device(card);
  auto hdl = device->get_device_handle();

  size_t sz1 = 0, sectionSz = 0;
  // Get the size of full debug_ip_layout
  xclGetDebugIpLayout(hdl, nullptr, sz1, &sectionSz);
  // Allocate buffer to retrieve debug_ip_layout information from loaded xclbin
  std::vector<char> buffer(sectionSz);
  xclGetDebugIpLayout(hdl, buffer.data(), sectionSz, &sz1);
  
  auto map = reinterpret_cast<debug_ip_layout*>(buffer.data());

  if (sectionSz == 0 || map->m_count <= 0) {
    std::cout << "INFO: Failed to find any debug IPs on the platform. "
              << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n"
              << std::endl;
    return;
  }

  bool ipOpt = false;
  for(uint8_t i = 0; i < maxDebugIpType; i++) {
    if(debugIpOpt[i]) {
      ipOpt = true;
      break;
    }
  }

  if(!ipOpt) {
    std::cout << "Number of IPs found :: " << map->m_count << std::endl;
    for(uint64_t i = 0; i < map->m_count; i++) {
      if (map->m_debug_ip_data[i].m_type > maxDebugIpType) {
        std::cout << "Found invalid IP in debug ip layout with type "
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

    std::cout << "IPs found [<ipname>(<count>)]: " << sstr.str() << std::endl;
    std::cout << "Run 'xbutil status' with option --<ipname> to get more information about the IP" << std::endl;
    std::cout << "INFO: xbutil2 status succeeded.\n";
    return;
  }

  if(debugIpOpt[AXI_MM_MONITOR]) {
    readAIMCounters(hdl, map);
  }
  if(debugIpOpt[ACCEL_MONITOR]) {
    readAMCounters(hdl, map);
  }
  if(debugIpOpt[AXI_STREAM_MONITOR]) {
    readASMCounters(hdl, map);
  }
  if(debugIpOpt[LAPC]) {
    readLAPCheckers(hdl, map, 1);
  }
  if(debugIpOpt[AXI_STREAM_PROTOCOL_CHECKER]) {
    readStreamingCheckers(hdl, map, 1);
  }

  std::cout << "INFO: xbutil2 status succeeded.\n";
  return;
}

