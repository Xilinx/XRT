/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

/*
 * Copyright (C) 2015 Xilinx, Inc
 * Author: Paul Schumacher
 * Performance Monitoring using PCIe for XDMA HAL Driver
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

#include "shim.h"
#include "../user_common/perfmon_parameters.h"
#include "driver/include/xclbin.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <thread>
#include <vector>
#include <time.h>
#include <string>

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

namespace xocl {
  // ****************
  // Helper functions
  // ****************

  void XOCLShim::readDebugIpLayout()
  {
    if (mIsDebugIpLayoutRead)
      return;

    //
    // Profiling - addresses and names
    // Parsed from debug_ip_layout.rtd contained in xclbin
    if (mLogStream.is_open()) {
      mLogStream << "debug_ip_layout: reading profile addresses and names..." << std::endl;
    }

    mMemoryProfilingNumberSlots = getIPCountAddrNames(AXI_MM_MONITOR, mPerfMonBaseAddress,
      mPerfMonSlotName, mPerfmonProperties, XSPM_MAX_NUMBER_SLOTS);
    
    mAccelProfilingNumberSlots = getIPCountAddrNames(ACCEL_MONITOR, mAccelMonBaseAddress,
      mAccelMonSlotName, mAccelmonProperties, XSAM_MAX_NUMBER_SLOTS);

    mStreamProfilingNumberSlots = getIPCountAddrNames(AXI_STREAM_MONITOR, mStreamMonBaseAddress,
      mStreamMonSlotName, mStreammonProperties, XSSPM_MAX_NUMBER_SLOTS);
    
    mIsDeviceProfiling = (mMemoryProfilingNumberSlots > 0 || mAccelProfilingNumberSlots > 0);

    std::string fifoName;
    uint64_t fifoCtrlBaseAddr = 0x0;
    getIPCountAddrNames(AXI_MONITOR_FIFO_LITE, &fifoCtrlBaseAddr, &fifoName, nullptr, 1);
    mPerfMonFifoCtrlBaseAddress = fifoCtrlBaseAddr;

    uint64_t fifoReadBaseAddr = XPAR_AXI_PERF_MON_0_TRACE_OFFSET_AXI_FULL2;
    getIPCountAddrNames(AXI_MONITOR_FIFO_FULL, &fifoReadBaseAddr, &fifoName, nullptr, 1);
    mPerfMonFifoReadBaseAddress = fifoReadBaseAddr;

    uint64_t traceFunnelAddr = 0x0;
    getIPCountAddrNames(AXI_TRACE_FUNNEL, &traceFunnelAddr, nullptr, nullptr, 1);
    mTraceFunnelAddress = traceFunnelAddr;

    // Count accel monitors with stall monitoring turned on
    mStallProfilingNumberSlots = 0;
    for (unsigned int i = 0; i < mAccelProfilingNumberSlots; ++i) {
      if ((mAccelmonProperties[i] >> 2) & 0x1)
        mStallProfilingNumberSlots++;
    }

    if (mLogStream.is_open()) {
      for (unsigned int i = 0; i < mMemoryProfilingNumberSlots; ++i) {
        mLogStream << "debug_ip_layout: AXI_MM_MONITOR slot " << i << ": "
                   << "base address = 0x" << std::hex << mPerfMonBaseAddress[i]
                   << ", name = " << mPerfMonSlotName[i] << std::endl;
      }
      for (unsigned int i = 0; i < mAccelProfilingNumberSlots; ++i) {
        mLogStream << "debug_ip_layout: ACCEL_MONITOR slot " << i << ": "
                   << "base address = 0x" << std::hex << mAccelMonBaseAddress[i]
                   << ", name = " << mAccelMonSlotName[i] << std::endl;
      }
      for (unsigned int i = 0; i < mStreamProfilingNumberSlots; ++i) {
        mLogStream << "debug_ip_layout: STREAM_MONITOR slot " << i << ": "
                   << "base address = 0x" << std::hex << mStreamMonBaseAddress[i]
                   << ", name = " << mStreamMonSlotName[i] << std::endl;
     }
      mLogStream << "debug_ip_layout: AXI_MONITOR_FIFO_LITE: "
                 << "base address = 0x" << std::hex << fifoCtrlBaseAddr << std::endl;
      mLogStream << "debug_ip_layout: AXI_MONITOR_FIFO_FULL: "
                 << "base address = 0x" << std::hex << fifoReadBaseAddr << std::endl;
      mLogStream << "debug_ip_layout: AXI_TRACE_FUNNEL: "
                 << "base address = 0x" << std::hex << traceFunnelAddr << std::endl;
    }

    // Only need to read it once
    mIsDebugIpLayoutRead = true;
  }

  // Gets the information about the specified IP from the sysfs debug_ip_table.
  // The IP types are defined in xclbin.h
  uint32_t XOCLShim::getIPCountAddrNames(int type, uint64_t *baseAddress, std::string * portNames,
                                         uint8_t *properties, size_t size) {
    debug_ip_layout *map;
    std::string path = "/sys/bus/pci/devices/" + mDevUserName + "/debug_ip_layout";
    std::ifstream ifs(path.c_str(), std::ifstream::binary);
    uint32_t count = 0;
    char buffer[65536];
    if( ifs ) {
      //debug_ip_layout max size is 65536
      ifs.read(buffer, 65536);
      if (ifs.gcount() > 0) {
        map = (debug_ip_layout*)(buffer);
        for( unsigned int i = 0; i < map->m_count; i++ ) {
          if (count >= size) break;
          if (map->m_debug_ip_data[i].m_type == type) {
            if(baseAddress)baseAddress[count] = map->m_debug_ip_data[i].m_base_address;
            if(portNames) portNames[count] = (char*)map->m_debug_ip_data[i].m_name;
            if(properties) properties[count] = map->m_debug_ip_data[i].m_properties;
            ++count;
          }
        }
      }
      ifs.close();
    }
    return count;
  }

  // Read APM performance counters
  size_t XOCLShim::xclDebugReadCheckers(xclDebugCheckersResults* aCheckerResults) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << aCheckerResults
      << ", Read protocl checker status..." << std::endl;
    }

    size_t size = 0;

    uint64_t statusRegisters[] = {
        LAPC_OVERALL_STATUS_OFFSET,

        LAPC_CUMULATIVE_STATUS_0_OFFSET, LAPC_CUMULATIVE_STATUS_1_OFFSET,
        LAPC_CUMULATIVE_STATUS_2_OFFSET, LAPC_CUMULATIVE_STATUS_3_OFFSET,

        LAPC_SNAPSHOT_STATUS_0_OFFSET, LAPC_SNAPSHOT_STATUS_1_OFFSET,
        LAPC_SNAPSHOT_STATUS_2_OFFSET, LAPC_SNAPSHOT_STATUS_3_OFFSET
    };

    uint64_t baseAddress[XLAPC_MAX_NUMBER_SLOTS];
    uint32_t numSlots = getIPCountAddrNames(LAPC, baseAddress, nullptr, nullptr, XLAPC_MAX_NUMBER_SLOTS);
    uint32_t temp[XLAPC_STATUS_PER_SLOT];
    aCheckerResults->NumSlots = numSlots;
    snprintf(aCheckerResults->DevUserName, 256, "%s", mDevUserName.c_str());
    for (uint32_t s = 0; s < numSlots; ++s) {
      for (int c=0; c < XLAPC_STATUS_PER_SLOT; c++)
        size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER, baseAddress[s]+statusRegisters[c], &temp[c], 4);

      aCheckerResults->OverallStatus[s]      = temp[XLAPC_OVERALL_STATUS];
      std::copy(temp+XLAPC_CUMULATIVE_STATUS_0, temp+XLAPC_SNAPSHOT_STATUS_0, aCheckerResults->CumulativeStatus[s]);
      std::copy(temp+XLAPC_SNAPSHOT_STATUS_0, temp+XLAPC_STATUS_PER_SLOT, aCheckerResults->SnapshotStatus[s]);
    }

    return size;
  }

  // Read APM performance counters
  
  size_t XOCLShim::xclDebugReadCounters(xclDebugCountersResults* aCounterResults) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << XCL_PERF_MON_MEMORY << ", " << aCounterResults
      << ", Read device counters..." << std::endl;
    }

    size_t size = 0;

    uint64_t spm_offsets[] = {
        XSPM_SAMPLE_WRITE_BYTES_OFFSET,
        XSPM_SAMPLE_WRITE_TRANX_OFFSET,
        XSPM_SAMPLE_READ_BYTES_OFFSET,
        XSPM_SAMPLE_READ_TRANX_OFFSET,
        XSPM_SAMPLE_OUTSTANDING_COUNTS_OFFSET,
        XSPM_SAMPLE_LAST_WRITE_ADDRESS_OFFSET,
        XSPM_SAMPLE_LAST_WRITE_DATA_OFFSET,
        XSPM_SAMPLE_LAST_READ_ADDRESS_OFFSET,
        XSPM_SAMPLE_LAST_READ_DATA_OFFSET
    };

    uint64_t spm_upper_offsets[] = { 
        XSPM_SAMPLE_WRITE_BYTES_UPPER_OFFSET,
        XSPM_SAMPLE_WRITE_TRANX_UPPER_OFFSET,
        XSPM_SAMPLE_READ_BYTES_UPPER_OFFSET,
        XSPM_SAMPLE_READ_TRANX_UPPER_OFFSET,
        XSPM_SAMPLE_OUTSTANDING_COUNTS_UPPER_OFFSET,
        XSPM_SAMPLE_LAST_WRITE_ADDRESS_UPPER_OFFSET,
        XSPM_SAMPLE_LAST_WRITE_DATA_UPPER_OFFSET,
        XSPM_SAMPLE_LAST_READ_ADDRESS_UPPER_OFFSET,
        XSPM_SAMPLE_LAST_READ_DATA_UPPER_OFFSET
    };

    // Read all metric counters
    uint64_t baseAddress[XSPM_MAX_NUMBER_SLOTS];
    uint32_t numSlots = getIPCountAddrNames(AXI_MM_MONITOR, baseAddress, nullptr, mPerfmonProperties, XSPM_MAX_NUMBER_SLOTS);

    uint32_t temp[XSPM_DEBUG_SAMPLE_COUNTERS_PER_SLOT];

    aCounterResults->NumSlots = numSlots;
    snprintf(aCounterResults->DevUserName, 256, "%s", mDevUserName.c_str());
    for (uint32_t s=0; s < numSlots; s++) {
      uint32_t sampleInterval;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress[s] + XSPM_SAMPLE_OFFSET,
                    &sampleInterval, 4);

      // If applicable, read the upper 32-bits of the 64-bit debug counters
      if (mPerfmonProperties[s] & XSPM_64BIT_PROPERTY_MASK) {
	for (int c = 0 ; c < XSPM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
	  xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
		  baseAddress[s] + spm_upper_offsets[c],
		  &temp[c], 4) ;
	}
	aCounterResults->WriteBytes[s]    = ((uint64_t)(temp[0])) << 32 ;
	aCounterResults->WriteTranx[s]    = ((uint64_t)(temp[1])) << 32 ;
	aCounterResults->ReadBytes[s]     = ((uint64_t)(temp[2])) << 32 ;
	aCounterResults->ReadTranx[s]     = ((uint64_t)(temp[3])) << 32 ;
	aCounterResults->OutStandCnts[s]  = ((uint64_t)(temp[4])) << 32 ;
	aCounterResults->LastWriteAddr[s] = ((uint64_t)(temp[5])) << 32 ;
	aCounterResults->LastWriteData[s] = ((uint64_t)(temp[6])) << 32 ;
	aCounterResults->LastReadAddr[s]  = ((uint64_t)(temp[7])) << 32 ;
	aCounterResults->LastReadData[s]  = ((uint64_t)(temp[8])) << 32 ;
      }

      for (int c=0; c < XSPM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++)
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+spm_offsets[c], &temp[c], 4);

      aCounterResults->WriteBytes[s]    |= temp[0];
      aCounterResults->WriteTranx[s]    |= temp[1];
      aCounterResults->ReadBytes[s]     |= temp[2];
      aCounterResults->ReadTranx[s]     |= temp[3];
      aCounterResults->OutStandCnts[s]  |= temp[4];
      aCounterResults->LastWriteAddr[s] |= temp[5];
      aCounterResults->LastWriteData[s] |= temp[6];
      aCounterResults->LastReadAddr[s]  |= temp[7];
      aCounterResults->LastReadData[s]  |= temp[8];
    }
    return size;
  }

  // Read the streaming performance monitors

  size_t XOCLShim::xclDebugReadStreamingCounters(xclStreamingDebugCountersResults* aCounterResults) { 

    size_t size = 0; // The amount of data read from the hardware

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << XCL_PERF_MON_MEMORY << ", " << aCounterResults
      << ", Read streaming device counters..." << std::endl;
    }

    // Get the base addresses of all the SSPM IPs in the debug IP layout 
    uint64_t baseAddress[XSSPM_MAX_NUMBER_SLOTS];
    uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_MONITOR, 
					    baseAddress, 
					    nullptr, nullptr, 
					    XSSPM_MAX_NUMBER_SLOTS);

    // Fill up the portions of the return struct that are known by the runtime
    aCounterResults->NumSlots = numSlots ;
    snprintf(aCounterResults->DevUserName, 256, "%s", mDevUserName.c_str());

    // Fill up the return structure with the values read from the hardware
    uint64_t sspm_offsets[] = {
      XSSPM_NUM_TRANX_OFFSET,
      XSSPM_DATA_BYTES_OFFSET,
      XSSPM_BUSY_CYCLES_OFFSET,
      XSSPM_STALL_CYCLES_OFFSET,
      XSSPM_STARVE_CYCLES_OFFSET
    };

    for (unsigned int i = 0 ; i < numSlots ; ++i)
    {
      uint32_t sampleInterval ;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
		      baseAddress[i] + XSSPM_SAMPLE_OFFSET,
		      &sampleInterval, sizeof(uint32_t));

      // Then read all the individual 64-bit counters
      unsigned long long int tmp[XSSPM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] ;

      for (unsigned int j = 0 ; j < XSSPM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; ++j)
      {
	size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			baseAddress[i] + sspm_offsets[j],
			&tmp[j], sizeof(unsigned long long int));
      }
      aCounterResults->StrNumTranx[i] = tmp[0] ;
      aCounterResults->StrDataBytes[i] = tmp[1] ;
      aCounterResults->StrBusyCycles[i] = tmp[2] ;
      aCounterResults->StrStallCycles[i] = tmp[3] ;
      aCounterResults->StrStarveCycles[i] = tmp[4] ;
    }
    return size;
  } 

  size_t XOCLShim::xclDebugReadAccelMonitorCounters(xclAccelMonitorCounterResults* samResult) {
    size_t size = 0;

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << XCL_PERF_MON_MEMORY << ", " << aCounterResults
      << ", Read device counters..." << std::endl;
    }
    return 0;
  }

} 
// namespace xocl_gem

size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type, void* debugResults)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  if (!drv)
    return -1;
  switch (type) {
    case XCL_DEBUG_READ_TYPE_LAPC :
      return drv->xclDebugReadCheckers(reinterpret_cast<xclDebugCheckersResults*>(debugResults));
    case XCL_DEBUG_READ_TYPE_SPM :
      return drv->xclDebugReadCounters(reinterpret_cast<xclDebugCountersResults*>(debugResults));
    case XCL_DEBUG_READ_TYPE_SAM :
      return 0;
  case XCL_DEBUG_READ_TYPE_SSPM :
    return drv->xclDebugReadStreamingCounters(reinterpret_cast<xclStreamingDebugCountersResults*>(debugResults));
    default:
      ;
  };
  return -1;
}


