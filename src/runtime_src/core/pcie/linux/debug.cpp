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
#include "scan.h"
#include "xcl_perfmon_parameters.h"
#include "xclbin.h"
#include "core/common/message.h"

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

  void shim::readDebugIpLayout()
  {
    if (mIsDebugIpLayoutRead)
      return;

    uint liveProcessesOnDevice = xclGetNumLiveProcesses();
    if(liveProcessesOnDevice > 1) {
      /* More than 1 process on device. Device Profiling for multi-process not supported yet.
       */
      std::string warnMsg = "Multiple live processes running on device. Hardware Debug and Profiling data will be unavailable for this process.";
      std::cout << warnMsg << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", warnMsg) ;
      mIsDeviceProfiling = false;
      mIsDebugIpLayoutRead = true;
      return;
    }

    //
    // Profiling - addresses and names
    // Parsed from debug_ip_layout.rtd contained in xclbin
    if (mLogStream.is_open()) {
      mLogStream << "debug_ip_layout: reading profile addresses and names..." << std::endl;
    }

    mMemoryProfilingNumberSlots = getIPCountAddrNames(AXI_MM_MONITOR, mPerfMonBaseAddress,
      mPerfMonSlotName, mPerfmonProperties, mPerfmonMajorVersions, mPerfmonMinorVersions, XAIM_MAX_NUMBER_SLOTS);

    mAccelProfilingNumberSlots = getIPCountAddrNames(ACCEL_MONITOR, mAccelMonBaseAddress,
      mAccelMonSlotName, mAccelmonProperties, mAccelmonMajorVersions, mAccelmonMinorVersions, XAM_MAX_NUMBER_SLOTS);

    mStreamProfilingNumberSlots = getIPCountAddrNames(AXI_STREAM_MONITOR, mStreamMonBaseAddress,
      mStreamMonSlotName, mStreammonProperties, mStreammonMajorVersions, mStreammonMinorVersions, XASM_MAX_NUMBER_SLOTS);

    mIsDeviceProfiling = (mMemoryProfilingNumberSlots > 0 || mAccelProfilingNumberSlots > 0 || mStreamProfilingNumberSlots > 0);

    std::string fifoName;
    uint64_t fifoCtrlBaseAddr = 0x0;
    getIPCountAddrNames(AXI_MONITOR_FIFO_LITE, &fifoCtrlBaseAddr, &fifoName, nullptr, nullptr, nullptr, 1);
    mPerfMonFifoCtrlBaseAddress = fifoCtrlBaseAddr;

    uint64_t fifoReadBaseAddr = XPAR_AXI_PERF_MON_0_TRACE_OFFSET_AXI_FULL2;
    getIPCountAddrNames(AXI_MONITOR_FIFO_FULL, &fifoReadBaseAddr, &fifoName, &mTraceFifoProperties, nullptr, nullptr, 1);
    mPerfMonFifoReadBaseAddress = fifoReadBaseAddr;

    uint64_t traceFunnelAddr = 0x0;
    getIPCountAddrNames(AXI_TRACE_FUNNEL, &traceFunnelAddr, nullptr, nullptr, nullptr, nullptr, 1);
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
  uint32_t shim::getIPCountAddrNames(int type, uint64_t *baseAddress, std::string * portNames,
                                         uint8_t *properties, uint8_t *majorVersions, uint8_t *minorVersions,
                                         size_t size) {
    debug_ip_layout *map;
    auto dev = pcidev::get_dev(mBoardNumber);
    std::string subdev_str = "icap";
    std::string entry_str = "debug_ip_layout";
    std::string path = dev->get_sysfs_path(subdev_str, entry_str);
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
            if(portNames) {
              // Fill up string with 128 characters (padded with null characters)
              portNames[count].assign(map->m_debug_ip_data[i].m_name, 128);
              // Strip away extraneous null characters
              portNames[count].assign(portNames[count].c_str());
            }
            if(properties) properties[count] = map->m_debug_ip_data[i].m_properties;
            if(majorVersions) majorVersions[count] = map->m_debug_ip_data[i].m_major;
            if(minorVersions) minorVersions[count] = map->m_debug_ip_data[i].m_minor;
            ++count;
          }
        }
      }
      ifs.close();
    }

    return count;
  }

  // Read APM performance counters
  size_t shim::xclDebugReadCheckers(xclDebugCheckersResults* aCheckerResults) {
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
    uint32_t numSlots = getIPCountAddrNames(LAPC, baseAddress, nullptr, nullptr, nullptr, nullptr, XLAPC_MAX_NUMBER_SLOTS);
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

  size_t shim::xclDebugReadCounters(xclDebugCountersResults* aCounterResults) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << XCL_PERF_MON_MEMORY << ", " << aCounterResults
      << ", Read device counters..." << std::endl;
    }

    size_t size = 0;

    uint64_t spm_offsets[] = {
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

    uint64_t spm_upper_offsets[] = {
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

    // Read all metric counters
    uint64_t baseAddress[XAIM_MAX_NUMBER_SLOTS];
    uint32_t numSlots = getIPCountAddrNames(AXI_MM_MONITOR, baseAddress, nullptr, mPerfmonProperties, nullptr, nullptr, XAIM_MAX_NUMBER_SLOTS);

    uint32_t temp[XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT];

    aCounterResults->NumSlots = numSlots;
    snprintf(aCounterResults->DevUserName, 256, "%s", mDevUserName.c_str());
    for (uint32_t s=0; s < numSlots; s++) {
      uint32_t sampleInterval;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress[s] + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, 4);

      // If applicable, read the upper 32-bits of the 64-bit debug counters
      if (mPerfmonProperties[s] & XAIM_64BIT_PROPERTY_MASK) {
	for (int c = 0 ; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
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

      for (int c=0; c < XAIM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++)
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

  size_t shim::xclDebugReadStreamingCounters(xclStreamingDebugCountersResults* aCounterResults) {

    size_t size = 0; // The amount of data read from the hardware

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << XCL_PERF_MON_MEMORY << ", " << aCounterResults
      << ", Read streaming device counters..." << std::endl;
    }

    // Get the base addresses of all the SSPM IPs in the debug IP layout
    uint64_t baseAddress[XASM_MAX_NUMBER_SLOTS];
    uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_MONITOR,
					    baseAddress,
					    nullptr, nullptr, nullptr, nullptr,
					    XASM_MAX_NUMBER_SLOTS);

    // Fill up the portions of the return struct that are known by the runtime
    aCounterResults->NumSlots = numSlots ;
    snprintf(aCounterResults->DevUserName, 256, "%s", mDevUserName.c_str());

    // Fill up the return structure with the values read from the hardware
    uint64_t sspm_offsets[] = {
      XASM_NUM_TRANX_OFFSET,
      XASM_DATA_BYTES_OFFSET,
      XASM_BUSY_CYCLES_OFFSET,
      XASM_STALL_CYCLES_OFFSET,
      XASM_STARVE_CYCLES_OFFSET
    };

    for (unsigned int i = 0 ; i < numSlots ; ++i)
    {
      uint32_t sampleInterval ;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
		      baseAddress[i] + XASM_SAMPLE_OFFSET,
		      &sampleInterval, sizeof(uint32_t));

      // Then read all the individual 64-bit counters
      unsigned long long int tmp[XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] ;

      for (unsigned int j = 0 ; j < XASM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; ++j)
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

  size_t shim::xclDebugReadStreamingCheckers(xclDebugStreamingCheckersResults* aStreamingCheckerResults) {

    size_t size = 0; // The amount of data read from the hardware

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << XCL_PERF_MON_MEMORY << ", " << aStreamingCheckerResults
      << ", Read streaming protocol checkers..." << std::endl;
    }

    // Get the base addresses of all the SPC IPs in the debug IP layout
    uint64_t baseAddress[XSPC_MAX_NUMBER_SLOTS];
    uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_PROTOCOL_CHECKER,
					    baseAddress,
					    nullptr, nullptr, nullptr, nullptr,
					    XSPC_MAX_NUMBER_SLOTS);

    // Fill up the portions of the return struct that are known by the runtime
    aStreamingCheckerResults->NumSlots = numSlots ;
    snprintf(aStreamingCheckerResults->DevUserName, 256, "%s", mDevUserName.c_str());

    // Fill up the return structure with the values read from the hardware
    for (unsigned int i = 0 ; i < numSlots ; ++i)
    {
      uint32_t pc_asserted ;
      uint32_t current_pc ;
      uint32_t snapshot_pc ;

      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
		      baseAddress[i] + XSPC_PC_ASSERTED_OFFSET,
		      &pc_asserted, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
		      baseAddress[i] + XSPC_CURRENT_PC_OFFSET,
		      &current_pc, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
		      baseAddress[i] + XSPC_SNAPSHOT_PC_OFFSET,
		      &snapshot_pc, sizeof(uint32_t));

      aStreamingCheckerResults->PCAsserted[i] = pc_asserted;
      aStreamingCheckerResults->CurrentPC[i] = current_pc;
      aStreamingCheckerResults->SnapshotPC[i] = snapshot_pc;
    }
    return size;
  }

  size_t shim::xclDebugReadAccelMonitorCounters(xclAccelMonitorCounterResults* samResult) {
    size_t size = 0;

    /*
      Here should read the version number
      and return immediately if version
      is not supported
    */

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << XCL_PERF_MON_MEMORY << ", " << samResult
      << ", Read device counters..." << std::endl;
    }

    uint64_t sam_offsets[] = {
        XAM_ACCEL_EXECUTION_COUNT_OFFSET,
        XAM_ACCEL_EXECUTION_CYCLES_OFFSET,
        XAM_ACCEL_STALL_INT_OFFSET,
        XAM_ACCEL_STALL_STR_OFFSET,
        XAM_ACCEL_STALL_EXT_OFFSET,
        XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET,
        XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET,
        XAM_ACCEL_TOTAL_CU_START_OFFSET
    };

    uint64_t sam_upper_offsets[] = {
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
    uint64_t baseAddress[XAM_MAX_NUMBER_SLOTS] = {0};
    uint8_t  accelmonProperties[XAM_MAX_NUMBER_SLOTS] = {0};
    uint8_t  accelmonMajorVersions[XAM_MAX_NUMBER_SLOTS] = {0};
    uint8_t  accelmonMinorVersions[XAM_MAX_NUMBER_SLOTS] = {0};

    uint32_t numSlots = getIPCountAddrNames(ACCEL_MONITOR, baseAddress, nullptr, accelmonProperties,
                                            accelmonMajorVersions, accelmonMinorVersions, XAM_MAX_NUMBER_SLOTS);

    uint32_t temp[XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT] = {0};

    samResult->NumSlots = numSlots;
    snprintf(samResult->DevUserName, 256, "%s", mDevUserName.c_str());
    for (uint32_t s=0; s < numSlots; s++) {
      uint32_t sampleInterval;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress[s] + XAM_SAMPLE_OFFSET,
                    &sampleInterval, 4);

      bool hasDataflow = (cmpMonVersions(accelmonMajorVersions[s],accelmonMinorVersions[s],1,1) < 0) ? true : false;

      // If applicable, read the upper 32-bits of the 64-bit debug counters
      if (accelmonProperties[s] & XAM_64BIT_PROPERTY_MASK) {
        for (int c = 0 ; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT ; ++c) {
          xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
            baseAddress[s] + sam_upper_offsets[c],
            &temp[c], 4) ;
        }
        samResult->CuExecCount[s]      = ((uint64_t)(temp[0])) << 32;
        samResult->CuExecCycles[s]     = ((uint64_t)(temp[1])) << 32;
        samResult->CuStallExtCycles[s] = ((uint64_t)(temp[2])) << 32;
        samResult->CuStallIntCycles[s] = ((uint64_t)(temp[3])) << 32;
        samResult->CuStallStrCycles[s] = ((uint64_t)(temp[4])) << 32;
        samResult->CuMinExecCycles[s]  = ((uint64_t)(temp[5])) << 32;
        samResult->CuMaxExecCycles[s]  = ((uint64_t)(temp[6])) << 32;
        samResult->CuStartCount[s]     = ((uint64_t)(temp[7])) << 32;

        if(hasDataflow) {
          uint64_t dfTmp[2] = {0};
          xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_BUSY_CYCLES_UPPER_OFFSET, &dfTmp[0], 4);
          xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET, &dfTmp[1], 4);

          samResult->CuBusyCycles[s]      = dfTmp[0] << 32;
          samResult->CuMaxParallelIter[s] = dfTmp[1] << 32;
        }
      }

      for (int c=0; c < XAM_DEBUG_SAMPLE_COUNTERS_PER_SLOT; c++)
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+sam_offsets[c], &temp[c], 4);

      samResult->CuExecCount[s]      |= temp[0];
      samResult->CuExecCycles[s]     |= temp[1];
      samResult->CuStallExtCycles[s] |= temp[2];
      samResult->CuStallIntCycles[s] |= temp[3];
      samResult->CuStallStrCycles[s] |= temp[4];
      samResult->CuMinExecCycles[s]  |= temp[5];
      samResult->CuMaxExecCycles[s]  |= temp[6];
      samResult->CuStartCount[s]     |= temp[7];

      if(hasDataflow) {
        uint64_t dfTmp[2] = {0};
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_BUSY_CYCLES_OFFSET, &dfTmp[0], 4);
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + XAM_MAX_PARALLEL_ITER_OFFSET, &dfTmp[1], 4);

        samResult->CuBusyCycles[s]      |= dfTmp[0] << 32;
        samResult->CuMaxParallelIter[s] |= dfTmp[1] << 32;
      } else {
        samResult->CuBusyCycles[s]      = samResult->CuExecCycles[s];
        samResult->CuMaxParallelIter[s] = 1;
      }
    }

    return size;
  }

} // namespace xocl_gem

size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type, void* debugResults)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if (!drv)
    return -1;
  switch (type) {
    case XCL_DEBUG_READ_TYPE_LAPC :
      return drv->xclDebugReadCheckers(reinterpret_cast<xclDebugCheckersResults*>(debugResults));
    case XCL_DEBUG_READ_TYPE_AIM :
      return drv->xclDebugReadCounters(reinterpret_cast<xclDebugCountersResults*>(debugResults));
    case XCL_DEBUG_READ_TYPE_AM :
      return drv->xclDebugReadAccelMonitorCounters(reinterpret_cast<xclAccelMonitorCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_ASM :
    return drv->xclDebugReadStreamingCounters(reinterpret_cast<xclStreamingDebugCountersResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_SPC:
    return drv->xclDebugReadStreamingCheckers(reinterpret_cast<xclDebugStreamingCheckersResults*>(debugResults));
    default:
      ;
  };
  return -1;
}
