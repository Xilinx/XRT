/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

#include "device_intf.h"
#include "xclperf.h"
#include "xcl_perfmon_parameters.h"

#include <iostream>
#include <cstdio>
#include <cstring>
//#include <algorithm>
#include <thread>
#include <vector>
//#include <time.h>
#include <string.h>
#include <chrono>

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

namespace xdp {

  // ***************************************************************************
  // Read/Write
  // ***************************************************************************

  size_t DeviceIntf::write(uint64_t offset, const void *hostBuf, size_t size)
  {
    if (mHandle == nullptr)
      return 0;
	return xclWrite(mHandle, XCL_ADDR_SPACE_DEVICE_PERFMON, offset, hostBuf, size);
  }

  size_t DeviceIntf::read(uint64_t offset, void *hostBuf, size_t size)
  {
    if (mHandle == nullptr)
      return 0;
	return xclRead(mHandle, XCL_ADDR_SPACE_DEVICE_PERFMON, offset, hostBuf, size);
  }

  size_t DeviceIntf::traceRead(void *buffer, size_t size, uint64_t addr)
  {
    if (mHandle == nullptr)
      return 0;
    return xclUnmgdPread(mHandle, 0, buffer, size, addr);
  }

  // ***************************************************************************
  // Generic Helper functions
  // ***************************************************************************

  // Get host timestamp to write to monitors
  // IMPORTANT NOTE: this *must* be compatible with the method of generating
  // timestamps as defined in RTProfile::getTraceTime()
  uint64_t DeviceIntf::getHostTraceTimeNsec()
  {
    using namespace std::chrono;
    typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
    duration_ns time_span =
        duration_cast<duration_ns>(high_resolution_clock::now().time_since_epoch());
    return time_span.count();
  }

  // Convert decimal to binary string
  // NOTE: length of string is always sizeof(uint32_t) * 8
  std::string DeviceIntf::dec2bin(uint32_t n) {
    char result[(sizeof(uint32_t) * 8) + 1];
    unsigned index = sizeof(uint32_t) * 8;
    result[index] = '\0';

    do {
      result[ --index ] = '0' + (n & 1);
    } while (n >>= 1);

    for (int i=index-1; i >= 0; --i)
      result[i] = '0';

    return std::string( result );
  }

  // Convert decimal to binary string of length bits
  std::string DeviceIntf::dec2bin(uint32_t n, unsigned bits) {
    char result[bits + 1];
    unsigned index = bits;
    result[index] = '\0';

    do result[ --index ] = '0' + (n & 1);
    while (n >>= 1);

    for (int i=index-1; i >= 0; --i)
      result[i] = '0';

    return std::string( result );
  }

  uint32_t DeviceIntf::getMaxSamples(xclPerfMonType type)
  {
    if (type == XCL_PERF_MON_MEMORY) return XPAR_AXI_PERF_MON_0_TRACE_NUMBER_SAMPLES;
    if (type == XCL_PERF_MON_HOST) return XPAR_AXI_PERF_MON_1_TRACE_NUMBER_SAMPLES;
    // TODO: get number of samples from metadata
    if (type == XCL_PERF_MON_ACCEL) return XPAR_AXI_PERF_MON_2_TRACE_NUMBER_SAMPLES;
    return 0;
  }

  // ***************************************************************************
  // Debug IP Layout
  // ***************************************************************************

  uint64_t DeviceIntf::getBaseAddress(xclPerfMonType type, uint32_t slotNum)
  {
    if (type == XCL_PERF_MON_MEMORY) return mBaseAddress[slotNum];
    if (type == XCL_PERF_MON_ACCEL)  return mAccelMonBaseAddress[slotNum];
    if (type == XCL_PERF_MON_STR)    return mStreamMonBaseAddress[slotNum];
    return 0;
  }

  uint64_t DeviceIntf::getFifoBaseAddress(xclPerfMonType type, uint32_t fifonum)
  {
    if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
        return mFifoCtrlBaseAddress;
    else
      return 0;
  }

  uint64_t DeviceIntf::getFifoReadBaseAddress(xclPerfMonType type, uint32_t fifonum)
  {
    if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
        return mFifoReadBaseAddress;
    else
      return 0;
  }

  uint64_t DeviceIntf::getTraceFunnelAddress(xclPerfMonType type)
  {
    if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
        return mTraceFunnelAddress;
    else
      return 0;
  }
  
  uint32_t DeviceIntf::getNumberSlots(xclPerfMonType type)
  {
    if (type == XCL_PERF_MON_MEMORY)
      return mMemoryProfilingNumberSlots;
    if (type == XCL_PERF_MON_ACCEL)
      return mAccelProfilingNumberSlots;
    if (type == XCL_PERF_MON_STALL)
      return mStallProfilingNumberSlots;
    if (type == XCL_PERF_MON_STR)
      return mStreamProfilingNumberSlots;

    if (type == XCL_PERF_MON_HOST) {
      uint32_t count = 0;
      for (unsigned int i=0; i < mMemoryProfilingNumberSlots; i++) {
        if (mProperties[i] & XSPM_HOST_PROPERTY_MASK) count++;
      }
      return count;
    }
    if (type == XCL_PERF_MON_SHELL) {
      uint32_t count = 0;
      for (unsigned int i=0; i < mMemoryProfilingNumberSlots; i++) {
        if (mProperties[i] & XSPM_HOST_PROPERTY_MASK) {
          std::string slotName = mSlotName[i];
          if (slotName.find("HOST") == std::string::npos)
            count++;
        }
      }
      return count;
    }

    return 0;
  }

  uint32_t DeviceIntf::getProperties(xclPerfMonType type, uint32_t slotnum)
  {
    if (type == XCL_PERF_MON_MEMORY && slotnum < XSPM_MAX_NUMBER_SLOTS)
      return static_cast<uint32_t>(mProperties[slotnum]);
    if (type == XCL_PERF_MON_STR && slotnum < XSSPM_MAX_NUMBER_SLOTS)
      return static_cast<uint32_t>(mStreammonProperties[slotnum]);
    return 0;
  }

  void DeviceIntf::getSlotName(xclPerfMonType type, uint32_t slotnum,
		                       char* slotName, uint32_t length)
  {
    std::string str = "";
    if (type == XCL_PERF_MON_MEMORY) {
      str = (slotnum < XSPM_MAX_NUMBER_SLOTS) ? mSlotName[slotnum] : "";
    }
    if (type == XCL_PERF_MON_ACCEL) {
      str = (slotnum < XSAM_MAX_NUMBER_SLOTS) ? mAccelMonSlotName[slotnum] : "";
    }
    if (type == XCL_PERF_MON_STR) {
      str = (slotnum < XSSPM_MAX_NUMBER_SLOTS) ? mStreamMonSlotName[slotnum] : "";
    }
    strncpy(slotName, str.c_str(), length);
  }

  // ***************************************************************************
  // Counters
  // ***************************************************************************

  // Start device counters performance monitoring
  size_t DeviceIntf::startCounters(xclPerfMonType type)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << type << ", Start device counters..." << std::endl;
    }

    // Update addresses for debug/profile IP
    //readDebugIpLayout();

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getNumberSlots(type);

    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getBaseAddress(type,i);
      
      // 1. Reset AXI - MM monitor metric counters
      size += read(baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);

      regValue = regValue | XSPM_CR_COUNTER_RESET_MASK;
      size += write(baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);

      regValue = regValue & ~(XSPM_CR_COUNTER_RESET_MASK);
      size += write(baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);

      // 2. Start AXI-MM monitor metric counters
      regValue = regValue | XSPM_CR_COUNTER_ENABLE_MASK;
      size += write(baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);

      // 3. Read from sample register to ensure total time is read again at end
      size += read(baseAddress + XSPM_SAMPLE_OFFSET, &regValue, 4);
    }
    return size;
  }

  // Stop both profile and trace performance monitoring
  size_t DeviceIntf::stopCounters(xclPerfMonType type) {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
          << type << ", Stop and reset device counters..." << std::endl;
    }

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getNumberSlots(type);

    for (uint32_t i=0; i < numSlots; i++) {
    baseAddress = getBaseAddress(type,i);

    // 1. Stop SPM metric counters
    size += read(baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);

    regValue = regValue & ~(XSPM_CR_COUNTER_ENABLE_MASK);
    size += write(baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);
    }
    return size;
  }

  // Read SPM performance counters
  size_t DeviceIntf::readCounters(xclPerfMonType type, xclCounterResults& counterResults) {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
      << ", " << type << ", " << &counterResults
      << ", Read device counters..." << std::endl;
    }

    // Initialize all values in struct to 0
    memset(&counterResults, 0, sizeof(xclCounterResults));

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;
    uint64_t baseAddress;
    uint32_t sampleInterval;
    uint32_t numSlots = 0;
    
    numSlots = getNumberSlots(XCL_PERF_MON_MEMORY);
    for (uint32_t s=0; s < numSlots; s++) {
      baseAddress = getBaseAddress(XCL_PERF_MON_MEMORY,s);
      // Read sample interval register
      // NOTE: this also latches the sampled metric counters
      size += read(baseAddress + XSPM_SAMPLE_OFFSET, &sampleInterval, 4);

      // Need to do this for every xilmon  
      if (s==0){
        counterResults.SampleIntervalUsec = sampleInterval / xclGetDeviceClockFreqMHz(mHandle);
      }

      size += read(baseAddress + XSPM_SAMPLE_WRITE_BYTES_OFFSET,
                   &counterResults.WriteBytes[s], 4);
      size += read(baseAddress + XSPM_SAMPLE_WRITE_TRANX_OFFSET,
                   &counterResults.WriteTranx[s], 4);
      size += read(baseAddress + XSPM_SAMPLE_WRITE_LATENCY_OFFSET,
                   &counterResults.WriteLatency[s], 4);
      size += read(baseAddress + XSPM_SAMPLE_READ_BYTES_OFFSET,
                   &counterResults.ReadBytes[s], 4);
      size += read(baseAddress + XSPM_SAMPLE_READ_TRANX_OFFSET,
                   &counterResults.ReadTranx[s], 4);
      size += read(baseAddress + XSPM_SAMPLE_READ_LATENCY_OFFSET,
                   &counterResults.ReadLatency[s], 4);

      // Read upper 32 bits (if available)
      if (mProperties[s] & XSPM_64BIT_PROPERTY_MASK) {
        uint64_t upper[6] = {};
        size += read(baseAddress + XSPM_SAMPLE_WRITE_BYTES_UPPER_OFFSET,
                     &upper[0], 4);
        size += read(baseAddress + XSPM_SAMPLE_WRITE_TRANX_UPPER_OFFSET,
                     &upper[1], 4);
        size += read(baseAddress + XSPM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET,
                     &upper[2], 4);
        size += read(baseAddress + XSPM_SAMPLE_READ_BYTES_UPPER_OFFSET,
                     &upper[3], 4);
        size += read(baseAddress + XSPM_SAMPLE_READ_TRANX_UPPER_OFFSET,
                     &upper[4], 4);
        size += read(baseAddress + XSPM_SAMPLE_READ_LATENCY_UPPER_OFFSET,
                     &upper[5], 4);

        counterResults.WriteBytes[s]   += (upper[0] << 32);
        counterResults.WriteTranx[s]   += (upper[1] << 32);
        counterResults.WriteLatency[s] += (upper[2] << 32);
        counterResults.ReadBytes[s]    += (upper[3] << 32);
        counterResults.ReadTranx[s]    += (upper[4] << 32);
        counterResults.ReadLatency[s]  += (upper[5] << 32);

        if (mVerbose) {
          std::cout << "SPM Upper 32, slot " << s << std::endl;
          std::cout << "  WriteBytes : " << upper[0] << std::endl;
          std::cout << "  WriteTranx : " << upper[1] << std::endl;
          std::cout << "  WriteLatency : " << upper[2] << std::endl;
          std::cout << "  ReadBytes : " << upper[3] << std::endl;
          std::cout << "  ReadTranx : " << upper[4] << std::endl;
          std::cout << "  ReadLatency : " << upper[5] << std::endl;
        }
      }

      if (mVerbose) {
        std::cout << "Reading SPM ...SlotNum : " << s << std::endl;
        std::cout << "Reading SPM ...WriteBytes : " << counterResults.WriteBytes[s] << std::endl;
        std::cout << "Reading SPM ...WriteTranx : " << counterResults.WriteTranx[s] << std::endl;
        std::cout << "Reading SPM ...WriteLatency : " << counterResults.WriteLatency[s] << std::endl;
        std::cout << "Reading SPM ...ReadBytes : " << counterResults.ReadBytes[s] << std::endl;
        std::cout << "Reading SPM ...ReadTranx : " << counterResults.ReadTranx[s] << std::endl;
        std::cout << "Reading SPM ...ReadLatency : " << counterResults.ReadLatency[s] << std::endl;
      }
    }

    /*
     * Read Accelerator Monitor Data
     */
    numSlots = getNumberSlots(XCL_PERF_MON_ACCEL);
    for (uint32_t s=0; s < numSlots; s++) {
      baseAddress = getBaseAddress(XCL_PERF_MON_ACCEL,s);
      uint32_t version = 0;
      size += read(baseAddress, &version, 4);
      if (mVerbose) {
        std::cout << "Accelerator Monitor Core Version : " << version << std::endl;
      }

      // Read sample interval register
      // NOTE: this also latches the sampled metric counters
      size += read(baseAddress + XSAM_SAMPLE_OFFSET,
                   &sampleInterval, 4);
      if (mVerbose) {
        std::cout << "Accelerator Monitor Sample Interval : " << sampleInterval << std::endl;
      }

      size += read(baseAddress + XSAM_ACCEL_EXECUTION_COUNT_OFFSET,
                   &counterResults.CuExecCount[s], 4);
      size += read(baseAddress + XSAM_ACCEL_EXECUTION_CYCLES_OFFSET,
                   &counterResults.CuExecCycles[s], 4);
      size += read(baseAddress + XSAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET,
                   &counterResults.CuMinExecCycles[s], 4);
      size += read(baseAddress + XSAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET,
                   &counterResults.CuMaxExecCycles[s], 4);

      // Read upper 32 bits (if available)
      if (mAccelmonProperties[s] & XSAM_64BIT_PROPERTY_MASK) {
        uint64_t upper[4] = {};
        size += read(baseAddress + XSAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET,
                     &upper[0], 4);
        size += read(baseAddress + XSAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET,
                     &upper[1], 4);
        size += read(baseAddress + XSAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET,
                     &upper[2], 4);
        size += read(baseAddress + XSAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET,
                     &upper[3], 4);

        counterResults.CuExecCount[s]     += (upper[0] << 32);
        counterResults.CuExecCycles[s]    += (upper[1] << 32);
        counterResults.CuMinExecCycles[s] += (upper[2] << 32);
        counterResults.CuMaxExecCycles[s] += (upper[3] << 32);

        if (mVerbose) {
          std::cout << "Accelerator Monitor Upper 32, slot " << s << std::endl;
          std::cout << "  CuExecCount : " << upper[0] << std::endl;
          std::cout << "  CuExecCycles : " << upper[1] << std::endl;
          std::cout << "  CuMinExecCycles : " << upper[2] << std::endl;
          std::cout << "  CuMaxExecCycles : " << upper[3] << std::endl;
        }
      }

      if (mVerbose) {
        std::cout << "Reading Accelerator Monitor... SlotNum : " << s << std::endl;
        std::cout << "Reading Accelerator Monitor... CuExecCount : " << counterResults.CuExecCount[s] << std::endl;
        std::cout << "Reading Accelerator Monitor... CuExecCycles : " << counterResults.CuExecCycles[s] << std::endl;
        std::cout << "Reading Accelerator Monitor... CuMinExecCycles : " << counterResults.CuMinExecCycles[s] << std::endl;
        std::cout << "Reading Accelerator Monitor... CuMaxExecCycles : " << counterResults.CuMaxExecCycles[s] << std::endl;
      }

      // Check Stall bit
      if (mAccelmonProperties[s] & XSAM_STALL_PROPERTY_MASK) {
        size += read(baseAddress + XSAM_ACCEL_STALL_INT_OFFSET,
                     &counterResults.CuStallIntCycles[s], 4);
        size += read(baseAddress + XSAM_ACCEL_STALL_STR_OFFSET,
                     &counterResults.CuStallStrCycles[s], 4);
        size += read(baseAddress + XSAM_ACCEL_STALL_EXT_OFFSET,
                     &counterResults.CuStallExtCycles[s], 4);
        if (mVerbose) {
          std::cout << "Stall Counters enabled : " << std::endl;
          std::cout << "Reading Accelerator Monitor... CuStallIntCycles : " << counterResults.CuStallIntCycles[s] << std::endl;
          std::cout << "Reading Accelerator Monitor... CuStallStrCycles : " << counterResults.CuStallStrCycles[s] << std::endl;
          std::cout << "Reading Accelerator Monitor... CuStallExtCycles : " << counterResults.CuStallExtCycles[s] << std::endl;
        }
      }
    }
    /*
     * Read AXI Stream Monitor Data
     */
    if (mVerbose) {
        std::cout << "Reading AXI Stream Monitors.." << std::endl;
    }
    numSlots = getNumberSlots(XCL_PERF_MON_STR);
    for (uint32_t s=0; s < numSlots; s++) {
      baseAddress = getBaseAddress(XCL_PERF_MON_STR,s);
      // Sample Register
      size += read(baseAddress + XSSPM_SAMPLE_OFFSET,
                   &sampleInterval, 4);
      size += read(baseAddress + XSSPM_NUM_TRANX_OFFSET,
                   &counterResults.StrNumTranx[s], 8);
      size += read(baseAddress + XSSPM_DATA_BYTES_OFFSET,
                   &counterResults.StrDataBytes[s], 8);
      size += read(baseAddress + XSSPM_BUSY_CYCLES_OFFSET,
                   &counterResults.StrBusyCycles[s], 8);
      size += read(baseAddress + XSSPM_STALL_CYCLES_OFFSET,
                   &counterResults.StrStallCycles[s], 8);
      size += read(baseAddress + XSSPM_STARVE_CYCLES_OFFSET,
                   &counterResults.StrStarveCycles[s], 8);
      if (mVerbose) {
        std::cout << "Reading AXI Stream Monitor... SlotNum : " << s << std::endl;
        std::cout << "Reading AXI Stream Monitor... NumTranx : " << counterResults.StrNumTranx[s] << std::endl;
        std::cout << "Reading AXI Stream Monitor... DataBytes : " << counterResults.StrDataBytes[s] << std::endl;
        std::cout << "Reading AXI Stream Monitor... BusyCycles : " << counterResults.StrBusyCycles[s] << std::endl;
        std::cout << "Reading AXI Stream Monitor... StallCycles : " << counterResults.StrStallCycles[s] << std::endl;
        std::cout << "Reading AXI Stream Monitor... StarveCycles : " << counterResults.StrStarveCycles[s] << std::endl;
      }
    }
    return size;
  }

  // ***************************************************************************
  // Timeline Trace
  // ***************************************************************************

  // Reset trace AXI stream FIFO
  size_t DeviceIntf::resetTraceFifo(xclPerfMonType type)
  {
    uint64_t resetCoreAddress = getFifoBaseAddress(type, 0) + AXI_FIFO_SRR;
    uint64_t resetFifoAddress = getFifoBaseAddress(type, 0) + AXI_FIFO_RDFR;
    size_t size = 0;
    uint32_t regValue = AXI_FIFO_RESET_VALUE;

    size += write(resetCoreAddress, &regValue, 4);
    size += write(resetFifoAddress, &regValue, 4);
    return size;
  }

  // Clock training used in converting device trace timestamps to host domain
  size_t DeviceIntf::clockTraining(xclPerfMonType type)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << type << ", Send clock training..." << std::endl;
    }
    // This will be enabled later. We're snapping first event to start of cu.
    return 1;
  }

  // Start trace performance monitoring
  size_t DeviceIntf::startTrace(xclPerfMonType type, uint32_t startTrigger)
  {
    // StartTrigger Bits:
    // Bit 0: Trace Coarse/Fine     Bit 1: Transfer Trace Ctrl
    // Bit 2: CU Trace Ctrl         Bit 3: INT Trace Ctrl
    // Bit 4: Str Trace Ctrl        Bit 5: Ext Trace Ctrl
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << type << ", " << startTrigger
                << ", Start device tracing..." << std::endl;
    }
    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getNumberSlots(XCL_PERF_MON_MEMORY);

    // Update addresses for debug/profile IP
    //readDebugIpLayout();

    if (!mIsDeviceProfiling)
   	  return 0;

    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getBaseAddress(XCL_PERF_MON_MEMORY,i);
      // Set SPM trace ctrl register bits
      regValue = startTrigger & XSPM_TRACE_CTRL_MASK;
      size += write(baseAddress + XSPM_TRACE_CTRL_OFFSET, &regValue, 4);
    }

    numSlots = getNumberSlots(XCL_PERF_MON_ACCEL);
    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getBaseAddress(XCL_PERF_MON_ACCEL,i);
      // Set Stall trace control register bits
      // Bit 1 : CU (Always ON)  Bit 2 : INT  Bit 3 : STR  Bit 4 : Ext 
      regValue = ((startTrigger & XSAM_TRACE_STALL_SELECT_MASK) >> 1) | 0x1 ;
      size += write(baseAddress + XSAM_TRACE_CTRL_OFFSET, &regValue, 4);
    }

    getTraceCount(type);
    size += resetTraceFifo(type);
    getTraceCount(type);

    for (uint32_t i = 0; i < 2; i++) {
      baseAddress = getTraceFunnelAddress(XCL_PERF_MON_MEMORY);
      uint64_t timeStamp = getHostTraceTimeNsec();
      regValue = static_cast <uint32_t> (timeStamp & 0xFFFF);
      size += write(baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 16 & 0xFFFF);
      size += write(baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 32 & 0xFFFF); 
      size += write(baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 48 & 0xFFFF);
      size += write(baseAddress, &regValue, 4);
      usleep(10);
    }
    return size;
  }

  // Stop trace performance monitoring
  size_t DeviceIntf::stopTrace(xclPerfMonType type)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << type << ", Stop and reset device tracing..." << std::endl;
    }

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;
    getTraceCount(type);
    size += resetTraceFifo(type);
    return size;
  }

  // Get trace word count
  uint32_t DeviceIntf::getTraceCount(xclPerfMonType type) {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << type << std::endl;
    }

    uint64_t fifoBaseAddress = getFifoBaseAddress(type, 0);

    if (!mIsDeviceProfiling || !fifoBaseAddress)
   	  return 0;

    //xclAddressSpace addressSpace = (type == XCL_PERF_MON_ACCEL) ?
    //    XCL_ADDR_KERNEL_CTRL : XCL_ADDR_SPACE_DEVICE_PERFMON;

    uint32_t fifoCount = 0;
    uint32_t numSamples = 0;
    uint32_t numBytes = 0;
    read(fifoBaseAddress + AXI_FIFO_RLR, &fifoCount, 4);
    // Read bits 22:0 per AXI-Stream FIFO product guide (PG080, 10/1/14)
    numBytes = fifoCount & 0x7FFFFF;
    numSamples = numBytes / (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH/8);

    if (mVerbose) {
      std::cout << "  No. of trace samples = " << std::dec << numSamples
                << " (fifoCount = 0x" << std::hex << fifoCount << ")" << std::dec << std::endl;
    }

    return numSamples;
  }

  // Read all values from APM trace AXI stream FIFOs
  size_t DeviceIntf::readTrace(xclPerfMonType type, xclTraceResultsVector& traceVector)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << type << ", " << &traceVector
                << ", Reading device trace stream..." << std::endl;
    }

    traceVector.mLength = 0;
    if (!mIsDeviceProfiling)
   	  return 0;

    uint32_t numSamples = getTraceCount(type);
    if (numSamples == 0)
      return 0;

    uint64_t fifoReadAddress[] = {0, 0, 0};
    if (type == XCL_PERF_MON_MEMORY) {
      fifoReadAddress[0] = getFifoReadBaseAddress(type, 0) + AXI_FIFO_RDFD_AXI_FULL;
    }
    else {
      for (int i=0; i < 3; i++)
        fifoReadAddress[i] = getFifoReadBaseAddress(type, i) + AXI_FIFO_RDFD;
    }

    size_t size = 0;

    // Limit to max number of samples so we don't overrun trace buffer on host
    uint32_t maxSamples = getMaxSamples(type);
    numSamples = (numSamples > maxSamples) ? maxSamples : numSamples;
    traceVector.mLength = numSamples;

    const uint32_t bytesPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 8);
    const uint32_t wordsPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 32);
    //uint32_t numBytes = numSamples * bytesPerSample;
    uint32_t numWords = numSamples * wordsPerSample;

    // Create trace buffer on host (requires alignment)
    const int BUFFER_BYTES = MAX_TRACE_NUMBER_SAMPLES * bytesPerSample;
    const int BUFFER_WORDS = MAX_TRACE_NUMBER_SAMPLES * wordsPerSample;
#ifndef _WINDOWS
// TODO: Windows build support
//    alignas is defined in c++11
//#if GCC_VERSION >= 40800
    // Alignment is limited to 16 by PPC64LE
    //alignas(AXI_FIFO_RDFD_AXI_FULL) uint32_t hostbuf[BUFFER_WORDS];
    alignas(16) uint32_t hostbuf[BUFFER_WORDS];
//#else
//    AlignedAllocator<uint32_t> alignedBuffer(AXI_FIFO_RDFD_AXI_FULL, BUFFER_WORDS);
//    uint32_t* hostbuf = alignedBuffer.getBuffer();
//#endif
#else
    uint32_t hostbuf[BUFFER_WORDS];
#endif

    // ******************************
    // Read all words from trace FIFO
    // ******************************
    if (type == XCL_PERF_MON_MEMORY) {
      memset((void *)hostbuf, 0, BUFFER_BYTES);

      // Iterate over chunks
      // NOTE: AXI limits this to 4K bytes per transfer
      uint32_t chunkSizeWords = 256 * wordsPerSample;
      if (chunkSizeWords > 1024) chunkSizeWords = 1024;
      uint32_t chunkSizeBytes = 4 * chunkSizeWords;
      uint32_t words=0;

      // Read trace a chunk of bytes at a time
      if (numWords > chunkSizeWords) {
        for (; words < (numWords-chunkSizeWords); words += chunkSizeWords) {
          if (mVerbose) {
            std::cout << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
                << std::hex << fifoReadAddress[0] << " and writing it to 0x"
                << (void *)(hostbuf + words) << std::dec << std::endl;
          }

          if (traceRead((void *)(hostbuf + words), chunkSizeBytes,  fifoReadAddress[0]) < 0)
            return 0;

          size += chunkSizeBytes;
        }
      }

      // Read remainder of trace not divisible by chunk size
      if (words < numWords) {
        chunkSizeBytes = 4 * (numWords - words);

        if (mVerbose) {
          std::cout << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
              << std::hex << fifoReadAddress[0] << " and writing it to 0x"
              << (void *)(hostbuf + words) << std::dec << std::endl;
        }

        if (traceRead((void *)(hostbuf + words), chunkSizeBytes,  fifoReadAddress[0]) < 0)
            return 0;

        size += chunkSizeBytes;
      }

      if (mVerbose) {
        std::cout << __func__ << ": done reading " << size << " bytes " << std::endl;
      }
    }

    // ******************************
    // Read & process all trace FIFOs
    // ******************************
    xclTraceResults results = {};
    for (uint32_t wordnum=0; wordnum < numSamples; wordnum++) {
      uint32_t index = wordsPerSample * wordnum;
      uint64_t temp = 0;

      temp = *(hostbuf + index) | (uint64_t)*(hostbuf + index + 1) << 32;
      if (!temp)
        continue;

      // This section assumes that we write 8 timestamp packets in startTrace
      int mod = (wordnum % 4);
      unsigned int clockWordIndex = 7;
      if (wordnum > clockWordIndex || mod == 0) {
        memset(&results, 0, sizeof(xclTraceResults));
      }
      if (wordnum <= clockWordIndex) {
        if (mod == 0) {
          results.Timestamp = temp & 0x1FFFFFFFFFFF;
        }
        uint64_t partial = (((temp >> 45) & 0xFFFF) << (16 * mod));
        results.HostTimestamp = results.HostTimestamp | partial;
        if (mVerbose) {
          std::cout << "Updated partial host timestamp : " << std::hex << partial << std::endl;
        }
        if (mod == 3) {
          if (mVerbose) {
            std::cout << "  Trace sample " << std::dec << wordnum << ": ";
            std::cout << " Timestamp : " << results.Timestamp << "   ";
            std::cout << " Host Timestamp : " << std::hex << results.HostTimestamp << std::endl;
          }
          traceVector.mArray[static_cast<int>(wordnum/4)] = results;
        }
        continue;
      }

      // Zynq Packet Format
      results.Timestamp = temp & 0x1FFFFFFFFFFF;
      results.EventType = ((temp >> 45) & 0xF) ? XCL_PERF_MON_END_EVENT : 
          XCL_PERF_MON_START_EVENT;
      results.TraceID = (temp >> 49) & 0xFFF;
      results.Reserved = (temp >> 61) & 0x1;
      results.Overflow = (temp >> 62) & 0x1;
      results.Error = (temp >> 63) & 0x1;
      results.EventID = XCL_PERF_MON_HW_EVENT;
      results.EventFlags = ((temp >> 45) & 0xF) | ((temp >> 57) & 0x10) ;
      traceVector.mArray[wordnum - clockWordIndex + 1] = results;

      if (mVerbose) {
        std::cout << "  Trace sample " << std::dec << wordnum << ": ";
        std::cout << dec2bin(uint32_t(temp>>32)) << " " << dec2bin(uint32_t(temp&0xFFFFFFFF));
        std::cout << std::endl;
        std::cout << " Timestamp : " << results.Timestamp << "   ";
        std::cout << "Event Type : " << results.EventType << "   ";
        std::cout << "slotID : " << results.TraceID << "   ";
        std::cout << "Start, Stop : " << static_cast<int>(results.Reserved) << "   ";
        std::cout << "Overflow : " << static_cast<int>(results.Overflow) << "   ";
        std::cout << "Error : " << static_cast<int>(results.Error) << "   ";
        std::cout << "EventFlags : " << static_cast<int>(results.EventFlags) << "   ";
        std::cout << std::endl;
      }
    }

    return size;
  }

} // namespace xdp
