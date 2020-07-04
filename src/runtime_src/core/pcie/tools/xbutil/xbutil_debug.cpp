/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 * Author(s): Hem C Neema
 * PCIe HAL Driver layered on top of XOCL GEM kernel driver
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

#include <errno.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <sys/mman.h>
#include <cstring>
#include <unistd.h>
#include <sys/file.h>

#include "xclbin.h"
#include "xclperf.h"
#include "core/pcie/linux/scan.h"
#include "xbutil.h"

static const int debug_ip_layout_max_size = 65536;
static const int debug_ip_max_type = 9;

uint32_t xcldev::device::getIPCountAddrNames(int type,
    std::vector<uint64_t> *baseAddress, std::vector<std::string> * portNames) {
    std::string errmsg;
    std::vector<char> buf;
    pcidev::get_dev(m_idx)->sysfs_get("icap", "debug_ip_layout", errmsg, buf);

    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }

    debug_ip_layout *map = (debug_ip_layout *)buf.data();
    if (buf.empty() || map->m_count == 0) {
        std::cout <<  "ERROR: Failed to open debug IP layout file. "
            << "Ensure that a valid xclbin is successfully downloaded. \n";
        return -EINVAL;
    }

    uint32_t count = 0;
    for(unsigned int i = 0; i < map->m_count; i++) {
        if (map->m_debug_ip_data[i].m_type == type) {
            if (baseAddress)
                baseAddress->push_back(map->m_debug_ip_data[i].m_base_address);
            if(portNames) {
                std::string portName;
		// This makes sure that the c string is null terminated,
		//  but it also copies a full 128 and fills unused spaces
		//  with null characters.
                portName.assign(map->m_debug_ip_data[i].m_name, 128);
		// This statement strips away any extraneous null characters
		portName.assign(portName.c_str());
                portNames->push_back(portName);
            }
            ++count;
        }
    }

    return count;
}

std::pair<size_t, size_t> xcldev::device::getCUNamePortName (std::vector<std::string>& aSlotNames,
    std::vector< std::pair<std::string, std::string> >& aCUNamePortNames) {
    //Slotnames are of the format "/cuname/portname" or "cuname/portname", split them and return in separate vector
    //return max length of the cuname and port names
    size_t max1 = 0, max2 = 0;
    char sep = '/';
    for (auto slotName: aSlotNames) {
        size_t found1;
        size_t start = 0;
        found1 = slotName.find(sep, 0);
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

std::pair<size_t, size_t> xcldev::device::getStreamName (const std::vector<std::string>& aSlotNames,
    std::vector< std::pair<std::string, std::string> >& aStreamNames) {
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

int xcldev::device::readAIMCounters() {
    xclDebugCountersResults debugResults = {0};

    std::vector<uint64_t> baseAddress;
    std::vector<std::string> slotNames;
    std::vector< std::pair<std::string, std::string> > cuNameportNames;
    uint32_t numSlots = getIPCountAddrNames (AXI_MM_MONITOR, &baseAddress, &slotNames);

    if(-EINVAL == static_cast<int32_t>(numSlots)) {
      return 0;  // error msg already printed to std::cout
    }

    if (numSlots == 0) {
        std::cout << "ERROR: AXI Interface Monitor IP does not exist on the platform" << std::endl;
        return 0;
    }
    std::pair<size_t, size_t> widths = getCUNamePortName(slotNames, cuNameportNames);
//    xclDebugReadIPStatus(m_handle, XCL_DEBUG_READ_TYPE_AIM, &debugResults);

    for(uint32_t i = 0 ; i < numSlots; i++) {
      std::string aimName("aximm_mon_");
      aimName = aimName + std::to_string(baseAddress[i]);
      std::string namePath = pcidev::get_dev(m_idx)->get_sysfs_path(aimName.c_str(), "name");

      std::size_t pos = namePath.find_last_of('/');
      std::string path = namePath.substr(0, pos+1);
      path += "counters";

      std::ifstream ifs(path.c_str());
      if(!ifs) {
        continue;
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
        continue;
      }

      debugResults.WriteBytes[i]      = valBuf[0];
      debugResults.WriteTranx[i]      = valBuf[1];
      debugResults.ReadBytes[i]       = valBuf[4];
      debugResults.ReadTranx[i]       = valBuf[5];
      debugResults.OutStandCnts[i]    = valBuf[8];
      debugResults.LastWriteAddr[i]   = valBuf[9];
      debugResults.LastWriteData[i]   = valBuf[10];
      debugResults.LastReadAddr[i]    = valBuf[11];
      debugResults.LastReadData[i]    = valBuf[12];

      ifs.close();
    }
    debugResults.NumSlots = numSlots;

    std::cout << "AXI Interface Monitor Counters\n";
    int col1 = std::max(widths.first, strlen("Region or CU")) + 4;
    int col2 = std::max(widths.second, strlen("Type or Port"));

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
    return 0;
}

int xcldev::device::readAMCounters() {
    xclAccelMonitorCounterResults debugResults = {0};
    std::vector<uint64_t> baseAddress;
    std::vector<std::string> slotNames;

    uint32_t numSlots = getIPCountAddrNames (ACCEL_MONITOR, &baseAddress, &slotNames);

    if(-EINVAL == static_cast<int32_t>(numSlots)) {
      return 0;  // error msg already printed to std::cout
    }
    if (numSlots == 0) {
        std::cout << "ERROR: Accelerator Monitor IP does not exist on the platform" << std::endl;
        return 0;
    }
//    xclDebugReadIPStatus(m_handle, XCL_DEBUG_READ_TYPE_AM, &debugResults);

    for(uint32_t i = 0 ; i < numSlots; i++) {
      std::string amName("accel_mon_");
      amName = amName + std::to_string(baseAddress[i]);
      std::string namePath = pcidev::get_dev(m_idx)->get_sysfs_path(amName.c_str(), "name");

      std::size_t pos = namePath.find_last_of('/');
      std::string path = namePath.substr(0, pos+1);
      path += "counters";

      std::ifstream ifs(path.c_str());
      if(!ifs) {
        continue;
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
        continue;
      }

      debugResults.CuExecCount[i]        = valBuf[0];
      debugResults.CuStartCount[i]       = valBuf[1];
      debugResults.CuExecCycles[i]       = valBuf[2];

      debugResults.CuStallIntCycles[i]   = valBuf[3];
      debugResults.CuStallStrCycles[i]   = valBuf[4];
      debugResults.CuStallExtCycles[i]   = valBuf[5];

      debugResults.CuBusyCycles[i]       = valBuf[6];
      debugResults.CuMaxParallelIter[i]  = valBuf[7];
      debugResults.CuMaxExecCycles[i]    = valBuf[8];
      debugResults.CuMinExecCycles[i]    = valBuf[9];

      ifs.close();
    }
    debugResults.NumSlots = numSlots;

    std::cout << "Accelerator Monitor Counters (hex values are cycle count)\n";

    size_t maxWidth = 0;
    for (auto& mon : slotNames) {
        maxWidth = std::max(strlen(mon.c_str()), maxWidth);
    }
    int col1 = std::max(maxWidth, strlen("Compute Unit")) + 4;

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
    return 0;
}

int xcldev::device::readASMCounters() {
    xclStreamingDebugCountersResults debugResults = {0};

    std::vector<uint64_t> baseAddress;
    std::vector<std::string> slotNames;
    std::vector< std::pair<std::string, std::string> > cuNameportNames;
    uint32_t numSlots = getIPCountAddrNames (AXI_STREAM_MONITOR, &baseAddress, &slotNames);

    if(-EINVAL == static_cast<int32_t>(numSlots)) {
      return 0;  // error msg already printed to std::cout
    }
    if (numSlots == 0) {
        std::cout << "ERROR: AXI Stream Monitor IP does not exist on the platform" << std::endl;
        return 0;
    }
    std::pair<size_t, size_t> widths = getStreamName(slotNames, cuNameportNames);
//    xclDebugReadIPStatus(m_handle, XCL_DEBUG_READ_TYPE_ASM, &debugResults);


    for(uint32_t i = 0 ; i < numSlots; i++) {
      std::string asmName("axistream_mon_");
      asmName = asmName + std::to_string(baseAddress[i]);
      std::string namePath = pcidev::get_dev(m_idx)->get_sysfs_path(asmName.c_str(), "name");

      std::size_t pos = namePath.find_last_of('/');
      std::string path = namePath.substr(0, pos+1);
      path += "counters";

      std::ifstream ifs(path.c_str());
      if(!ifs) {
        continue;
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
        continue;
      }

      debugResults.StrNumTranx[i]     = valBuf[0];
      debugResults.StrDataBytes[i]    = valBuf[1];
      debugResults.StrBusyCycles[i]   = valBuf[2];
      debugResults.StrStallCycles[i]  = valBuf[3];
      debugResults.StrStarveCycles[i] = valBuf[4];

      ifs.close();
    }
    debugResults.NumSlots = numSlots;


    std::cout << "AXI Stream Monitor Counters\n";
    int col1 = std::max(widths.first, strlen("Stream Master")) + 4;
    int col2 = std::max(widths.second, strlen("Stream Slave"));

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
    return 0;
}

int xcldev::device::readLAPCheckers(int aVerbose) {
    xclDebugCheckersResults debugResults = {0};
    //if (getuid() && geteuid()) {
    //    std::cout << "ERROR: Reading LAPC requires root privileges" << std::endl;
    //    return -EACCES;
    //}
    std::vector<uint64_t> baseAddress;
    std::vector<std::string> lapcSlotNames;
    std::vector< std::pair<std::string, std::string> > cuNameportNames;
    uint32_t numSlots = getIPCountAddrNames (LAPC, &baseAddress, &lapcSlotNames);

    if(-EINVAL == static_cast<int32_t>(numSlots)) {
      return 0;  // error msg already printed to std::cout
    }
    if (numSlots == 0) {
        std::cout << "ERROR: LAPC IP does not exist on the platform" << std::endl;
        return 0;
    }
    std::pair<size_t, size_t> widths = getCUNamePortName(lapcSlotNames, cuNameportNames);
//    xclDebugReadIPStatus(m_handle, XCL_DEBUG_READ_TYPE_LAPC, &debugResults);

    for(uint32_t i = 0 ; i < numSlots; i++) {
      std::string lapcName("lapc_");
      lapcName = lapcName + std::to_string(baseAddress[i]);
      std::string namePath = pcidev::get_dev(m_idx)->get_sysfs_path(lapcName.c_str(), "name");

      std::size_t pos = namePath.find_last_of('/');
      std::string path = namePath.substr(0, pos+1);
      path += "status";

      std::ifstream ifs(path.c_str());
      if(!ifs) {
        continue;
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
        continue;
      }

      debugResults.OverallStatus[i]       = valBuf[0];

      debugResults.CumulativeStatus[i][0] = valBuf[1];
      debugResults.CumulativeStatus[i][1] = valBuf[2];
      debugResults.CumulativeStatus[i][2] = valBuf[3];
      debugResults.CumulativeStatus[i][3] = valBuf[4];

      debugResults.SnapshotStatus[i][0]   = valBuf[5];
      debugResults.SnapshotStatus[i][1]   = valBuf[6];
      debugResults.SnapshotStatus[i][2]   = valBuf[7];
      debugResults.SnapshotStatus[i][3]   = valBuf[8];

      ifs.close();
    }
    debugResults.NumSlots = numSlots;

    bool violations_found = false;
    bool invalid_codes = false;
    std::cout << "Light Weight AXI Protocol Checkers codes \n";
    int col1 = std::max(widths.first, strlen("CU Name")) + 4;
    int col2 = std::max(widths.second, strlen("AXI Portname"));

    for (size_t i = 0; i<debugResults.NumSlots; ++i) {
        if (!xclAXICheckerCodes::isValidAXICheckerCodes(debugResults.OverallStatus[i],
                        debugResults.SnapshotStatus[i], debugResults.CumulativeStatus[i])) {
            std::cout << "CU Name: " << cuNameportNames[i].first << " AXI Port: " << cuNameportNames[i].second << "\n";
            std::cout << "  Invalid codes read, skip decoding\n";
            invalid_codes = true;
        }
        else if (debugResults.OverallStatus[i]) {
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
            }
            else {
              std::cout << "    " <<  tstr;
            }
            violations_found = true;
        }
    }
    if (!violations_found && !invalid_codes) {
        std::cout << "No AXI violations found \n";
    }
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
    return 0;
}

int xcldev::device::readStreamingCheckers(int aVerbose) {

  std::vector<uint64_t> baseAddress;
  std::vector<std::string> streamingCheckerSlotNames ;
  uint32_t numCheckers = getIPCountAddrNames(AXI_STREAM_PROTOCOL_CHECKER,
						 &baseAddress,
						 &streamingCheckerSlotNames);

  if(-EINVAL == static_cast<int32_t>(numCheckers)) {
    return 0;  // error msg already printed to std::cout
  }
  if (numCheckers == 0) {
    std::cout << "ERROR: AXI Streaming Protocol Checkers do not exist on the platform" << std::endl ;
    return 0 ;
  }

  std::vector< std::pair<std::string, std::string> > cuNameportNames;

  std::pair<size_t, size_t> widths = getCUNamePortName(streamingCheckerSlotNames, cuNameportNames);

  xclDebugStreamingCheckersResults debugResults = {0};
//  xclDebugReadIPStatus(m_handle, XCL_DEBUG_READ_TYPE_SPC, &debugResults);

  for(uint32_t i = 0 ; i < numCheckers; i++) {
    std::string spcName("spc_");
    spcName = spcName + std::to_string(baseAddress[i]);
    std::string namePath = pcidev::get_dev(m_idx)->get_sysfs_path(spcName.c_str(), "name");

    std::size_t pos = namePath.find_last_of('/');
    std::string path = namePath.substr(0, pos+1);
    path += "status";

    std::ifstream ifs(path.c_str());
    if(!ifs) {
      continue;
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
      continue;
    }

    debugResults.PCAsserted[i] = valBuf[0];
    debugResults.CurrentPC[i]  = valBuf[1];
    debugResults.SnapshotPC[i] = valBuf[2];

    ifs.close();
  }
  debugResults.NumSlots = numCheckers;

  // Now print out all of the values (and their interpretations)

  std::cout << "AXI Streaming Protocol Checkers codes\n";
  bool invalid_codes = false ;
  bool violations_found = false ;

  for (size_t i = 0 ; i < debugResults.NumSlots; ++i) {
    std::cout << "CU Name: " << cuNameportNames[i].first 
	      << " AXI Port: " << cuNameportNames[i].second << "\n";

    if (!xclStreamingAXICheckerCodes::isValidStreamingAXICheckerCodes(debugResults.PCAsserted[i], debugResults.CurrentPC[i], debugResults.SnapshotPC[i]))
    {
      std::cout << "  Invalid codes read, skip decoding\n";
      invalid_codes = true ;
    }
    else
    {
      std::cout << "  First violation: \n";
      std::cout << "    " << xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(debugResults.SnapshotPC[i]);
      std::cout << "  Other violations: \n";
      std::string tstr = xclStreamingAXICheckerCodes::decodeStreamingAXICheckerCodes(debugResults.CurrentPC[i]);
      if (tstr == "") 
      {
	std::cout << "    None";
      }
      else 
      {
	std::cout << "    " <<  tstr;
      }
      violations_found = true;
    }
  }
  if (!violations_found && !invalid_codes)
  {
    std::cout << "No AXI violations found \n";
  }
  if (violations_found && aVerbose && !invalid_codes) 
  {
    int col1 = std::max(widths.first, strlen("CU Name")) + 4;
    int col2 = std::max(widths.second, strlen("AXI Portname"));

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
  return 0;
}

int xcldev::device::print_debug_ip_list (int aVerbose) {
    static const char *debug_ip_names[debug_ip_max_type] = {
        "unknown",
        "lapc",
        "ila",
        "aim",
        "tracefunnel",
        "monitorfifolite",
        "monitorfifofull",
        "accelmonitor",
        "asm"
    };
    int available_ip [debug_ip_max_type] = {0};
    std::string errmsg;
    std::vector<char> buf;
    pcidev::get_dev(m_idx)->sysfs_get("icap", "debug_ip_layout", errmsg, buf);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }
    debug_ip_layout *map = (debug_ip_layout *)buf.data();

    if (buf.empty() || map->m_count <= 0) {
        std::cout << "INFO: Failed to find any debug IPs on the platform. "
            << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is "
            << "successfully downloaded. \n";
        return 0;
    }

    std::cout << "Number of IPs found: " << map->m_count << "\n";
    for(unsigned int i = 0; i < map->m_count; i++) {
        if (map->m_debug_ip_data[i].m_type >
            sizeof (debug_ip_names) / sizeof (debug_ip_names[0])) {
            std::cout  << "Found invalid IP in debug ip layout with type "
                << map->m_debug_ip_data[i].m_type << std::endl;
            return -EINVAL;
        }
        ++available_ip[map->m_debug_ip_data[i].m_type];
    }

    std::stringstream sstr;
    for(unsigned int i = 0;
        i < sizeof (available_ip) / sizeof(available_ip[0]); ++i) {
        if (available_ip[i])
            sstr << debug_ip_names[i] << "(" << available_ip[i] << ") ";
    }

    std::cout << "IPs found [<ipname>(<count>)]: " << sstr.str() << std::endl;
    std::cout << "Run 'xbutil status' with option --<ipname> to get more "
        "information about the IP" << std::endl;
    return 0;
}
