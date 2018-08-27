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

//#include <CL/opencl.h>
#include "../../driver/include/xclperf.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cassert>
#include <cstring>

#include "xdp_profile.h"
#include "xdp_perf_counters.h"
#include "xdp_profile_results.h"
#include "xdp_profile_writers.h"

// Uncomment to use device-based timestamps in timeline trace
//#define USE_DEVICE_TIMELINE

namespace XDP {
  // ***********************
  // Top-Level Profile Class
  // ***********************
  XDPProfile::XDPProfile(int& flag)
  : ProfileFlags(flag),
    PerfCounters(),
    FileFlags(0),
	FlowMode(DEVICE)
  {
    HostSlotIndex = XPAR_SPM0_HOST_SLOT;
    memset(&CUPortsToDDRBanks, 0, MAX_DDR_BANKS*sizeof(int));
  }

  XDPProfile::~XDPProfile()
  {
    if (ProfileFlags)
      writeProfileSummary();

    FinalCounterResultsMap.clear();
    RolloverCounterResultsMap.clear();
    RolloverCountsMap.clear();
  }

  // ***************************************************************************
  // Helper Functions
  // ***************************************************************************
  void XDPProfile::attach(WriterI* writer)
  {
    std::lock_guard < std::mutex > lock(LogMutex);
    auto itr = std::find(Writers.begin(), Writers.end(), writer);
    if (itr == Writers.end())
      Writers.push_back(writer);
  }

  void XDPProfile::detach(WriterI* writer)
  {
    std::lock_guard < std::mutex > lock(LogMutex);
    auto itr = std::find(Writers.begin(), Writers.end(), writer);
    if (itr != Writers.end())
      Writers.erase(itr);
  }

  uint32_t XDPProfile::getProfileNumberSlots(xclPerfMonType type,
      const std::string& deviceName) const
  {
	// TODO: for now, assume single device (ignore deviceName)
    auto iter = NumberSlotMap.find(type);
	if (iter != NumberSlotMap.end())
      return iter->second;
    return 0;
  }

  void XDPProfile::getFlowModeName(std::string& str) const
  {
    if (FlowMode == CPU)
      str = "CPU Emulation";
    else if (FlowMode == HW_EM)
      str = "Hardware Emulation";
    else
      str = "System Run";
  }

  void XDPProfile::commandKindToString(e_profile_command_kind objKind,
      std::string& commandString) const
  {
    switch (objKind) {
    case READ_BUFFER:
      commandString = "READ_BUFFER";
      break;
    case WRITE_BUFFER:
      commandString = "WRITE_BUFFER";
      break;
    case EXECUTE_KERNEL:
      commandString = "KERNEL";
      break;
    case DEVICE_KERNEL_READ:
      commandString = "KERNEL_READ";
      break;
    case DEVICE_KERNEL_WRITE:
      commandString = "KERNEL_WRITE";
      break;
    case DEVICE_KERNEL_EXECUTE:
      commandString = "KERNEL_EXECUTE";
      break;
    case DEVICE_BUFFER_READ:
      commandString = "READ_BUFFER_DEVICE";
      break;
    case DEVICE_BUFFER_WRITE:
      commandString = "WRITE_BUFFER_DEVICE";
      break;
    case DEPENDENCY_EVENT:
      commandString = "DEPENDENCY_EVENT";
      break;
    default:
      assert(0);
      break;
    }
  }

  void XDPProfile::commandStageToString(e_profile_command_state objStage,
      std::string& stageString) const
  {
    switch (objStage) {
    case QUEUE:
      stageString = "QUEUE";
      break;
    case SUBMIT:
      stageString = "SUBMIT";
      break;
    case START:
      stageString = "START";
      break;
    case END:
      stageString = "END";
      break;
    case COMPLETE:
      stageString = "COMPLETE";
      break;
    default:
      assert(0);
      break;
    }
  }

  void XDPProfile::getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                                      unsigned slotnum, std::string& slotName) const
  {
    if (type == XCL_PERF_MON_ACCEL) {
      auto iter = SlotComputeUnitNameMap.find(slotnum);
      if (iter != SlotComputeUnitNameMap.end())
        slotName = iter->second;
    }

    if (type == XCL_PERF_MON_MEMORY) {
      auto iter = SlotComputeUnitPortNameMap.find(slotnum);
      if (iter != SlotComputeUnitPortNameMap.end())
        slotName = iter->second;
    }
  }

  void XDPProfile::getProfileKernelName(const std::string& deviceName, const std::string& cuName,
                                        std::string& kernelName) const
  {
    auto iter = ComputeUnitKernelNameMap.find(cuName);
    if (iter != ComputeUnitKernelNameMap.end())
      kernelName = iter->second;
  }

  // Set kernel clock freq on device
  void XDPProfile::setKernelClockFreqMHz(const std::string &deviceName, unsigned int kernelClockRateMHz) {
    DeviceKernelClockFreqMap[deviceName] = kernelClockRateMHz;
  }

  // Get kernel clock freq on device
  unsigned int XDPProfile::getKernelClockFreqMHz(std::string &deviceName) const
  {
    auto iter = DeviceKernelClockFreqMap.find(deviceName);
    if (iter != DeviceKernelClockFreqMap.end())
      return iter->second;
    return 300;
  }

  // Get device clock freq (in MHz)
  double XDPProfile::getDeviceClockFreqMHz() const
  {
    return 300.0;
  }

  // Get global memory clock freq (in MHz)
  double XDPProfile::getGlobalMemoryClockFreqMHz() const
  {
    return 300.0;
  }

  // Get global memory bit width
  uint32_t XDPProfile::getGlobalMemoryBitWidth() const
  {
    return XPAR_AXI_PERF_MON_0_SLOT0_DATA_WIDTH;
  }

  // Max. achievable bandwidth between kernels and DDR global memory = 60% of 10.7 GBps for PCIe Gen 3
  double XDPProfile::getGlobalMemoryMaxBandwidthMBps() const
  {
    double maxBandwidthMBps =
        0.6 * (getGlobalMemoryBitWidth() / 8) * getGlobalMemoryClockFreqMHz();
    return maxBandwidthMBps;
  }

  // Max. achievable read bandwidth between host and DDR global memory
  // TODO: this should be a call to the HAL function xclGetReadMaxBandwidthMBps()
  double XDPProfile::getReadMaxBandwidthMBps() const
  {
    return 9600.0;
  }

  // Max. achievable write bandwidth between host and DDR global memory
  // TODO: this should be a call to the HAL function xclGetWriteMaxBandwidthMBps()
  double XDPProfile::getWriteMaxBandwidthMBps() const
  {
    return 9600.0;
  }

  // Log device counters results
  void XDPProfile::logDeviceCounters(std::string deviceName, std::string binaryName, xclPerfMonType type,
      xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram)
  {
    // Number of monitor slots
    uint32_t numSlots = 0;
    std::string key = deviceName + "|" + binaryName;
    std::string slotName = "";

    printf("logDeviceCounters: first read = %d, device: %s\n", firstReadAfterProgram, deviceName.c_str());

    // If not already defined, zero out rollover values for this device
    if (FinalCounterResultsMap.find(key) == FinalCounterResultsMap.end()) {
      FinalCounterResultsMap[key] = counterResults;

      xclCounterResults rolloverResults;
      memset(&rolloverResults, 0, sizeof(xclCounterResults));
      //rolloverResults.NumSlots = counterResults.NumSlots;
      RolloverCounterResultsMap[key] = rolloverResults;
      RolloverCountsMap[key] = rolloverResults;
    }
    else {
      /*
       * Log SPM Counters
       */
      numSlots = getProfileNumberSlots(XCL_PERF_MON_MEMORY, deviceName);
      // Traverse all monitor slots (host and all CU ports)
   	   bool deviceDataExists = (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()) ? false : true;
       for (int s=0; s < numSlots; ++s) {
        getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, s, slotName);
        if (!deviceDataExists)
          DeviceBinaryDataSlotsMap[key].push_back(slotName);
        uint32_t prevWriteBytes   = FinalCounterResultsMap[key].WriteBytes[s];
        uint32_t prevReadBytes    = FinalCounterResultsMap[key].ReadBytes[s];
        uint32_t prevWriteTranx   = FinalCounterResultsMap[key].WriteTranx[s];
        uint32_t prevReadTranx    = FinalCounterResultsMap[key].ReadTranx[s];
        uint32_t prevWriteLatency = FinalCounterResultsMap[key].WriteLatency[s];
        uint32_t prevReadLatency  = FinalCounterResultsMap[key].ReadLatency[s];

        // Check for rollover of byte counters; if detected, add 2^32
        // Otherwise, if first read after program with binary, then capture bytes from previous xclbin
        if (!firstReadAfterProgram) {
          if (counterResults.WriteBytes[s] < prevWriteBytes)
            RolloverCountsMap[key].WriteBytes[s]    += 1;
          if (counterResults.ReadBytes[s] < prevReadBytes)
            RolloverCountsMap[key].ReadBytes[s]     += 1;
          if (counterResults.WriteTranx[s] < prevWriteTranx)
            RolloverCountsMap[key].WriteTranx[s]    += 1;
          if (counterResults.ReadTranx[s] < prevReadTranx)
            RolloverCountsMap[key].ReadTranx[s]     += 1;
          if (counterResults.WriteLatency[s] < prevWriteLatency)
            RolloverCountsMap[key].WriteLatency[s]  += 1;
          if (counterResults.ReadLatency[s] < prevReadLatency)
            RolloverCountsMap[key].ReadLatency[s]   += 1;
        }
        else {
          RolloverCounterResultsMap[key].WriteBytes[s]    += prevWriteBytes;
          RolloverCounterResultsMap[key].ReadBytes[s]     += prevReadBytes;
          RolloverCounterResultsMap[key].WriteTranx[s]    += prevWriteTranx;
          RolloverCounterResultsMap[key].ReadTranx[s]     += prevReadTranx;
          RolloverCounterResultsMap[key].WriteLatency[s]  += prevWriteLatency;
          RolloverCounterResultsMap[key].ReadLatency[s]   += prevReadLatency;
        }
   	  }
      /*
       * Log SAM Counters
       */     
      numSlots = getProfileNumberSlots(XCL_PERF_MON_ACCEL, deviceName);
      for (int s=0; s < numSlots; ++s) {
        uint32_t prevCuExecCount      = FinalCounterResultsMap[key].CuExecCount[s];
        uint32_t prevCuExecCycles     = FinalCounterResultsMap[key].CuExecCycles[s];
        uint32_t prevCuStallExtCycles = FinalCounterResultsMap[key].CuStallExtCycles[s];
        uint32_t prevCuStallIntCycles = FinalCounterResultsMap[key].CuStallIntCycles[s];
        uint32_t prevCuStallStrCycles = FinalCounterResultsMap[key].CuStallStrCycles[s];
        if (!firstReadAfterProgram) {
          if (counterResults.CuExecCycles[s] < prevCuExecCycles)
            RolloverCountsMap[key].CuExecCycles[s]     += 1;
          if (counterResults.CuStallExtCycles[s] < prevCuStallExtCycles)
            RolloverCountsMap[key].CuStallExtCycles[s] += 1;
          if (counterResults.CuStallIntCycles[s] < prevCuStallIntCycles)
            RolloverCountsMap[key].CuStallIntCycles[s] += 1;
          if (counterResults.CuStallStrCycles[s] < prevCuStallStrCycles)
            RolloverCountsMap[key].CuStallStrCycles[s] += 1;
        }
        else {
          RolloverCounterResultsMap[key].CuExecCount[s]      += prevCuExecCount;
          RolloverCounterResultsMap[key].CuExecCycles[s]     += prevCuExecCycles;
          RolloverCounterResultsMap[key].CuStallExtCycles[s] += prevCuStallExtCycles;
          RolloverCounterResultsMap[key].CuStallIntCycles[s] += prevCuStallIntCycles;
          RolloverCounterResultsMap[key].CuStallStrCycles[s] += prevCuStallStrCycles;
        }
      }
      FinalCounterResultsMap[key] = counterResults;
    }
    /*
     * Update Stats Database
     */
    uint32_t kernelClockMhz = getKernelClockFreqMHz(deviceName);
    double deviceCyclesMsec = kernelClockMhz * 1000.0 ;
    std::string cuName = "";
    std::string kernelName ="";
    bool deviceDataExists = (DeviceBinaryCuSlotsMap.find(key) == DeviceBinaryCuSlotsMap.end()) ? false : true;
    xclCounterResults rolloverResults = RolloverCounterResultsMap.at(key);
    xclCounterResults rolloverCounts = RolloverCountsMap.at(key);
    for (int s=0; s < numSlots; ++s) {
      getProfileSlotName(XCL_PERF_MON_ACCEL, deviceName, s, cuName);
      getProfileKernelName(deviceName, cuName, kernelName);
      if (!deviceDataExists)
        DeviceBinaryCuSlotsMap[key].push_back(cuName);
      uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
      uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
      uint32_t cuMaxExecCycles  = counterResults.CuMaxExecCycles[s];
      uint32_t cuMinExecCycles  = counterResults.CuMinExecCycles[s];
      double cuRunTimeMsec = (double) cuExecCycles / deviceCyclesMsec;
      double cuMaxExecCyclesMsec = (double) cuMaxExecCycles / deviceCyclesMsec;
      double cuMinExecCyclesMsec = (double) cuMinExecCycles / deviceCyclesMsec;
      PerfCounters.logComputeUnitStats(cuName, kernelName, cuRunTimeMsec, cuMaxExecCyclesMsec, 
                                        cuMinExecCyclesMsec, cuExecCount, kernelClockMhz);
    }
  }

  // ***************************************************************************
  // Writer Functions
  // ***************************************************************************
  void XDPProfile::writeKernelSummary(WriterI* writer) const
  {
    PerfCounters.writeKernelSummary(writer);
  }

  void XDPProfile::writeComputeUnitSummary(WriterI* writer) const
  {
    PerfCounters.writeComputeUnitSummary(writer);
  }

  void XDPProfile::writeHostTransferSummary(WriterI* writer) const
  {
    uint64_t totalReadBytes    = 0;
    uint64_t totalWriteBytes   = 0;
    uint64_t totalReadLatency  = 0;
    uint64_t totalWriteLatency = 0;
    double totalReadTimeMsec   = 0.0;
    double totalWriteTimeMsec  = 0.0;

    // Get total bytes and total time (currently derived from latency)
    // across all devices
    //
    // CR 951564: Use APM counters to calculate throughput (i.e., byte count and total time)
    // NOTE: for now, we only use this for writes (see PerformanceCounter::writeHostTransferSummary)
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));
      uint32_t  numHostSlots = getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);
      for (uint32_t s=HostSlotIndex; s < HostSlotIndex + numHostSlots; s++) {
        totalReadBytes += counterResults.ReadBytes[s]
                          + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        totalWriteBytes += counterResults.WriteBytes[s]
                          + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        // Total transfer time = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        totalReadLatency += counterResults.ReadLatency[s]
                            + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        totalWriteLatency += counterResults.WriteLatency[s]
                            + (rolloverCounts.WriteLatency[s] * 4294967296UL);
      }
    }
    totalReadTimeMsec = totalReadLatency / (1000.0 * getDeviceClockFreqMHz());
    totalWriteTimeMsec = totalWriteLatency / (1000.0 * getDeviceClockFreqMHz());

    // Get maximum throughput rates
    double readMaxBandwidthMBps = 0.0;
    double writeMaxBandwidthMBps = 0.0;
    if (getFlowMode() != CPU) {
      readMaxBandwidthMBps = getReadMaxBandwidthMBps();
      writeMaxBandwidthMBps = getWriteMaxBandwidthMBps();
    }

    PerfCounters.writeHostTransferSummary(writer, true,  totalReadBytes,  totalReadTimeMsec,  readMaxBandwidthMBps);
    PerfCounters.writeHostTransferSummary(writer, false, totalWriteBytes, totalWriteTimeMsec, writeMaxBandwidthMBps);
  }

  void XDPProfile::writeStallSummary(WriterI* writer) const
  {
    auto iter = FinalCounterResultsMap.begin();
    double deviceCyclesMsec = (getDeviceClockFreqMHz() * 1000.0);
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryCuSlotsMap.find(key) == DeviceBinaryCuSlotsMap.end()))
        continue;

    // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      std::string cuName = "";

      uint32_t numSlots = DeviceBinaryCuSlotsMap.at(key).size();
      for (int s=0; s < numSlots; ++s) {
        cuName = DeviceBinaryCuSlotsMap.at(key)[s];
        uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
        uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                  + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
        uint64_t cuStallExtCycles = counterResults.CuStallExtCycles[s] + rolloverResults.CuStallExtCycles[s]
                                  + (rolloverCounts.CuStallExtCycles[s] * 4294967296UL);
        uint64_t cuStallStrCycles = counterResults.CuStallStrCycles[s] + rolloverResults.CuStallStrCycles[s]
                                  + (rolloverCounts.CuStallStrCycles[s] * 4294967296UL);
        uint64_t cuStallIntCycles = counterResults.CuStallIntCycles[s] + rolloverResults.CuStallIntCycles[s]
                                  + (rolloverCounts.CuStallIntCycles[s] * 4294967296UL);
        double cuRunTimeMsec = (double) cuExecCycles / deviceCyclesMsec;
        double cuStallExt =    (double) cuStallExtCycles / deviceCyclesMsec;
        double cuStallStr =    (double) cuStallStrCycles / deviceCyclesMsec;
        double cuStallInt =    (double) cuStallIntCycles / deviceCyclesMsec;
        writer->writeStallSummary(cuName, cuExecCount, cuRunTimeMsec,
                                  cuStallExt, cuStallStr, cuStallInt);
      }
    }
  }

  void XDPProfile::writeKernelTransferSummary(WriterI* writer) const
  {
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = DeviceBinaryDataSlotsMap.at(key).size();
      uint32_t numHostSlots = getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);

      // Total kernel time = sum of all kernel executions
      //double totalKernelTimeMsec = PerfCounters.getTotalKernelExecutionTime(deviceName);
      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      int s = 0;
      if (HostSlotIndex == 0)
        s = numHostSlots;
      for (; s < numSlots; ++s) {
        if (s == HostSlotIndex)
          continue;

   	    std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        std::string portName = cuPortName.substr(cuPortName.find_first_of("/")+1);
        std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);

        // TODO: currently we don't know the arguments or DDR bank;
        // in OpenCL, this was known by the runtime
        uint32_t ddrBank = 0;
        std::string argNames = "N/A";
        //getArgumentsBank(deviceName, cuName, portName, argNames, ddrBank);

        double totalCUTimeMsec = PerfCounters.getComputeUnitTotalTime(deviceName, cuName);

        uint64_t totalReadBytes    = counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                                     + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        uint64_t totalWriteBytes   = counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                                     + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        uint64_t totalReadTranx    = counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                                     + (rolloverCounts.ReadTranx[s] * 4294967296UL);
        uint64_t totalWriteTranx   = counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                                     + (rolloverCounts.WriteTranx[s] * 4294967296UL);

        // Total transfer time = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        uint64_t totalReadLatency  = counterResults.ReadLatency[s] + rolloverResults.ReadLatency[s]
                                     + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        double totalReadTimeMsec   = totalReadLatency / (1000.0 * getDeviceClockFreqMHz());
        uint64_t totalWriteLatency = counterResults.WriteLatency[s] + rolloverResults.WriteLatency[s]
                                     + (rolloverCounts.WriteLatency[s] * 4294967296UL);
        double totalWriteTimeMsec  = totalWriteLatency / (1000.0 * getDeviceClockFreqMHz());

        printf("writeKernelTransferSummary: s=%d, reads=%d, writes=%d, %s time = %f msec\n",
            s, totalReadTranx, totalWriteTranx, cuName.c_str(), totalCUTimeMsec);

        // First do READ, then WRITE
        if (totalReadTranx > 0) {
          PerfCounters.writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, ddrBank,
        	  true,  totalReadBytes, totalReadTranx, totalCUTimeMsec, totalReadTimeMsec, maxTransferRateMBps);
        }
        if (totalWriteTranx > 0) {
          PerfCounters.writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, ddrBank,
        	  false, totalWriteBytes, totalWriteTranx, totalCUTimeMsec, totalWriteTimeMsec, maxTransferRateMBps);
        }
      }
    }
  }

  void XDPProfile::writeTopKernelSummary(WriterI* writer) const
  {
    PerfCounters.writeTopKernelSummary(writer);
  }

  void XDPProfile::writeTopKernelTransferSummary(WriterI* writer) const
  {
    // Iterate over all devices
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      //if (!isDeviceActive(deviceName) || (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()))
      //  continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = DeviceBinaryDataSlotsMap.at(key).size();
      uint32_t numHostSlots = getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);

      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      //double totalReadTimeMsec  = PerfCounters.getTotalKernelExecutionTime(deviceName);
      //double totalWriteTimeMsec = totalReadTimeMsec;

      // Maximum bytes per AXI data transfer
      // NOTE: this assumes the entire global memory bit width with a burst of 256 (max burst length of AXI4)
      //       AXI standard also limits a transfer to 4K total bytes
      uint32_t maxBytesPerTransfer = (getGlobalMemoryBitWidth() / 8) * 256;
      if (maxBytesPerTransfer > 4096)
        maxBytesPerTransfer = 4096;

      // Gather unique names of monitored CUs on this device
      std::map<std::string, uint64_t> cuNameTranxMap;
      int s;
      if (HostSlotIndex == 0)
        s = numHostSlots;
      else 
        s = 0;
      for (; s < numSlots; ++s) {
        if (s == HostSlotIndex)
          continue;

        std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        cuNameTranxMap[cuName] = 0;
      }

      // Get their total tranx counts
      auto cuIter = cuNameTranxMap.begin();
      for (; cuIter != cuNameTranxMap.end(); ++cuIter) {
        std::string cuName = cuIter->first;

        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        if (HostSlotIndex == 0)
          s = numHostSlots;
        else 
          s = 0;
        for (; s < numSlots; ++s) {
          if (s == HostSlotIndex)
            continue;

          std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
          }
        }

        cuNameTranxMap[cuName] = (totalReadTranx + totalWriteTranx);
      }

      // Sort the CUs by their tranx count
      std::vector<std::pair<std::string, uint64_t>> cuPairs(cuNameTranxMap.begin(),
          cuNameTranxMap.end());
      std::sort(cuPairs.begin(), cuPairs.end(),
          [](const std::pair<std::string, uint64_t>& A, const std::pair<std::string, uint64_t>& B) {
               return (A.second > B.second);
             });

      // Now report them in order of total tranx counts
      for (const auto &pair : cuPairs) {
        std::string cuName = pair.first;

        uint64_t totalReadBytes  = 0;
        uint64_t totalWriteBytes = 0;
        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        if (HostSlotIndex == 0)
          s = numHostSlots;
        else 
          s = 0;
        for (; s < numSlots; ++s) {
          if (s == HostSlotIndex)
            continue;

          std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadBytes  += counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                               + (rolloverCounts.ReadBytes[s] * 4294967296UL);
            totalWriteBytes += counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                               + (rolloverCounts.WriteBytes[s] * 4294967296UL);
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
          }
        }

        double totalCUTimeMsec = PerfCounters.getComputeUnitTotalTime(deviceName, cuName);

        PerfCounters.writeTopKernelTransferSummary(writer, deviceName, cuName, totalWriteBytes,
            totalReadBytes, totalWriteTranx, totalReadTranx, totalCUTimeMsec, totalCUTimeMsec,
            maxBytesPerTransfer, maxTransferRateMBps);
      }
    }
  }

  void XDPProfile::writeDeviceTransferSummary(WriterI* writer) const
  {
    PerfCounters.writeDeviceTransferSummary(writer, true);
    PerfCounters.writeDeviceTransferSummary(writer, false);
  }

  void XDPProfile::writeTopDataTransferSummary(WriterI* writer, bool isRead) const
  {
    PerfCounters.writeTopDataTransferSummary(writer, isRead);
  }

  void XDPProfile::writeTopDeviceTransferSummary(WriterI* writer, bool isRead) const
  {
    PerfCounters.writeTopDeviceTransferSummary(writer, isRead);
  }

#if 0
  void XDPProfile::getProfileRuleCheckSummary()
  {
    RuleChecks->getProfileRuleCheckSummary(this);
  }

  void XDPProfile::writeProfileRuleCheckSummary(WriterI* writer)
  {
    RuleChecks->writeProfileRuleCheckSummary(writer, this);
  }
#endif

  void XDPProfile::writeProfileSummary()
  {
    if (!this->isApplicationProfileOn())
      return;

    for (auto w : Writers) {
      w->writeSummary(this);
    }
  }
}

