/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s): Jason Villarreal
 *          : Paul Schumacher
 * ZNYQ HAL Driver profiling functionality
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

#include <cassert>
#include <cstring>
#include <unistd.h>
#include <chrono>

#include "shim.h"
#include "shim-profile.h"

#include "xclperf.h"
#include "driver/zynq/include/zynq_perfmon_params.h"

namespace ZYNQ {
  
  // Returns -1 if Version1 > Version2
  // Returns 0 if Version1 == Version2
  // Returns 1 if Version1 < Version2
  int ZYNQShimProfiling::cmpMonVersions(unsigned major1, unsigned minor1,
					unsigned major2, unsigned minor2)
  {
    if (major1 > major2) return -1 ;
    if (major1 < major2) return  1 ;
    // major1 == major2
    if (minor1 > minor2) return -1 ;
    if (minor1 < minor2) return  1 ;
    // major1.minor1 == major2.minor2
    return 0 ;
  }

  ZYNQShimProfiling::ZYNQShimProfiling(ZYNQShim* s) : 
    shim(s), 
    mMemoryProfilingNumberSlots(0), 
    mAccelProfilingNumberSlots(0), 
    mStallProfilingNumberSlots(0), 
    mStreamProfilingNumberSlots(0)
  {
    assert(shim != nullptr) ;
  }

  ZYNQShimProfiling::~ZYNQShimProfiling()
  {
    ;
  }

  double ZYNQShimProfiling::xclGetDeviceClockFreqMHz()
  {
    xclDeviceInfo2  deviceInfo ;
    shim->xclGetDeviceInfo2(&deviceInfo) ;
    unsigned clockFreq = deviceInfo.mOCLFrequency[0] ;
    if (clockFreq == 0) clockFreq = 100 ;
    
    return (double)clockFreq ; 
  }

  uint32_t ZYNQShimProfiling::getProfilingNumberSlots(xclPerfMonType type)
  {
    switch (type)
    {
    case XCL_PERF_MON_MEMORY: return mMemoryProfilingNumberSlots ;
    case XCL_PERF_MON_ACCEL:  return mAccelProfilingNumberSlots ;
    case XCL_PERF_MON_STALL:  return mStallProfilingNumberSlots ;
    case XCL_PERF_MON_HOST:   return 0 ;
    case XCL_PERF_MON_STR:    return mStreamProfilingNumberSlots ;
    default:                  return 0 ;
    }
  }

  void ZYNQShimProfiling::getProfilingSlotName(xclPerfMonType type, uint32_t slotnum, char* slotName, uint32_t length)
  {
    std::string str = "" ;
    switch (type)
    {
    case XCL_PERF_MON_MEMORY:
      str = (slotnum < XSPM_MAX_NUMBER_SLOTS) ? mPerfMonSlotName[slotnum] : "";
      break ;
    case XCL_PERF_MON_ACCEL:
      str = (slotnum < XSAM_MAX_NUMBER_SLOTS) ? mAccelMonSlotName[slotnum] : "";
      break ;
    case XCL_PERF_MON_STR:
      str = (slotnum < XSSPM_MAX_NUMBER_SLOTS) ? mStreamMonSlotName[slotnum] : "";
      break ;
    default:
      str = "" ;
      break ;
    }
    
    strncpy(slotName, str.c_str(), length);
  }
  
  size_t ZYNQShimProfiling::xclPerfMonStartCounters(xclPerfMonType type)
  {
    // Update addresses for debug/profile IP
    readDebugIpLayout() ;
  
    if (!mIsDeviceProfiling) return 0 ;

    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getProfilingNumberSlots(type);

    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(type,i);

      // 1. Reset AXI - MM monitor metric counters
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);
      
      regValue = regValue | XSPM_CR_COUNTER_RESET_MASK;
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);
      
      regValue = regValue & ~(XSPM_CR_COUNTER_RESET_MASK);
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);
      
      // 2. Start AXI-MM monitor metric counters
      regValue = regValue | XSPM_CR_COUNTER_ENABLE_MASK;
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);
      
      // 3. Read from sample register to ensure total time is read again at end
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_SAMPLE_OFFSET, &regValue, 4);
      
    }
    return size;
  }
  
  size_t ZYNQShimProfiling::xclPerfMonStopCounters(xclPerfMonType type)
  {
    if (!mIsDeviceProfiling)
      return 0;

    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getProfilingNumberSlots(type);

    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(type,i);

      // 1. Stop SPM metric counters
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);

      regValue = regValue & ~(XSPM_CR_COUNTER_ENABLE_MASK);
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_CONTROL_OFFSET, &regValue, 4);
    }
    return size;
  }
  
  size_t ZYNQShimProfiling::xclPerfMonReadCounters(xclPerfMonType type, xclCounterResults& counterResults)
  {
    // Initialize all values in struct to 0
    memset(&counterResults, 0, sizeof(xclCounterResults));
    
    if (!mIsDeviceProfiling) return 0;
    
    size_t size = 0;

    size += readSPMRegisters(counterResults) ;
    size += readSAMRegisters(counterResults) ;
    size += readSSPMRegisters(counterResults) ;

    return size;
}

  size_t ZYNQShimProfiling::readSPMRegisters(xclCounterResults& counterResults)
  {
    size_t size = 0 ;
    uint64_t baseAddress;
    uint32_t sampleInterval = 0 ;

    uint32_t numSlots = getProfilingNumberSlots(XCL_PERF_MON_MEMORY);
    for (uint32_t s=0; s < numSlots; s++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_MEMORY,s);
      
      // Read sample interval register
      // NOTE: this also latches the sampled metric counters
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSPM_SAMPLE_OFFSET, 
			    &sampleInterval, 4);
      
      // Need to do this for every xilmon  
      if (s==0){
	counterResults.SampleIntervalUsec = sampleInterval / xclGetDeviceClockFreqMHz();
      }
      
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSPM_SAMPLE_WRITE_BYTES_OFFSET, 
			    &counterResults.WriteBytes[s], 4); 
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSPM_SAMPLE_WRITE_TRANX_OFFSET, 
			    &counterResults.WriteTranx[s], 4);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSPM_SAMPLE_WRITE_LATENCY_OFFSET, 
			    &counterResults.WriteLatency[s], 4);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSPM_SAMPLE_READ_BYTES_OFFSET, 
			    &counterResults.ReadBytes[s], 4);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSPM_SAMPLE_READ_TRANX_OFFSET, 
			    &counterResults.ReadTranx[s], 4);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSPM_SAMPLE_READ_LATENCY_OFFSET, 
			    &counterResults.ReadLatency[s], 4);

      // Read upper 32 bits (if available)
      if (mPerfmonProperties[s] & XSPM_64BIT_PROPERTY_MASK) {
	uint64_t upper[6] = {};
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSPM_SAMPLE_WRITE_BYTES_UPPER_OFFSET,
			      &upper[0], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSPM_SAMPLE_WRITE_TRANX_UPPER_OFFSET,
			      &upper[1], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSPM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET,
			      &upper[2], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSPM_SAMPLE_READ_BYTES_UPPER_OFFSET,
			      &upper[3], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSPM_SAMPLE_READ_TRANX_UPPER_OFFSET,
			      &upper[4], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSPM_SAMPLE_READ_LATENCY_UPPER_OFFSET,
			      &upper[5], 4);
	
	counterResults.WriteBytes[s]   += (upper[0] << 32);
	counterResults.WriteTranx[s]   += (upper[1] << 32);
	counterResults.WriteLatency[s] += (upper[2] << 32);
	counterResults.ReadBytes[s]    += (upper[3] << 32);
	counterResults.ReadTranx[s]    += (upper[4] << 32);
	counterResults.ReadLatency[s]  += (upper[5] << 32);
      }
    }

    return size ;
  }

  size_t ZYNQShimProfiling::readSAMRegisters(xclCounterResults& counterResults)
  {
    size_t size = 0 ;
    uint32_t numSlots = getProfilingNumberSlots(XCL_PERF_MON_ACCEL);
    uint64_t baseAddress ;
    uint32_t sampleInterval = 0 ;

    for (uint32_t s=0; s < numSlots; ++s) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_ACCEL,s);
      uint32_t version = 0;
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress, 
			    &version, 4);

      // Read sample interval register
      // NOTE: this also latches the sampled metric counters
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSAM_SAMPLE_OFFSET, 
			    &sampleInterval, 4);

      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSAM_ACCEL_EXECUTION_COUNT_OFFSET, 
			    &counterResults.CuExecCount[s], 4); 
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSAM_ACCEL_EXECUTION_CYCLES_OFFSET, 
			    &counterResults.CuExecCycles[s], 4);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET, 
			    &counterResults.CuMinExecCycles[s], 4);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			    baseAddress + XSAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET, 
			    &counterResults.CuMaxExecCycles[s], 4);
      
      // Read upper 32 bits (if available)
      if (mAccelmonProperties[s] & XSAM_64BIT_PROPERTY_MASK) {
	uint64_t upper[4] = {};
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET,
			      &upper[0], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET,
			      &upper[1], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET,
			      &upper[2], 4);
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			      baseAddress + XSAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET,
			      &upper[3], 4);
	
	counterResults.CuExecCount[s]     += (upper[0] << 32);
	counterResults.CuExecCycles[s]    += (upper[1] << 32);
	counterResults.CuMinExecCycles[s] += (upper[2] << 32);
	counterResults.CuMaxExecCycles[s] += (upper[3] << 32);
      }
      
      // Check Stall bit
      if (mAccelmonProperties[s] & XSAM_STALL_PROPERTY_MASK) {
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			      baseAddress + XSAM_ACCEL_STALL_INT_OFFSET, 
			      &counterResults.CuStallIntCycles[s], 4); 
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			      baseAddress + XSAM_ACCEL_STALL_STR_OFFSET, 
			      &counterResults.CuStallStrCycles[s], 4); 
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, 
			      baseAddress + XSAM_ACCEL_STALL_EXT_OFFSET, 
			      &counterResults.CuStallExtCycles[s], 4);
      }

      // Accelerator monitor versions > 1.1 support dataflow
      if (cmpMonVersions(mAccelmonMajorVersions[s], mAccelmonMinorVersions[s], 1, 1) < 0)
      {	
	size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XSAM_BUSY_CYCLES_OFFSET,
                        &counterResults.CuBusyCycles[s], 4);
        size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XSAM_MAX_PARALLEL_ITER_OFFSET,
                        &counterResults.CuMaxParallelIter[s], 4);
	if (mAccelmonProperties[s] & XSAM_64BIT_PROPERTY_MASK) {
	  uint64_t upper[2] = {0};
          size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			  baseAddress + XSAM_BUSY_CYCLES_UPPER_OFFSET,
			  &upper[0], 4);
          size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			  baseAddress + XSAM_MAX_PARALLEL_ITER_UPPER_OFFSET,
			  &upper[1], 4);
          counterResults.CuBusyCycles[s]      += (upper[0] << 32);
          counterResults.CuMaxParallelIter[s] += (upper[1] << 32);
	}
      }
      else
      {
	counterResults.CuBusyCycles[s] = counterResults.CuExecCycles[s];
	counterResults.CuMaxParallelIter[s] = 1;
      }
    }
    
    return size ;
  }

  size_t ZYNQShimProfiling::readSSPMRegisters(xclCounterResults& counterResults)
  {
    size_t size = 0 ;
    uint32_t numSlots = getProfilingNumberSlots(XCL_PERF_MON_STR);
    uint64_t baseAddress;
    uint32_t sampleInterval = 0 ;

    for (uint32_t s=0; s < numSlots; s++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_STR,s);
      // Sample Register
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			    baseAddress + XSSPM_SAMPLE_OFFSET, 
			    &sampleInterval, 4);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			    baseAddress + XSSPM_NUM_TRANX_OFFSET, 
			    &counterResults.StrNumTranx[s], 8);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			    baseAddress + XSSPM_DATA_BYTES_OFFSET, 
			    &counterResults.StrDataBytes[s], 8);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			    baseAddress + XSSPM_BUSY_CYCLES_OFFSET, 
			    &counterResults.StrBusyCycles[s], 8);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			    baseAddress + XSSPM_STALL_CYCLES_OFFSET, 
			    &counterResults.StrStallCycles[s], 8);
      size += shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
			    baseAddress + XSSPM_STARVE_CYCLES_OFFSET, 
			    &counterResults.StrStarveCycles[s], 8);
    }
    return size;
  }

  size_t ZYNQShimProfiling::xclPerfMonStartTrace(xclPerfMonType type, uint32_t startTrigger)
  {
    // Start Trigger bits:
    //  Bit 0: Trace Coarse/Fine
    //  Bit 1: Transfer Trace Ctrl
    //  Bit 2: CU Trace Ctrl
    //  Bit 3: INT Trace Ctrl
    //  Bit 4: Str Trace Ctrl
    //  Bit 5: Ext Trace Ctrl
    
    size_t size = 0 ;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getProfilingNumberSlots(XCL_PERF_MON_MEMORY);

    // Update addresses for debug/profile IP
    readDebugIpLayout();
    if (!mIsDeviceProfiling)
   	  return 0;

    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_MEMORY,i);
      // Set SPM trace ctrl register bits
      regValue = startTrigger & XSPM_TRACE_CTRL_MASK;
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSPM_TRACE_CTRL_OFFSET, &regValue, 4);
    }

    numSlots = getProfilingNumberSlots(XCL_PERF_MON_ACCEL);
    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_ACCEL,i);
      // Set Stall trace control register bits
      // Bit 1 : CU (Always ON)  Bit 2 : INT  Bit 3 : STR  Bit 4 : Ext 
      regValue = ((startTrigger & XSAM_TRACE_STALL_SELECT_MASK) >> 1) | 0x1 ;
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XSAM_TRACE_CTRL_OFFSET, &regValue, 4);
    }

    xclPerfMonGetTraceCount(type);
    size += resetFifos(type);
    xclPerfMonGetTraceCount(type);

    for (uint32_t i = 0; i < 2; i++) {
      baseAddress = getTraceFunnelAddress(XCL_PERF_MON_MEMORY);
      uint64_t timeStamp = getHostTraceTimeNsec();
      regValue = static_cast <uint32_t> (timeStamp & 0xFFFF);
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 16 & 0xFFFF);
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 32 & 0xFFFF); 
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 48 & 0xFFFF);
      size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      usleep(10);
    }
    return size;
  }

  size_t ZYNQShimProfiling::xclPerfMonStopTrace(xclPerfMonType type)
  {
    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;
    xclPerfMonGetTraceCount(type);
    size += resetFifos(type);
    return size;
  }

  uint32_t ZYNQShimProfiling::xclPerfMonGetTraceCount(xclPerfMonType type)
  {
    uint64_t fifoBaseAddress = getPerfMonFifoBaseAddress(type, 0);

    if (!mIsDeviceProfiling || !fifoBaseAddress)
   	  return 0;

    xclAddressSpace addressSpace = (type == XCL_PERF_MON_ACCEL) ?
        XCL_ADDR_KERNEL_CTRL : XCL_ADDR_SPACE_DEVICE_PERFMON;

    uint32_t fifoCount = 0;
    uint32_t numSamples = 0;
    uint32_t numBytes = 0;
    shim->xclRead(addressSpace, fifoBaseAddress + AXI_FIFO_RLR, &fifoCount, 4);
    // Read bits 22:0 per AXI-Stream FIFO product guide (PG080, 10/1/14)
    numBytes = fifoCount & 0x7FFFFF;
    numSamples = numBytes / (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH/8);

    return numSamples;
  }

  size_t ZYNQShimProfiling::xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector)
  {
    traceVector.mLength = 0;
    if (!mIsDeviceProfiling)
   	  return 0;

    uint32_t numSamples = xclPerfMonGetTraceCount(type);
    if (numSamples == 0)
    {
      return 0;
    }

    // On Zynq platforms, we cannot use unmanaged reads, but instead
    //  must use xclRead and repeatedly read out the samples from
    //  the same register.

    uint64_t readAddress = mPerfMonFifoReadBaseAddress + 0x1000;
    size_t size = 0;

    // Limit to max number of samples so we don't overrun trace buffer on host
    uint32_t maxSamples = getPerfMonNumberSamples(type);
    numSamples = (numSamples > maxSamples) ? maxSamples : numSamples;
    traceVector.mLength = numSamples;

    // Read all of the contents of the trace FIFO into local memory
    uint64_t fifoContents[numSamples] ;

    for (uint32_t i = 0 ; i < numSamples ; ++i)
    {
      // For each sample, we will need to read two 32-bit values and
      //  assemble them together.
      uint32_t lowOrder = 0 ;
      uint32_t highOrder = 0 ;
      shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, readAddress, &lowOrder, sizeof(uint32_t));
      shim->xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, readAddress, &highOrder, sizeof(uint32_t));

      fifoContents[i] = ((uint64_t)(highOrder) << 32) | (uint64_t)(lowOrder) ;
    }

    // Process all of the contents of the trace FIFO (now in local memory)
    static unsigned long long firstTimestamp ;
    xclTraceResults results = {};    
    for(uint32_t i = 0 ; i < numSamples ; ++i)
    {
      uint64_t currentSample = fifoContents[i] ;
      if (currentSample == 0)
	continue ;
      
      if (i == 0)
	firstTimestamp = currentSample & 0x1FFFFFFFFFFF;

      // This section assumes that we write 8 timestamp packets in startTrace
      int mod = (i % 4);
      unsigned int clockWordIndex = 7;
      if (i > clockWordIndex || mod == 0) {
        memset(&results, 0, sizeof(xclTraceResults));
      }
      if (i <= clockWordIndex) {
        if (mod == 0) {
          uint64_t currentTimestamp = currentSample & 0x1FFFFFFFFFFF;
          if (currentTimestamp >= firstTimestamp)
            results.Timestamp = currentTimestamp - firstTimestamp;
          else
            results.Timestamp = currentTimestamp + (0x1FFFFFFFFFFF - firstTimestamp);
        }
        uint64_t partial = (((currentSample >> 45) & 0xFFFF) << (16 * mod));
        results.HostTimestamp = results.HostTimestamp | partial;
        if (mod == 3) {
          traceVector.mArray[static_cast<int>(i/4)] = results;
        }
        continue;
      }

      results.Timestamp = (currentSample & 0x1FFFFFFFFFFF) - firstTimestamp;
      results.EventType = ((currentSample >> 45) & 0xF) ? XCL_PERF_MON_END_EVENT : 
          XCL_PERF_MON_START_EVENT;
      results.TraceID = (currentSample >> 49) & 0xFFF;
      results.Reserved = (currentSample >> 61) & 0x1;
      results.Overflow = (currentSample >> 62) & 0x1;
      results.Error = (currentSample >> 63) & 0x1;
      results.EventID = XCL_PERF_MON_HW_EVENT;
      results.EventFlags = ((currentSample >> 45) & 0xF) | ((currentSample >> 57) & 0x10) ;
      traceVector.mArray[i - clockWordIndex + 1] = results;
      
    }
    return size;
  }

  uint64_t ZYNQShimProfiling::getTraceFunnelAddress(xclPerfMonType type)
  {
    if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
      return mTraceFunnelAddress;
    else
      return 0 ;
  }

  size_t ZYNQShimProfiling::resetFifos(xclPerfMonType type)
  {
     uint64_t resetCoreAddress = getPerfMonFifoBaseAddress(type, 0) + AXI_FIFO_SRR;
    uint64_t resetFifoAddress = getPerfMonFifoBaseAddress(type, 0) + AXI_FIFO_RDFR;
    size_t size = 0;
    uint32_t regValue = AXI_FIFO_RESET_VALUE;

    size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, resetCoreAddress, &regValue, 4);
    size += shim->xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, resetFifoAddress, &regValue, 4);
    return size;
  }

  uint64_t ZYNQShimProfiling::getPerfMonFifoBaseAddress(xclPerfMonType type, uint32_t fifonum)
  {
    if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
      return mPerfMonFifoCtrlBaseAddress;
    else
      return 0;
  }

  uint32_t ZYNQShimProfiling::getPerfMonNumberSamples(xclPerfMonType type)
  {
    switch (type)
    {
    case XCL_PERF_MON_MEMORY: return XPAR_AXI_PERF_MON_0_TRACE_NUMBER_SAMPLES;
    case XCL_PERF_MON_HOST:   return XPAR_AXI_PERF_MON_1_TRACE_NUMBER_SAMPLES;
    case XCL_PERF_MON_ACCEL:  return XPAR_AXI_PERF_MON_2_TRACE_NUMBER_SAMPLES;
    default:
      return 0 ;
    }
  }

  // Get host timestamp to write to APM
  // IMPORTANT NOTE: this *must* be compatible with the method of generating
  // timestamps as defined in RTProfile::getTraceTime()
  uint64_t ZYNQShimProfiling::getHostTraceTimeNsec() {
    using namespace std::chrono;
    typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
    duration_ns time_span =
        duration_cast<duration_ns>(high_resolution_clock::now().time_since_epoch());
    return time_span.count();
  }

  uint32_t ZYNQShimProfiling::getIPCountAddrNames(int type, uint64_t *baseAddress, std::string *portNames, uint8_t *properties, uint8_t *majorVersions, uint8_t *minorVersions, size_t size)
  {
    char sysfsPath[512] ;

    debug_ip_layout* layout ;
    
    shim->xclGetSysfsPath("", "debug_ip_layout", sysfsPath, 512);
    
    std::ifstream ifs(sysfsPath, std::ifstream::binary);
    uint32_t count = 0 ;
    
    char buffer[65536];
    if (ifs) {
      // debug_ip_layout max size is 65536
      ifs.read(buffer, 65536) ;
      
      if (ifs.gcount() > 0) {
	layout = (debug_ip_layout*)(buffer);
	
	for (unsigned int i = 0 ; i < layout->m_count; ++i) 
	{
	  if (count >= size) break ;
	  if (layout->m_debug_ip_data[i].m_type == type) 
	  {
	    if(baseAddress)
	      baseAddress[count] = layout->m_debug_ip_data[i].m_base_address;
	    if(portNames)
	      portNames[count].assign(layout->m_debug_ip_data[i].m_name, 128);
	    if(properties)
	      properties[count] = layout->m_debug_ip_data[i].m_properties;
	    if(majorVersions) 
	      majorVersions[count] = layout->m_debug_ip_data[i].m_major;
	    if(minorVersions) 
	      minorVersions[count] = layout->m_debug_ip_data[i].m_minor;
	    
	    ++count;
	  }
	}
      }
      ifs.close() ;
    }
    return count;
  }

  void ZYNQShimProfiling::readDebugIpLayout()
  {
    if (mIsDebugIpLayoutRead) return ;
    
    mMemoryProfilingNumberSlots = getIPCountAddrNames(AXI_MM_MONITOR, 
						      mPerfMonBaseAddress, 
						      mPerfMonSlotName, 
						      mPerfmonProperties, 
						      mPerfmonMajorVersions, 
						      mPerfmonMinorVersions, 
						      XSPM_MAX_NUMBER_SLOTS);

    mAccelProfilingNumberSlots = getIPCountAddrNames(ACCEL_MONITOR, 
						     mAccelMonBaseAddress, 
						     mAccelMonSlotName, 
						     mAccelmonProperties, 
						     mAccelmonMajorVersions, 
						     mAccelmonMinorVersions, 
						     XSAM_MAX_NUMBER_SLOTS);

    mStreamProfilingNumberSlots = getIPCountAddrNames(AXI_STREAM_MONITOR, 
						      mStreamMonBaseAddress, 
						      mStreamMonSlotName, 
						      mStreammonProperties, 
						      mStreammonMajorVersions, 
						      mStreammonMinorVersions, 
						      XSSPM_MAX_NUMBER_SLOTS);

    mIsDeviceProfiling = (mMemoryProfilingNumberSlots > 0 || mAccelProfilingNumberSlots > 0) ;
  
    std::string fifoName;
    uint64_t fifoCtrlBaseAddr = 0x0;
    getIPCountAddrNames(AXI_MONITOR_FIFO_LITE, 
			&fifoCtrlBaseAddr, 
			&fifoName, 
			nullptr, 
			nullptr, 
			nullptr, 
			1);
    mPerfMonFifoCtrlBaseAddress = fifoCtrlBaseAddr;
    
    uint64_t fifoReadBaseAddr = XPAR_AXI_PERF_MON_0_TRACE_OFFSET_AXI_FULL2;
    getIPCountAddrNames(AXI_MONITOR_FIFO_FULL, 
			&fifoReadBaseAddr, 
			&fifoName, 
			nullptr, 
			nullptr, 
			nullptr, 
			1);
    mPerfMonFifoReadBaseAddress = fifoReadBaseAddr;
    
    uint64_t traceFunnelAddr = 0x0;
    getIPCountAddrNames(AXI_TRACE_FUNNEL, 
			&traceFunnelAddr, 
			nullptr, 
			nullptr, 
			nullptr, 
			nullptr, 
			1);
    mTraceFunnelAddress = traceFunnelAddr;
    
    // Count accel monitors with stall monitoring turned on
    mStallProfilingNumberSlots = 0;
    for (unsigned int i = 0; i < mAccelProfilingNumberSlots; ++i) {
      if ((mAccelmonProperties[i] >> 2) & 0x1)
	mStallProfilingNumberSlots++;
    }
    
    mIsDebugIpLayoutRead = true ;
  }

  uint64_t ZYNQShimProfiling::getPerfMonBaseAddress(xclPerfMonType type, uint32_t slotNum)
  {
    switch(type)
    {
    case XCL_PERF_MON_MEMORY: return mPerfMonBaseAddress[slotNum] ;
    case XCL_PERF_MON_ACCEL:  return mAccelMonBaseAddress[slotNum] ;
    case XCL_PERF_MON_STR:    return mStreamMonBaseAddress[slotNum] ;
    default:                  return 0 ;
    }
  }

} // End namespace ZYNQ
