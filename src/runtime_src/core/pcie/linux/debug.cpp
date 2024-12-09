// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2015-2017 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidev.h"
#include "shim.h"
#include "xrt/detail/xclbin.h"

#include "core/common/message.h"
#include "core/include/xdp/aim.h"
#include "core/include/xdp/am.h"
#include "core/include/xdp/asm.h"
#include "core/include/xdp/common.h"
#include "core/include/xdp/counters.h"
#include "core/include/xdp/lapc.h"
#include "core/include/xdp/spc.h"
#include "core/include/deprecated/xcl_app_debug.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <time.h>
#include <vector>

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

  /*
   * Returns  1 if Version2 > Version1
   * Returns  0 if Version2 = Version1
   * Returns -1 if Version2 < Version1
   */
  signed shim::cmpMonVersions(unsigned major1, unsigned minor1, unsigned major2, unsigned minor2) {
    if (major2 > major1)
      return 1;
    else if (major2 < major1)
      return -1;
    else if (minor2 > minor1)
      return 1;
    else if (minor2 < minor1)
      return -1;
    else return 0;
  }


  // Gets the information about the specified IP from the sysfs debug_ip_table.
  // The IP types are defined in xclbin.h
  uint32_t shim::getIPCountAddrNames(int type, uint64_t *baseAddress, std::string * portNames,
                                         uint8_t *properties, uint8_t *majorVersions, uint8_t *minorVersions,
                                         size_t size) {
    debug_ip_layout *map;
    auto dev = xrt_core::pci::get_dev(mBoardNumber);
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
  size_t shim::xclDebugReadCheckers(xdp::LAPCCounterResults* aCheckerResults) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
      << ", " << aCheckerResults
      << ", Read protocl checker status..." << std::endl;
    }

    size_t size = 0;

    uint64_t statusRegisters[] = {
      xdp::IP::LAPC::AXI_LITE::STATUS,
      xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_0,
      xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_1,
      xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_2,
      xdp::IP::LAPC::AXI_LITE::CUMULATIVE_STATUS_3,
      xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_0,
      xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_1,
      xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_2,
      xdp::IP::LAPC::AXI_LITE::SNAPSHOT_STATUS_3
    };

    uint64_t baseAddress[xdp::MAX_NUM_LAPCS];
    uint32_t numSlots = getIPCountAddrNames(LAPC, baseAddress, nullptr, nullptr, nullptr, nullptr, xdp::MAX_NUM_LAPCS);
    uint32_t temp[xdp::IP::LAPC::NUM_COUNTERS];
    aCheckerResults->NumSlots = numSlots;
    snprintf(aCheckerResults->DevUserName, 256, "%s", mDevUserName.c_str());
    for (uint32_t s = 0; s < numSlots; ++s) {
      for (int c=0; c < xdp::IP::LAPC::NUM_COUNTERS; c++)
        size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER, baseAddress[s]+statusRegisters[c], &temp[c], 4);

      aCheckerResults->OverallStatus[s]      = temp[xdp::IP::LAPC::sysfs::STATUS];

      std::copy(temp+xdp::IP::LAPC::sysfs::CUMULATIVE_STATUS_0, temp+xdp::IP::LAPC::sysfs::SNAPSHOT_STATUS_0, aCheckerResults->CumulativeStatus[s]);
      std::copy(temp+xdp::IP::LAPC::sysfs::SNAPSHOT_STATUS_0, temp+xdp::IP::LAPC::NUM_COUNTERS, aCheckerResults->SnapshotStatus[s]);
    }

    return size;
  }

  // Read AIM performance counters

  size_t shim::xclDebugReadCounters(xdp::AIMCounterResults* aCounterResults) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
                 << ", " << xdp::MonitorType::memory << ", " << aCounterResults
                 << ", Read device counters..." << std::endl;
    }

    size_t size = 0;

    uint64_t aim_offsets[] = {
      xdp::IP::AIM::AXI_LITE::WRITE_BYTES,
      xdp::IP::AIM::AXI_LITE::WRITE_TRANX,
      xdp::IP::AIM::AXI_LITE::READ_BYTES,
      xdp::IP::AIM::AXI_LITE::READ_TRANX,
      xdp::IP::AIM::AXI_LITE::OUTSTANDING_COUNTS,
      xdp::IP::AIM::AXI_LITE::LAST_WRITE_ADDRESS,
      xdp::IP::AIM::AXI_LITE::LAST_WRITE_DATA,
      xdp::IP::AIM::AXI_LITE::LAST_READ_ADDRESS,
      xdp::IP::AIM::AXI_LITE::LAST_READ_DATA
    };

    uint64_t aim_upper_offsets[] = {
      xdp::IP::AIM::AXI_LITE::WRITE_BYTES_UPPER,
      xdp::IP::AIM::AXI_LITE::WRITE_TRANX_UPPER,
      xdp::IP::AIM::AXI_LITE::READ_BYTES_UPPER,
      xdp::IP::AIM::AXI_LITE::READ_TRANX_UPPER,
      xdp::IP::AIM::AXI_LITE::OUTSTANDING_COUNTS_UPPER,
      xdp::IP::AIM::AXI_LITE::LAST_WRITE_ADDRESS_UPPER,
      xdp::IP::AIM::AXI_LITE::LAST_WRITE_DATA_UPPER,
      xdp::IP::AIM::AXI_LITE::LAST_READ_ADDRESS_UPPER,
      xdp::IP::AIM::AXI_LITE::LAST_READ_DATA_UPPER
    };

    // Read all metric counters
    uint64_t baseAddress[xdp::MAX_NUM_AIMS];
    uint8_t  perfMonProperties[xdp::MAX_NUM_AIMS] = {};
    uint32_t numSlots = getIPCountAddrNames(AXI_MM_MONITOR, baseAddress, nullptr, perfMonProperties, nullptr, nullptr, xdp::MAX_NUM_AIMS);

    uint32_t temp[xdp::IP::AIM::NUM_COUNTERS_REPORT];

    aCounterResults->NumSlots = numSlots;
    snprintf(aCounterResults->DevUserName, 256, "%s", mDevUserName.c_str());
    for (uint32_t s=0; s < numSlots; s++) {
      uint32_t sampleInterval;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress[s] + xdp::IP::AIM::AXI_LITE::SAMPLE,
                      &sampleInterval, 4);

      // If applicable, read the upper 32-bits of the 64-bit debug counters
      if (perfMonProperties[s] & xdp::IP::AIM::mask::PROPERTY_64BIT) {
	for (int c = 0; c < xdp::IP::AIM::NUM_COUNTERS_REPORT; ++c) {
	  xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
		  baseAddress[s] + aim_upper_offsets[c],
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

      for (int c=0; c < xdp::IP::AIM::NUM_COUNTERS_REPORT; c++)
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+aim_offsets[c], &temp[c], 4);

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

  size_t shim::xclDebugReadStreamingCounters(xdp::ASMCounterResults* aCounterResults) {

    size_t size = 0; // The amount of data read from the hardware

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
                 << ", " << xdp::MonitorType::memory << ", " << aCounterResults
                 << ", Read streaming device counters..." << std::endl;
    }

    // Get the base addresses of all the SSPM IPs in the debug IP layout
    uint64_t baseAddress[xdp::MAX_NUM_ASMS];
    uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_MONITOR,
					    baseAddress,
					    nullptr, nullptr, nullptr, nullptr,
					    xdp::MAX_NUM_ASMS);

    // Fill up the portions of the return struct that are known by the runtime
    aCounterResults->NumSlots = numSlots ;
    snprintf(aCounterResults->DevUserName, 256, "%s", mDevUserName.c_str());

    // Fill up the return structure with the values read from the hardware
    uint64_t sspm_offsets[] = {
      xdp::IP::ASM::AXI_LITE::NUM_TRANX,
      xdp::IP::ASM::AXI_LITE::DATA_BYTES,
      xdp::IP::ASM::AXI_LITE::BUSY_CYCLES,
      xdp::IP::ASM::AXI_LITE::STALL_CYCLES,
      xdp::IP::ASM::AXI_LITE::STARVE_CYCLES
    };

    for (unsigned int i = 0 ; i < numSlots ; ++i)
    {
      uint32_t sampleInterval ;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
		      baseAddress[i] + xdp::IP::ASM::AXI_LITE::SAMPLE,
		      &sampleInterval, sizeof(uint32_t));

      // Then read all the individual 64-bit counters
      unsigned long long int tmp[xdp::IP::ASM::NUM_COUNTERS] ;

      for (unsigned int j = 0 ; j < xdp::IP::ASM::NUM_COUNTERS; ++j)
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

  size_t shim::xclDebugReadStreamingCheckers(xdp::SPCCounterResults* aStreamingCheckerResults) {

    size_t size = 0; // The amount of data read from the hardware

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
                 << ", " << xdp::MonitorType::memory << ", " << aStreamingCheckerResults
                 << ", Read streaming protocol checkers..." << std::endl;
    }

    // Get the base addresses of all the SPC IPs in the debug IP layout
    uint64_t baseAddress[xdp::MAX_NUM_SPCS];
    uint32_t numSlots = getIPCountAddrNames(AXI_STREAM_PROTOCOL_CHECKER,
					    baseAddress,
					    nullptr, nullptr, nullptr, nullptr,
					    xdp::MAX_NUM_SPCS);

    // Fill up the portions of the return struct that are known by the runtime
    aStreamingCheckerResults->NumSlots = numSlots ;
    snprintf(aStreamingCheckerResults->DevUserName, 256, "%s", mDevUserName.c_str());

    // Fill up the return structure with the values read from the hardware
    for (unsigned int i = 0 ; i < numSlots ; ++i) {
      uint32_t pc_asserted ;
      uint32_t current_pc ;
      uint32_t snapshot_pc ;

      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
		      baseAddress[i] + xdp::IP::SPC::AXI_LITE::PC_ASSERTED,
		      &pc_asserted, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
		      baseAddress[i] + xdp::IP::SPC::AXI_LITE::CURRENT_PC,
		      &current_pc, sizeof(uint32_t));
      size += xclRead(XCL_ADDR_SPACE_DEVICE_CHECKER,
		      baseAddress[i] + xdp::IP::SPC::AXI_LITE::SNAPSHOT_PC,
		      &snapshot_pc, sizeof(uint32_t));

      aStreamingCheckerResults->PCAsserted[i] = pc_asserted;
      aStreamingCheckerResults->CurrentPC[i] = current_pc;
      aStreamingCheckerResults->SnapshotPC[i] = snapshot_pc;
    }
    return size;
  }

  size_t shim::xclDebugReadAccelMonitorCounters(xdp::AMCounterResults* samResult) {
    size_t size = 0;

    /*
      Here should read the version number
      and return immediately if version
      is not supported
    */

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
                 << ", " << xdp::MonitorType::memory << ", " << samResult
                 << ", Read device counters..." << std::endl;
    }

    uint64_t am_offsets[] = {
      xdp::IP::AM::AXI_LITE::EXECUTION_COUNT,
      xdp::IP::AM::AXI_LITE::EXECUTION_CYCLES,
      xdp::IP::AM::AXI_LITE::STALL_INT,
      xdp::IP::AM::AXI_LITE::STALL_STR,
      xdp::IP::AM::AXI_LITE::STALL_EXT,
      xdp::IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES,
      xdp::IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES,
      xdp::IP::AM::AXI_LITE::TOTAL_CU_START
    };

    uint64_t am_upper_offsets[] = {
      xdp::IP::AM::AXI_LITE::EXECUTION_COUNT_UPPER,
      xdp::IP::AM::AXI_LITE::EXECUTION_CYCLES_UPPER,
      xdp::IP::AM::AXI_LITE::STALL_INT_UPPER,
      xdp::IP::AM::AXI_LITE::STALL_STR_UPPER,
      xdp::IP::AM::AXI_LITE::STALL_EXT_UPPER,
      xdp::IP::AM::AXI_LITE::MIN_EXECUTION_CYCLES_UPPER,
      xdp::IP::AM::AXI_LITE::MAX_EXECUTION_CYCLES_UPPER,
      xdp::IP::AM::AXI_LITE::TOTAL_CU_START
    };

    // Read all metric counters
    uint64_t baseAddress[xdp::MAX_NUM_AMS] = {0};
    uint8_t  accelmonProperties[xdp::MAX_NUM_AMS] = {0};
    uint8_t  accelmonMajorVersions[xdp::MAX_NUM_AMS] = {0};
    uint8_t  accelmonMinorVersions[xdp::MAX_NUM_AMS] = {0};

    uint32_t numSlots = getIPCountAddrNames(ACCEL_MONITOR, baseAddress, nullptr, accelmonProperties,
                                            accelmonMajorVersions, accelmonMinorVersions, xdp::MAX_NUM_AMS);

    uint32_t temp[xdp::IP::AM::NUM_COUNTERS_REPORT] = {0};

    samResult->NumSlots = numSlots;
    snprintf(samResult->DevUserName, 256, "%s", mDevUserName.c_str());
    for (uint32_t s=0; s < numSlots; s++) {
      uint32_t sampleInterval;
      // Read sample interval register to latch the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress[s] + xdp::IP::AM::AXI_LITE::SAMPLE,
                      &sampleInterval, 4);

      bool hasDataflow = (cmpMonVersions(accelmonMajorVersions[s],accelmonMinorVersions[s],1,1) < 0) ? true : false;

      // If applicable, read the upper 32-bits of the 64-bit debug counters
      if (accelmonProperties[s] & xdp::IP::AM::mask::PROPERTY_64BIT) {
        for (int c = 0; c < xdp::IP::AM::NUM_COUNTERS_REPORT; ++c) {
          xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
            baseAddress[s] + am_upper_offsets[c],
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
          xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::BUSY_CYCLES_UPPER, &dfTmp[0], 4);
          xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::MAX_PARALLEL_ITER_UPPER, &dfTmp[1], 4);

          samResult->CuBusyCycles[s]      = dfTmp[0] << 32;
          samResult->CuMaxParallelIter[s] = dfTmp[1] << 32;
        }
      }

      for (int c=0; c < xdp::IP::AM::NUM_COUNTERS_REPORT; c++)
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s]+am_offsets[c], &temp[c], 4);

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
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::BUSY_CYCLES, &dfTmp[0], 4);
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress[s] + xdp::IP::AM::AXI_LITE::MAX_PARALLEL_ITER, &dfTmp[1], 4);

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
      return drv->xclDebugReadCheckers(reinterpret_cast<xdp::LAPCCounterResults*>(debugResults));
    case XCL_DEBUG_READ_TYPE_AIM :
      return drv->xclDebugReadCounters(reinterpret_cast<xdp::AIMCounterResults*>(debugResults));
    case XCL_DEBUG_READ_TYPE_AM :
      return drv->xclDebugReadAccelMonitorCounters(reinterpret_cast<xdp::AMCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_ASM :
    return drv->xclDebugReadStreamingCounters(reinterpret_cast<xdp::ASMCounterResults*>(debugResults));
  case XCL_DEBUG_READ_TYPE_SPC:
    return drv->xclDebugReadStreamingCheckers(reinterpret_cast<xdp::SPCCounterResults*>(debugResults));
    default:
      ;
  };
  return -1;
}
