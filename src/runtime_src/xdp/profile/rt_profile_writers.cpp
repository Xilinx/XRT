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

// Copyright 2014 Xilinx, Inc. All rights reserved.

#include "rt_profile_writers.h"
#include "rt_profile.h"
#include "rt_profile_results.h"
#include "rt_profile_device.h"
#include "xdp/rt_singleton.h"
#include "driver/include/xclperf.h"
#include "debug.h"
#include "xocl/core/device.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <boost/format.hpp>

namespace XCL {
  //************
  // Base Writer
  //************
  WriterI::WriterI() 
  {
    // Reset previous values of device profile counters
    memset(&CountersPrev, 0, sizeof(xclCounterResults));
  }
  
  std::string WriterI::getCurrentDateTime()
  {
    auto time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    //It seems std::put_time is not yet available
    //std::stringstream ss;
    //ss << std::put_time(std::localtime(&time), "%Y-%m-%d %X");
    //return ss.str();
    struct tm tstruct = *(std::localtime(&time));
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return std::string(buf);
  }

  std::string WriterI::getCurrentTimeMsec()
  {
    struct timespec now;
    int err;
    if ((err = clock_gettime(CLOCK_REALTIME, &now)) < 0)
      return "0";

    uint64_t nsec = (uint64_t) now.tv_sec * 1000000000UL + (uint64_t) now.tv_nsec;
    uint64_t msec = nsec / 1e6;
    return std::to_string(msec);
  }

  std::string WriterI::getCurrentExecutableName()
  {
    std::string execName("");
  #if defined(__linux__) && defined(__x86_64__)
    const int maxLength = 1024;
    char buf[maxLength];
    ssize_t len;
    if ((len=readlink("/proc/self/exe", buf, maxLength-1)) != -1) {
      buf[len]= '\0';
      execName = buf;
    }

    // Remove directory path and only keep filename
    const size_t lastSlash = execName.find_last_of("\\/");
    if (lastSlash != std::string::npos)
      execName.erase(0, lastSlash + 1);
  #endif
    return execName;
  }

  void WriterI::openStream(std::ofstream& ofs, const std::string& fileName)
  {
    ofs.open(fileName);
    if (!ofs.is_open()) {
      throw std::runtime_error("Unable to open profile report for writing");
    }
  }

  void WriterI::writeSummary(RTProfile* profile)
  {
    auto rts = XCL::RTSingleton::Instance();
    auto flowMode = rts->getFlowMode();

    // Sub-header
    writeDocumentSubHeader(getSummaryStream(), profile);

    //Table 1: API Call summary
    std::vector<std::string> APICallSummaryColumnLabels = { "API Name",
        "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)" };

    writeTableHeader(getSummaryStream(), "OpenCL API Calls", APICallSummaryColumnLabels);
    profile->writeAPISummary(this);
    writeTableFooter(getSummaryStream());

    // Table 2: Kernel Execution Summary
    std::vector<std::string> KernelExecutionSummaryColumnLabels = {
        "Kernel", "Number Of Enqueues", "Total Time (ms)",
        "Minimum Time (ms)", "Average Time (ms)", "Maximum Time (ms)" };

    std::string table2Caption = (flowMode == XCL::RTSingleton::HW_EM) ?
        "Kernel Execution (includes estimated device times)" : "Kernel Execution";
    writeTableHeader(getSummaryStream(), table2Caption, KernelExecutionSummaryColumnLabels);
    profile->writeKernelSummary(this);
    writeTableFooter(getSummaryStream());

    // Table 3: Compute Unit Utilization
    std::vector<std::string> ComputeUnitExecutionSummaryColumnLabels = {
        "Device", "Compute Unit", "Kernel", "Global Work Size", "Local Work Size",
        "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)", "Clock Frequency (MHz)" };

    std::string table3Caption = (flowMode == XCL::RTSingleton::HW_EM) ?
        "Compute Unit Utilization (includes estimated device times)" : "Compute Unit Utilization";
    writeTableHeader(getSummaryStream(), table3Caption, ComputeUnitExecutionSummaryColumnLabels);
    profile->writeComputeUnitSummary(this);
    writeTableFooter(getSummaryStream());

    // Table 4: CU Stalls only for HW Runs
    // NOTE: only display this table if
    //   * device counter profiling is turned on (default: true)
    //   * it was run on a board
    //   * at least one device has stall profiling in the dynamic region
    unsigned numStallSlots = 0;
    unsigned numStreamSlots = 0;
    auto platform = rts->getcl_platform_id();
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      numStallSlots += rts->getProfileNumberSlots(XCL_PERF_MON_STALL, deviceName);
      numStreamSlots += rts->getProfileNumberSlots(XCL_PERF_MON_STR, deviceName);
    }

    if (profile->isDeviceProfileOn() && 
      (flowMode == XCL::RTSingleton::DEVICE) && 
      (numStallSlots > 0)) {
      std::vector<std::string> KernelStallLabels = {
        "Compute Unit", "Execution Count", "Running Time (ms)", "Intra-Kernel Dataflow Stalls (ms)", 
        "External Memory Stalls (ms)", "Inter-Kernel Pipe Stalls (ms)"
      };

      writeTableHeader(getSummaryStream(), "Compute Units: Stall Information", KernelStallLabels);
      profile->writeStallSummary(this);
      writeTableFooter(getSummaryStream());
    }

    // Table 5: Data Transfer: Host & Global
    std::vector<std::string> DataTransferSummaryColumnLabels = {
        "Context:Number of Devices", "Transfer Type", "Number Of Transfers",
        "Transfer Rate (MB/s)", "Average Bandwidth Utilization (%)",
        "Average Size (KB)", "Total Time (ms)", "Average Time (ms)"
    };
    writeTableHeader(getSummaryStream(), "Data Transfer: Host and Global Memory",
        DataTransferSummaryColumnLabels);
    if ((flowMode != XCL::RTSingleton::CPU) && (flowMode != XCL::RTSingleton::COSIM_EM)) {
      profile->writeHostTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());

    // Table 6: Data Transfer: Kernel & Global
    std::vector<std::string> KernelDataTransferSummaryColumnLabels = {
        "Device", "Compute Unit/Port Name", "Kernel Arguments", "Memory Resources",
		"Transfer Type", "Number Of Transfers", "Transfer Rate (MB/s)",
		"Average Bandwidth Utilization (%)", "Average Size (KB)", "Average Latency (ns)"
    };
    writeTableHeader(getSummaryStream(), "Data Transfer: Kernels and Global Memory",
        KernelDataTransferSummaryColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeKernelTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());

    // Table 6.1 : Stream Data Transfers
    if (profile->isDeviceProfileOn() && (flowMode == XCL::RTSingleton::DEVICE) && (numStreamSlots > 0)) {
    std::vector<std::string> StreamTransferSummaryColumnLabels = {
        "Device", "Compute Unit/Port Name", "Number Of Transfers", "Average Size (KB)",
		    "Link Utilization (%)", "Link Starve (%)", "Link Stall (%)"
        };
      writeTableHeader(getSummaryStream(), "Stream Data Transfers", StreamTransferSummaryColumnLabels);
      profile->writeKernelStreamSummary(this);
      writeTableFooter(getSummaryStream());
    }

    // Table 7: Top Data Transfer: Kernel & Global
    std::vector<std::string> TopKernelDataTransferSummaryColumnLabels = {
        "Device", "Compute Unit", "Number of Transfers", "Average Bytes per Transfer",
        "Transfer Efficiency (%)", "Total Data Transfer (MB)", "Total Write (MB)",
        "Total Read (MB)", "Total Transfer Rate (MB/s)"
    };
    writeTableHeader(getSummaryStream(), "Top Data Transfer: Kernels and Global Memory",
        TopKernelDataTransferSummaryColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeTopKernelTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());
  }

  // Tables 1 and 2: API Call and Kernel Execution Summary: Name, Number Of Calls,
  // Total Time (ms), Minimum Time (ms), Average Time (ms), Maximum Time (ms)
  void WriterI::writeSummary(const std::string& name, const TimeStats& stats)
  {
    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), name, stats.getNoOfCalls(),
        stats.getTotalTime(), stats.getMinTime(),
        stats.getAveTime(), stats.getMaxTime());
    writeTableRowEnd(getSummaryStream());
  }

  void WriterI::writeStallSummary(std::string& cuName, uint32_t cuRunCount, 
      double cuRunTimeMsec, double cuStallExt, double cuStallStr, double cuStallInt)
  {
    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(),cuName,cuRunCount, cuRunTimeMsec,
     cuStallInt, cuStallExt, cuStallStr);
    writeTableRowEnd(getSummaryStream());
  }

  void WriterI::writeKernelStreamSummary(std::string& deviceName, std::string& cuPortName, std::string& argNames,
      uint64_t strNumTranx, double transferRateMBps, double avgSize, double avgUtil,
      double linkStarve, double linkStall)
  {
    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), deviceName , cuPortName, argNames,
        strNumTranx, transferRateMBps, avgSize, avgUtil, linkStarve, linkStall);
    writeTableRowEnd(getSummaryStream());
  }

  // Table 4: Data Transfer: Host & Global Memory
  // Context ID, Transfer Type, Number Of Transfers, Transfer Rate (MB/s),
  // Average Size (KB), Total Time (ms), Average Time (ms)
  void WriterI::writeHostTransferSummary(const std::string& name,
      const BufferStats& stats, uint64_t totalBytes, uint64_t totalTranx,
      double totalTimeMsec, double maxTransferRateMBps)
  {
    //double aveTimeMsec = stats.getAveTime();
    double aveTimeMsec = (totalTranx == 0) ? 0.0 : totalTimeMsec / totalTranx;

    // Get min/average/max bytes per transaction
    // NOTE: to remove the dependency on trace, we calculate it based on counter values
    //       also, v1.1 of Alpha Data DSA has incorrect AXI lengths so these will always be 16K
#if 0
    double minBytes = (double)(stats.getMin());
    double aveBytes = (double)(stats.getAverage());
    double maxBytes = (double)(stats.getMax());
#else
    double aveBytes = (totalTranx == 0) ? 0.0 : (double)(totalBytes) / totalTranx;
    //double minBytes = aveBytes;
    //double maxBytes = aveBytes;
#endif

    double transferRateMBps = (totalTimeMsec == 0) ? 0.0 :
        totalBytes / (1000.0 * totalTimeMsec);
    double aveBWUtil = (100.0 * transferRateMBps) / maxTransferRateMBps;
    if (aveBWUtil > 100.0)
      aveBWUtil = 100.0;

    if (aveBWUtil > 0) {
      XDP_LOG("%s: Transfered %u bytes in %.3f msec\n", name.c_str(), totalBytes, totalTimeMsec);
      XDP_LOG("  AveBWUtil = %.3f = %.3f / %.3f\n", aveBWUtil, transferRateMBps, maxTransferRateMBps);
    }

    // Don't show these values for HW emulation
    std::string transferRateStr = std::to_string(transferRateMBps);
    std::string aveBWUtilStr = std::to_string(aveBWUtil);
    std::string totalTimeStr = std::to_string(totalTimeMsec);
    std::string aveTimeStr = std::to_string(aveTimeMsec);
    if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) {
      transferRateStr = "N/A";
      aveBWUtilStr = "N/A";
      totalTimeStr = "N/A";
      aveTimeStr = "N/A";
    }

    std::string contextDevices = "context" + std::to_string(stats.getContextId())
        + ":" + std::to_string(stats.getNumDevices());

    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), contextDevices, name, totalTranx,
        transferRateStr, aveBWUtilStr, aveBytes/1000.0, totalTimeStr, aveTimeStr);

    writeTableRowEnd(getSummaryStream());
  }

  // Table 5: Data Transfer: Kernels & Global Memory
  // Device, CU Port, Kernel Arguments, DDR Bank, Transfer Type, Number Of Transfers,
  // Transfer Rate (MB/s), Average Size (KB), Maximum Size (KB), Average Latency (ns)
  void WriterI::writeKernelTransferSummary(const std::string& deviceName,
      const std::string& cuPortName, const std::string& argNames, const std::string& memoryName,
	  const std::string& transferType, uint64_t totalBytes, uint64_t totalTranx,
	  double totalKernelTimeMsec, double totalTransferTimeMsec, double maxTransferRateMBps)
  {
    //double aveTimeMsec = stats.getAveTime();
    double aveTimeMsec = (totalTranx == 0) ? 0.0 : totalTransferTimeMsec / totalTranx;

    // Get min/average/max bytes per transaction
    // NOTE: to remove the dependency on trace, we calculate it based on counter values
    //       also, v1.1 of Alpha Data DSA has incorrect AXI lengths so these will always be 16K
#if 0
    double minBytes = (double)(stats.getMin());
    double aveBytes = (double)(stats.getAverage());
    double maxBytes = (double)(stats.getMax());
#else
    double aveBytes = (totalTranx == 0) ? 0.0 : (double)(totalBytes) / totalTranx;
    //double minBytes = aveBytes;
    //double maxBytes = aveBytes;
#endif

    double transferRateMBps = (totalKernelTimeMsec == 0) ? 0.0 :
        totalBytes / (1000.0 * totalKernelTimeMsec);
    double aveBWUtil = (100.0 * transferRateMBps) / maxTransferRateMBps;
    if (aveBWUtil > 100.0)
      aveBWUtil = 100.0;

    if (aveBWUtil > 0) {
      XDP_LOG("Kernel %s: Transfered %u bytes in %.3f msec (device: %s)\n",
          transferType.c_str(), totalBytes, totalKernelTimeMsec, deviceName.c_str());
      XDP_LOG("  AveBWUtil = %.3f = %.3f / %.3f\n",
          aveBWUtil, transferRateMBps, maxTransferRateMBps);
    }

    // Get memory name from CU port name string (if found)
    std::string cuPortName2 = cuPortName;
    std::string memoryName2 = memoryName;
    size_t index = cuPortName.find_last_of(":");
    if (index != std::string::npos) {
      cuPortName2 = cuPortName.substr(0, index);
      memoryName2 = cuPortName.substr(index+1);
    }

    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), deviceName, cuPortName2, argNames, memoryName2,
    	transferType, totalTranx, transferRateMBps, aveBWUtil,
        aveBytes/1000.0, 1.0e6*aveTimeMsec);

    writeTableRowEnd(getSummaryStream());
  }

  // Table 6: Data Transfer: Top Kernel & Global
  // Device, Compute Unit, Number of Transfers, Average Bytes per Transfer,
  // Total Data Transfer (MB), Total Write (MB), Total Read (MB), Total Transfer Rate (MB/s)
  void WriterI::writeTopKernelTransferSummary(
      const std::string& deviceName, const std::string& cuName,
      uint64_t totalWriteBytes, uint64_t totalReadBytes,
      uint64_t totalWriteTranx, uint64_t totalReadTranx,
      double totalWriteTimeMsec, double totalReadTimeMsec,
      uint32_t maxBytesPerTransfer, double maxTransferRateMBps)
  {
    double totalTimeMsec = (totalWriteTimeMsec > totalReadTimeMsec) ?
        totalWriteTimeMsec : totalReadTimeMsec;

    double transferRateMBps = (totalTimeMsec == 0) ? 0.0 :
        (double)(totalReadBytes + totalWriteBytes) / (1000.0 * totalTimeMsec);
#if 0
    double aveBWUtil = (100.0 * transferRateMBps) / maxTransferRateMBps;
    if (aveBWUtil > 100.0)
      aveBWUtil = 100.0;
#endif

    double aveBytesPerTransfer = ((totalReadTranx + totalWriteTranx) == 0) ? 0.0 :
        (double)(totalReadBytes + totalWriteBytes) / (totalReadTranx + totalWriteTranx);
    double transferEfficiency = (100.0 * aveBytesPerTransfer) / maxBytesPerTransfer;
    if (transferEfficiency > 100.0)
      transferEfficiency = 100.0;

    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(),
        deviceName, cuName,
        totalReadTranx + totalWriteTranx,
        aveBytesPerTransfer, transferEfficiency,
        (double)(totalReadBytes + totalWriteBytes) / 1.0e6,
        (double)(totalWriteBytes) / 1.0e6, (double)(totalReadBytes) / 1.0e6,
        transferRateMBps);
    writeTableRowEnd(getSummaryStream());
  }

  void WriterI::writeSummary(const KernelTrace& trace)
  {
    writeTableRowStart(getSummaryStream());

    std::string globalWorkSize = std::to_string(trace.getGlobalWorkSizeByIndex(0))
        + ":" + std::to_string(trace.getGlobalWorkSizeByIndex(1))
        + ":" + std::to_string(trace.getGlobalWorkSizeByIndex(2) ) ;

    std::string localWorkSize = std::to_string(trace.getLocalWorkSizeByIndex(0))  + ":" + std::to_string(trace.getLocalWorkSizeByIndex(1))
    + ":" + std::to_string(trace.getLocalWorkSizeByIndex(2) ) ;

    writeTableCells(getSummaryStream(), trace.getAddress(), trace.getKernelName(),
        trace.getContextId(), trace.getCommandQueueId(),
        trace.getDeviceName(), trace.getStart(), trace.getDuration(),
        //globalWorkSize, trace.getWorkGroupSize());
        globalWorkSize, localWorkSize);
    writeTableRowEnd(getSummaryStream());
  }

  // Write buffer trace summary (host to global memory)
  void WriterI::writeSummary(const BufferTrace& trace)
  {
    std::string durationStr = std::to_string( trace.getDuration() );
    double rate = (double)(trace.getSize()) / (1000.0 * trace.getDuration());
    std::string rateStr = std::to_string(rate);
    if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::CPU
        || XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::COSIM_EM
        || XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) {
      durationStr = "N/A";
      rateStr = "N/A";
    }

    writeTableRowStart(getSummaryStream());

    writeTableCells(getSummaryStream(), trace.getAddress(), trace.getContextId(),
        trace.getCommandQueueId(), trace.getStart(), durationStr,
        (double)(trace.getSize())/1000.0, rateStr);

    writeTableRowEnd(getSummaryStream());
  }

  // Write device trace summary
  void WriterI::writeSummary(const DeviceTrace& trace)
  {
    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), trace.Name, trace.ContextId, trace.Start,
        trace.BurstLength, (trace.EndTime - trace.StartTime),
        1000.0*(trace.End - trace.Start));
    writeTableRowEnd(getSummaryStream());
  }

  void WriterI::writeComputeUnitSummary(const std::string& name, const TimeStats& stats)
  {
    if (stats.getTotalTime() == 0.0)
      return;
    //"name" is of the form "deviceName|kernelName|globalSize|localSize|cuName"
    size_t first_index = name.find_first_of("|");
    size_t second_index = name.find('|', first_index+1);
    size_t third_index = name.find('|', second_index+1);
    size_t fourth_index = name.find_last_of("|");

    std::string deviceName = name.substr(0, first_index);
    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), deviceName,
        name.substr(fourth_index+1), // cuName
        name.substr(first_index+1, second_index - first_index -1), // kernelName
        name.substr(second_index+1, third_index - second_index -1), // globalSize
        name.substr(third_index+1, fourth_index - third_index -1), // localSize
        stats.getNoOfCalls(), stats.getTotalTime(), stats.getMinTime(),
        stats.getAveTime(), stats.getMaxTime(), stats.getClockFreqMhz());
    writeTableRowEnd(getSummaryStream());
  }

  void WriterI::writeAcceleratorSummary(const std::string& name, const TimeStats& stats)
  {
    //"name" is of the form "deviceName|kernelName|globalSize|localSize|cuName"
    size_t first_index = name.find_first_of("|");
    //size_t second_index = name.find('|', first_index+1);
    //size_t third_index = name.find('|', second_index+1);
    size_t fourth_index = name.find_last_of("|");

    std::string deviceName = name.substr(0, first_index);
    auto clockFreqMHz = XCL::RTSingleton::Instance()->getProfileManager()->getKernelClockFreqMHz(deviceName);

    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), deviceName,
        name.substr(fourth_index+1), // cuName
        stats.getNoOfCalls(), stats.getTotalTime(), stats.getMinTime(),
        stats.getAveTime(), stats.getMaxTime(), clockFreqMHz);
    writeTableRowEnd(getSummaryStream());
  }

  void WriterI::writeSummary(const std::string& name,
      const BufferStats& stats)
  {
    writeTableRowStart(getSummaryStream());
  #ifndef _WINDOWS
    // TODO: Windows build support
    //    Variadic Template is not supported
    writeTableCells(getSummaryStream(), name, stats.getCount(),
        stats.getTotalTime(), stats.getAveTime(),
        stats.getAveTransferRate(), (double)(stats.getMin())/1000.0,
        (double)(stats.getAverage())/1000.0, (double)(stats.getMax())/1000.0);
  #endif
    writeTableRowEnd(getSummaryStream());
  }

  // Write API function event to trace
  void WriterI::writeTimeline(double time, const std::string& functionName,
      const std::string& eventName, unsigned int functionID)
  {
    if (!Timeline_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << time;

    writeTableRowStart(getTimelineStream());
  #ifndef _WINDOWS
    // TODO: Windows build support
    //    Variadic Template is not supported
    writeTableCells(getTimelineStream(), timeStr.str(), functionName, eventName,
        "", "", "", "", "", "", "", "", std::to_string(functionID));
  #endif
    writeTableRowEnd(getTimelineStream());
  }

  // Write kernel event to trace
  void WriterI::writeTimeline(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, uint64_t objId, size_t size)
  {
    if (!Timeline_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << traceTime;

    std::stringstream strObjId;
    strObjId << std::showbase << std::hex << std::uppercase << objId;

    writeTableRowStart(getTimelineStream());
  #ifndef _WINDOWS
    // TODO: Windows build support
    //    Variadic Template is not supported
    writeTableCells(getTimelineStream(), timeStr.str(), commandString,
        stageString, strObjId.str(), size, "", "", "", "", "", "",
        eventString, dependString);
  #endif
    writeTableRowEnd(getTimelineStream());
  }

  // Write data transfer event to trace
  void WriterI::writeTimeline(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId)
  {
    if (!Timeline_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << traceTime;

    // Write out DDR physical address and bank
    // NOTE: thread ID is only valid for START and END
    std::stringstream strAddress;
    strAddress << (boost::format("0X%09x") % address) << "|" << std::dec << bank;
    //strAddress << std::showbase << std::hex << std::uppercase << address
    //		   << "|" << std::dec << bank;
    if (stageString == "START" || stageString == "END")
      strAddress << "|" << std::showbase << std::hex << std::uppercase << threadId;

    writeTableRowStart(getTimelineStream());
  #ifndef _WINDOWS
    // TODO: Windows build support
    //    Variadic Template is not supported
    writeTableCells(getTimelineStream(), timeStr.str(), commandString,
        stageString, strAddress.str(), size, "", "", "", "", "", "",
        eventString, dependString);
  #endif
    writeTableRowEnd(getTimelineStream());
  }

  // Write dependency event to trace
  void WriterI::writeTimeline(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString)
  {
    if (!Timeline_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << traceTime;

    writeTableRowStart(getTimelineStream());
  #ifndef _WINDOWS
    // TODO: Windows build support
    //    Variadic Template is not supported
    writeTableCells(getTimelineStream(), timeStr.str(), commandString,
        stageString, eventString, dependString);
  #endif
    writeTableRowEnd(getTimelineStream());
  }

  // Functions for device counters
  void WriterI::writeDeviceCounters(xclPerfMonType type, xclCounterResults& results,
      double timestamp, uint32_t sampleNum, bool firstReadAfterProgram)
  {
    if (!Timeline_ofs.is_open())
      return;
    if (firstReadAfterProgram) {
      CountersPrev = results;
      return;
    }

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << timestamp;

    // This version computes the avg. throughput and latency and writes those values

    static const char* slotNames[] = {
        XPAR_AXI_PERF_MON_0_SLOT0_NAME, XPAR_AXI_PERF_MON_0_SLOT1_NAME,
        XPAR_AXI_PERF_MON_0_SLOT2_NAME, XPAR_AXI_PERF_MON_0_SLOT3_NAME,
        XPAR_AXI_PERF_MON_0_SLOT4_NAME, XPAR_AXI_PERF_MON_0_SLOT5_NAME,
        XPAR_AXI_PERF_MON_0_SLOT6_NAME, XPAR_AXI_PERF_MON_0_SLOT7_NAME
    };

    uint32_t numSlots = XPAR_AXI_PERF_MON_0_NUMBER_SLOTS;
    //uint32_t numSlots = results.mNumSlots;

    for (unsigned int slot=0; slot < numSlots; slot++) {
      // Write
  #if 0
      double writeThputMBps = 0.0;
      if (results.SampleIntervalUsec > 0) {
        writeThputMBps = (results.WriteBytes[slot] - CountersPrev.WriteBytes[slot]) /
            results.SampleIntervalUsec;
      }

      std::stringstream writeThputCellStr;
      writeThputCellStr << std::setprecision(5) << writeThputMBps << " MBps";
  #else
      uint32_t writeBytes = results.WriteBytes[slot] - CountersPrev.WriteBytes[slot];
  #endif

      double writeLatency = 0.0;
      uint32_t numWriteTranx = results.WriteTranx[slot] - CountersPrev.WriteTranx[slot];
      if (numWriteTranx > 0) {
        writeLatency = (results.WriteLatency[slot] - CountersPrev.WriteLatency[slot]) /
            numWriteTranx;
      }

      // Don't report if no new transactions in this sample time window
      if (writeBytes != 0 || writeLatency != 0) {
        // TODO: the APM in the current base platform does not support min/max
  #if 0
        std::stringstream writeLatencyCellStr;
        writeLatencyCellStr << results.WriteMinLatency[slot] << " / " << writeLatency << " / "
            << results.WriteMaxLatency[slot];

        writeTableRowStart(getTimelineStream());
        writeTableCells(getTimelineStream(), timeStr.str(), "Device Counters", "Write", slotNames[slot],
            writeBytes, writeLatencyCellStr.str(), "", "", "", "");
        writeTableRowEnd(getTimelineStream());
  #else
        writeTableRowStart(getTimelineStream());
  #ifndef _WINDOWS
        // TODO: Windows build support
        //    Variadic Template is not supported
        writeTableCells(getTimelineStream(), timeStr.str(), "Device Counters", "Write", slotNames[slot],
            writeBytes, writeLatency, "", "", "", "", "");
  #endif
        writeTableRowEnd(getTimelineStream());
  #endif
      }

      // Read
  #if 0
      double readThputMBps = 0.0;
      if (results.SampleIntervalUsec > 0) {
        readThputMBps = (results.ReadBytes[slot] - CountersPrev.ReadBytes[slot]) /
            results.SampleIntervalUsec;
      }

      std::stringstream readThputCellStr;
      readThputCellStr << std::setprecision(5) << readThputMBps << " MBps";
  #else
      uint32_t readBytes = results.ReadBytes[slot] - CountersPrev.ReadBytes[slot];
  #endif

      double readLatency = 0.0;
      uint32_t numReadTranx = results.ReadTranx[slot] - CountersPrev.ReadTranx[slot];
      if (numReadTranx > 0) {
        readLatency = (results.ReadLatency[slot] - CountersPrev.ReadLatency[slot]) /
            numReadTranx;
      }

      // Don't report if no new transactions in this sample time window
      if (readBytes != 0 || readLatency != 0) {
        // TODO: the APM in the current base platform does not support min/max
  #if 0
        std::stringstream readLatencyCellStr;
        readLatencyCellStr << results.ReadMinLatency[slot] << " / " << readLatency << " / "
            << results.ReadMaxLatency[slot];

        writeTableRowStart(getTimelineStream());
        writeTableCells(getTimelineStream(), timeStr.str(), "Device Counters", "Read", slotNames[slot],
            readBytes, readLatencyCellStr.str(), "", "", "", "");
        writeTableRowEnd(getTimelineStream());
  #else
        writeTableRowStart(getTimelineStream());
  #ifndef _WINDOWS
        // TODO: Windows build support
        //    Variadic Template is not supported
        writeTableCells(getTimelineStream(), timeStr.str(), "Device Counters", "Read", slotNames[slot],
            readBytes, readLatency, "", "", "", "");
  #endif
        writeTableRowEnd(getTimelineStream());
  #endif
      }
    }

    CountersPrev = results;
  }

  // Functions for device trace
  void WriterI::writeDeviceTrace(const RTProfileDevice::TraceResultVector &resultVector,
      std::string deviceName, std::string binaryName)
  {
    if (!Timeline_ofs.is_open())
      return;

#if 0
    // Store name of binary container
    // NOTE: assumes 1:1 correspondence between device and binary
    auto deviceIter = DeviceBinaryNameMap.find(deviceName);
    if (deviceIter == DeviceBinaryNameMap.end())
      DeviceBinaryNameMap[deviceName] = binaryName;
#endif

    for (auto it = resultVector.begin(); it != resultVector.end(); it++) {
      DeviceTrace tr = *it;

#ifndef XDP_VERBOSE
      if (tr.Kind == DeviceTrace::DEVICE_BUFFER)
        continue;
#endif

      auto rts = XCL::RTSingleton::Instance();
      double deviceClockDurationUsec = (1.0 / (rts->getProfileManager()->getKernelClockFreqMHz(deviceName)));

      std::stringstream startStr;
      startStr << std::setprecision(10) << tr.Start;
      std::stringstream endStr;
      endStr << std::setprecision(10) << tr.End;

      bool showKernelCUNames = true;
      bool showPortName = false;
      std::string memoryName;
      std::string traceName;
      std::string cuName;
      std::string argNames;

      // Populate trace name string
      if (tr.Kind == DeviceTrace::DEVICE_KERNEL) {
        if (tr.Type == "Kernel") {
          traceName = "KERNEL";
        } else if (tr.Type.find("Stall") != std::string::npos) {
          traceName = "Kernel_Stall";
          showPortName = false;
        } else if (tr.Type == "Write") {
          showPortName = true;
          traceName = "Kernel_Write";
        } else {
          showPortName = true;
          traceName = "Kernel_Read";
        }
      }
      else if (tr.Kind == DeviceTrace::DEVICE_STREAM) {
        traceName = tr.Name;
        showPortName = true;
      } else {
        showKernelCUNames = false;
        if (tr.Type == "Write")
          traceName = "Host_Write";
        else
          traceName = "Host_Read";
      }

      traceName += ("|" + deviceName + "|" + binaryName);

      if (showKernelCUNames || showPortName) {
        std::string portName;
        std::string cuPortName;
        if (tr.Kind == DeviceTrace::DEVICE_KERNEL && (tr.Type == "Kernel" || tr.Type.find("Stall") != std::string::npos)) {
          rts->getProfileSlotName(XCL_PERF_MON_ACCEL, deviceName, tr.SlotNum, cuName);
        }
        else {
          if (tr.Kind == DeviceTrace::DEVICE_STREAM){
            rts->getProfileSlotName(XCL_PERF_MON_STR, deviceName, tr.SlotNum, cuPortName);
          }
          else {
            rts->getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, tr.SlotNum, cuPortName);
          }
          cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
          portName = cuPortName.substr(cuPortName.find_first_of("/")+1);
          std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);
        }
        std::string kernelName;
        XCL::RTSingleton::Instance()->getProfileKernelName(deviceName, cuName, kernelName);

        if (showKernelCUNames)
          traceName += ("|" + kernelName + "|" + cuName);

        if (showPortName) {
          rts->getProfileManager()->getArgumentsBank(deviceName, cuName, portName, argNames, memoryName);
          traceName += ("|" + portName + "|" + memoryName);
        }
      }

      if (tr.Type == "Kernel") {
        std::string workGroupSize;
        rts->getProfileManager()->getTraceStringFromComputeUnit(deviceName, cuName, traceName);
        if (traceName.empty()) continue;
        size_t pos = traceName.find_last_of("|");
        workGroupSize = traceName.substr(pos + 1);
        traceName = traceName.substr(0, pos);
        
        writeTableRowStart(getTimelineStream());
        writeTableCells(getTimelineStream(), startStr.str(), traceName, "START", "", workGroupSize);
        writeTableRowEnd(getTimelineStream());

        writeTableRowStart(getTimelineStream());
        writeTableCells(getTimelineStream(), endStr.str(), traceName, "END", "", workGroupSize);
        writeTableRowEnd(getTimelineStream());
        continue;
      }

      double deviceDuration = 1000.0*(tr.End - tr.Start);
      if (!(deviceDuration > 0.0)) deviceDuration = deviceClockDurationUsec;
      writeTableRowStart(getTimelineStream());
      writeTableCells(getTimelineStream(), startStr.str(), traceName,
          tr.Type, argNames, tr.BurstLength, (tr.EndTime - tr.StartTime),
          tr.StartTime, tr.EndTime, deviceDuration,
          startStr.str(), endStr.str());
      writeTableRowEnd(getTimelineStream());
    }
  }

  void WriterI::writeProfileRuleCheckSummary(RTProfile *profile,
      const ProfileRuleChecks::ProfileRuleCheckMap  &deviceExecTimesMap,
      const ProfileRuleChecks::ProfileRuleCheckMap  &computeUnitCallsMap,
      const ProfileRuleChecks::ProfileRuleCheckMap2 &kernelCountsMap)
  {
    // 1. Device execution times
    std::string checkName;
    ProfileRuleChecks::getRuleCheckName(ProfileRuleChecks::DEVICE_EXEC_TIME, checkName);

    auto iter = deviceExecTimesMap.begin();
    for (; iter != deviceExecTimesMap.end(); ++iter) {
      std::string deviceName = iter->first;
      std::string value = iter->second;

      writeTableRowStart(getSummaryStream());
      writeTableCells(getSummaryStream(), checkName, deviceName, value);
      writeTableRowEnd(getSummaryStream());
    }

    // 2. Compute Unit calls
    std::string checkName2;
    ProfileRuleChecks::getRuleCheckName(ProfileRuleChecks::CU_CALLS, checkName2);

    auto iter2 = computeUnitCallsMap.begin();
    for (; iter2 != computeUnitCallsMap.end(); ++iter2) {
      std::string cuName = iter2->first;
      std::string value = iter2->second;

      writeTableRowStart(getSummaryStream());
      writeTableCells(getSummaryStream(), checkName2, cuName, value);
      writeTableRowEnd(getSummaryStream());
    }

    // 3. Global memory bit widths
    std::string checkName3;
    ProfileRuleChecks::getRuleCheckName(ProfileRuleChecks::MEMORY_BIT_WIDTH, checkName3);
    uint32_t bitWidth = profile->getGlobalMemoryBitWidth();

    auto iter3 = deviceExecTimesMap.begin();
    for (; iter3 != deviceExecTimesMap.end(); ++iter3) {
      std::string deviceName = iter3->first;

      writeTableRowStart(getSummaryStream());
      writeTableCells(getSummaryStream(), checkName3, deviceName, bitWidth);
      writeTableRowEnd(getSummaryStream());
    }

    // 4. Usage of MigrateMemObjects
    std::string checkName4;
    ProfileRuleChecks::getRuleCheckName(ProfileRuleChecks::MIGRATE_MEM, checkName4);
    int migrateMemCalls = profile->getMigrateMemCalls();
    writeTableCells(getSummaryStream(), checkName4, "host", migrateMemCalls);
    writeTableRowEnd(getSummaryStream());

    // 5. Usage of memory resources
    std::string checkName5;
    ProfileRuleChecks::getRuleCheckName(ProfileRuleChecks::DDR_BANKS, checkName5);

    auto cuPortsToMemory = profile->getCUPortsToMemoryMap();
    auto memoryIter = cuPortsToMemory.begin();
    for (; memoryIter != cuPortsToMemory.end(); ++memoryIter) {
      writeTableCells(getSummaryStream(), checkName5, memoryIter->first, memoryIter->second);
      writeTableRowEnd(getSummaryStream());
    }

    // 6. Port data widths
    std::string checkName6;
    ProfileRuleChecks::getRuleCheckName(ProfileRuleChecks::PORT_BIT_WIDTH, checkName6);

    auto cuPortVector = profile->getCUPortVector();
    for (auto& cuPort : cuPortVector) {
      auto cu    = std::get<0>(cuPort);
      auto port  = std::get<1>(cuPort);
      std::string portName = cu + "/" + port;
      auto portWidth = std::get<4>(cuPort);
      writeTableCells(getSummaryStream(), checkName6, portName, portWidth);
      writeTableRowEnd(getSummaryStream());
    }

    // 7. Kernel CU counts
    std::string checkName7;
    ProfileRuleChecks::getRuleCheckName(ProfileRuleChecks::KERNEL_COUNT, checkName7);

    for (auto kernelCount : kernelCountsMap) {
      writeTableCells(getSummaryStream(), checkName7, kernelCount.first, kernelCount.second);
      writeTableRowEnd(getSummaryStream());
    }
  }

  // ***********
  // HTML Writer
  // ***********
  HTMLWriter::HTMLWriter(const std::string& summaryFileName, const std::string& timelineFileName,
      const std::string& platformName) :
        SummaryFileName(summaryFileName),
        TimelineFileName(timelineFileName),
        PlatformName(platformName)
  {
    if(SummaryFileName != "") {
      assert(!Summary_ofs.is_open());
      SummaryFileName += FileExtension;
      openStream(Summary_ofs, SummaryFileName);
      writeDocumentHeader(Summary_ofs, "SDAccel Profile Summary");
    }


    if (TimelineFileName != "") {
      assert(!Timeline_ofs.is_open());
      TimelineFileName += FileExtension;
      openStream(Timeline_ofs, TimelineFileName);
      writeDocumentHeader(Timeline_ofs, "SDAccel Timeline Trace");
      std::vector<std::string> TimelineTraceColumnLabels = {
          "Time (msec)", "Name", "Event", "Address/Port",
          "Size (Bytes or Num)", "Latency (cycles)",
          "Start (cycles)", "End (cycles)", "Latency (usec)",
          "Start (msec)", "End (msec)"
      };
      writeTableHeader(Timeline_ofs, "", TimelineTraceColumnLabels);
    }
  }

  HTMLWriter::~HTMLWriter()
  {
    if (Summary_ofs.is_open()) {
      writeDocumentFooter(Summary_ofs);
      Summary_ofs.close();
    }
    if (Timeline_ofs.is_open()) {
      writeTableFooter(Timeline_ofs);
      writeDocumentFooter(Timeline_ofs);
      Timeline_ofs.close();
    }
  }

  void HTMLWriter::writeSummary(RTProfile* profile)
  {
    WriterI::writeSummary(profile);
  }

  void HTMLWriter::writeDocumentFooter(std::ofstream& ofs)
  {
    if (ofs.is_open()) {
      // Close the document
      ofs << "</BODY>" << "\n" << "</HTML>" << "\n";
    }
  }

  void HTMLWriter::writeDocumentHeader(std::ofstream& ofs,
      const std::string& docName)
  {
    if (!ofs.is_open())
      return;

    // Opening of the document
    ofs << "<!DOCTYPE html>" << "\n" << "<HTML>" << "\n" << "<BODY>"
        << "\n";

    // Style Sheet
    ofs << "<STYLE>\n" << "\th1 {\n" << "\t\tfont-size:200%;\n" << "\t}\n";

    ofs << "\ttable th,tr,td {\n";
    ofs << "\t\tborder-collapse: collapse; /* share common border between cells */\n";
    ofs << "\t\tpadding: 4px; /* padding within cells */\n";
    ofs << "\t\ttable-layout : fixed\n";
    ofs << "\t}\n";

    ofs << "\ttable th {\n" << "\tbackground-color:lightsteelblue\n"
        << "\t}\n";
    ofs << "</STYLE>\n";

    // Header of document
    ofs << "<h1>" << docName << "</h1>\n";
    ofs << "<br>\n";
    ofs << "<h3>Generated on: " << WriterI::getCurrentDateTime() << "</h3>\n";
    if (!WriterI::getCurrentExecutableName().empty()) {
      ofs << "<h3>Profiled application: "
          << WriterI::getCurrentExecutableName() << "</h3>\n";
    }
    ofs << "<h3>Target platform: " << PlatformName << "</h3>\n";
    ofs << "<h3>Tool version: " << getToolVersion() << "</h3>\n";
    ofs.flush();
  }

  // Write sub-header to profile summary
  // NOTE: this part of the header must be written after a run is completed.
  void HTMLWriter::writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile)
  {
    if (!ofs.is_open())
      return;

    // Sub-header of document
    ofs << "<h3>Target devices: " << profile->getDeviceNames() << "</h3>\n";

    std::string flowMode;
    XCL::RTSingleton::Instance()->getFlowModeName(flowMode);
    ofs << "<h3>Flow mode: " << flowMode << "</h3>\n";
    ofs << "<br>\n";
    ofs.flush();
  }

  void HTMLWriter::writeTableHeader(
      std::ofstream& ofs, const std::string& caption,
      const std::vector<std::string>& columnLabels)
  {
    if (!ofs.is_open())
      return;

    ofs.flush();
    ofs << "<br>\n";
    ofs << "<h2>" << caption << "</h2>\n";

    ofs << "\n<TABLE border=\"1\">\n";
    ofs << "<TR>\n";
    for (auto str : columnLabels) {
      ofs << "<TH>" << str << "</TH>\n";
    }
    ofs << "</TR>\n";
    ofs.flush();
  }

  // **********
  // CSV Writer
  // **********
  CSVWriter::CSVWriter(const std::string& summaryFileName, const std::string& timelineFileName,
      const std::string& platformName) :
        SummaryFileName(summaryFileName),
        TimelineFileName(timelineFileName),
        PlatformName(platformName)
  {
    if(SummaryFileName != "") {
      assert(!Summary_ofs.is_open());
      SummaryFileName += FileExtension;
      openStream(Summary_ofs, SummaryFileName);
      writeDocumentHeader(Summary_ofs, "SDAccel Profile Summary");
    }

    if (TimelineFileName != "") {
      assert(!Timeline_ofs.is_open());
      TimelineFileName += FileExtension;
      openStream(Timeline_ofs, TimelineFileName);
      writeDocumentHeader(Timeline_ofs, "SDAccel Timeline Trace");
      std::vector<std::string> TimelineTraceColumnLabels = {
          "Time_msec", "Name", "Event", "Address_Port", "Size",
          "Latency_cycles", "Start_cycles", "End_cycles",
          "Latency_usec", "Start_msec", "End_msec"
      };
      writeTableHeader(Timeline_ofs, "", TimelineTraceColumnLabels);

    }
  }

  CSVWriter::~CSVWriter()
  {
    if (Summary_ofs.is_open()) {
      writeDocumentFooter(Summary_ofs);
      Summary_ofs.close();
    }
    if (Timeline_ofs.is_open()) {
      writeTimelineFooter(Timeline_ofs);
      Timeline_ofs.close();
    }
  }

  void CSVWriter::writeSummary(RTProfile* profile)
  {
    WriterI::writeSummary(profile);

    //Table 7: Top Kernel Summary.
    std::vector<std::string> TopKernelSummaryColumnLabels = {
        "Kernel Instance Address", "Kernel", "Context ID", "Command Queue ID",
        "Device", "Start Time (ms)", "Duration (ms)",
        "Global Work Size", "Local Work Size"};
    writeTableHeader(getSummaryStream(), "Top Kernel Execution",
        TopKernelSummaryColumnLabels);
    profile->writeTopKernelSummary(this);
    writeTableFooter(getSummaryStream());

    //Table 8: Top Buffer Write Summary
    std::vector<std::string> TopBufferWritesColumnLabels = {
        "Buffer Address", "Context ID", "Command Queue ID", "Start Time (ms)",
        "Duration (ms)", "Buffer Size (KB)", "Writing Rate(MB/s)"};
    writeTableHeader(getSummaryStream(), "Top Buffer Writes",
        TopBufferWritesColumnLabels);
    profile->writeTopDataTransferSummary(this, false); // Writes
    writeTableFooter(getSummaryStream());

    //Table 9: Top Buffer Read Summary
    std::vector<std::string> TopBufferReadsColumnLabels = {
        "Buffer Address", "Context ID", "Command Queue ID", "Start Time (ms)",
        "Duration (ms)", "Buffer Size (KB)", "Reading Rate(MB/s)"};
    writeTableHeader(getSummaryStream(), "Top Buffer Reads",
        TopBufferReadsColumnLabels);
    profile->writeTopDataTransferSummary(this, true); // Reads
    writeTableFooter(getSummaryStream());

    //Table 10: Parameters used in PRCs
    std::vector<std::string> PRCSummaryColumnLabels = {
      "Parameter", "Element", "Value"
    };
    writeTableHeader(getSummaryStream(), "PRC Parameters", PRCSummaryColumnLabels);
    profile->writeProfileRuleCheckSummary(this);
    writeTableFooter(getSummaryStream());
  }

  void CSVWriter::writeDocumentHeader(std::ofstream& ofs,
      const std::string& docName)
  {
    if (!ofs.is_open())
      return;

    // Header of document
    ofs << docName << "\n";
    ofs << "Generated on: " << WriterI::getCurrentDateTime() << "\n";
    ofs << "Msec since Epoch: " << WriterI::getCurrentTimeMsec() << "\n";
    if (!WriterI::getCurrentExecutableName().empty()) {
      ofs << "Profiled application: " << WriterI::getCurrentExecutableName() << "\n";
    }
    ofs << "Target platform: " << PlatformName << "\n";
    ofs << "Tool version: " << getToolVersion() << "\n";
  }

  // Write sub-header to profile summary
  // NOTE: this part of the header must be written after a run is completed.
  void CSVWriter::writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile)
  {
    if (!ofs.is_open())
      return;

    // Sub-header of profile summary
    ofs << "Target devices: " << profile->getDeviceNames() << "\n";

    std::string flowMode;
    XCL::RTSingleton::Instance()->getFlowModeName(flowMode);
    ofs << "Flow mode: " << flowMode << "\n";
  }

  void CSVWriter::writeTableHeader(
      std::ofstream& ofs, const std::string& caption,
      const std::vector<std::string>& columnLabels)
  {
    if (!ofs.is_open())
      return;

    ofs << "\n" << caption << "\n";
    for (const auto& str : columnLabels) {
      ofs << str << ",";
    }
    ofs << "\n";
  }

  void CSVWriter::writeDocumentFooter(std::ofstream& ofs)
  {
    if (ofs.is_open()) {
      // Close the document
      ofs << "\n";
    }
  }

  void CSVWriter::writeTimelineFooter(std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    auto rts = XCL::RTSingleton::Instance();
    auto profile = rts->getProfileManager();

    ofs << "Footer,begin\n";

    //
    // Settings (project name, stalls, target, & platform)
    //
    std::string projectName = profile->getProjectName();
    ofs << "Project," << projectName << ",\n";

    std::string stallProfiling = (profile->getStallTrace() == XCL::RTProfile::STALL_TRACE_OFF) ?
        "false" : "true";
    ofs << "Stall profiling," << stallProfiling << ",\n";

    std::string flowMode;
    rts->getFlowModeName(flowMode);
    ofs << "Target," << flowMode << ",\n";

    std::string deviceNames = profile->getDeviceNames("|");
    ofs << "Platform," << deviceNames << ",\n";

    for (auto& threadId : profile->getThreadIds())
      ofs << "Read/Write Thread," << std::showbase << std::hex << std::uppercase
	      << threadId << std::endl;

    //
    // Platform/device info
    //
    auto platform = rts->getcl_platform_id();
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      ofs << "Device," << deviceName << ",begin\n";

      // DDR Bank addresses
      // TODO: this assumes start address of 0x0 and evenly divided banks
      unsigned int ddrBanks = device_id->get_ddr_bank_count();
      if (ddrBanks == 0) ddrBanks = 1;
      size_t ddrSize = device_id->get_ddr_size();
      size_t bankSize = ddrSize / ddrBanks;
      ofs << "DDR Banks,begin\n";
      for (unsigned int b=0; b < ddrBanks; ++b)
        ofs << "Bank," << std::dec << b << ","
		    << (boost::format("0X%09x") % (b * bankSize)) << std::endl;
      ofs << "DDR Banks,end\n";

#if 0
      std::string binaryName = "binary_container";
      auto deviceIter = DeviceBinaryNameMap.find(deviceName);
      if (deviceIter != DeviceBinaryNameMap.end())
        binaryName = deviceIter->second;
      ofs << "BinaryContainer," << binaryName << ",begin\n";

      // Traverse all CUs on current device
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        std::string cuName = cu->getname();
        std::string kernelName;
        profile->getKernelFromComputeUnit(cuName, kernelName);

        ofs << "ComputeUnit," << cuName << "|" << kernelName << ",begin\n";
        // TODO: add any CU-specific values
        ofs << "ComputeUnit," << cuName << "|" << kernelName << ",end\n";
      }

      ofs << "BinaryContainer," << binaryName << ",end\n";
#endif
      ofs << "Device," << deviceName << ",end\n";
    }

    //
    // Unused CUs
    //
    //auto platform = rts->getcl_platform_id();
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      if (!profile->isDeviceActive(deviceName))
        continue;

      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto cuName = cu->get_name();

        if (profile->getComputeUnitCalls(deviceName, cuName) == 0)
          ofs << "UnusedComputeUnit," << cuName << ",\n";
      }
    }

    ofs << "Footer,end\n";

    writeDocumentFooter(ofs);
  }
  
  // ******************
  // Unified CSV Writer
  // ******************
  UnifiedCSVWriter::UnifiedCSVWriter(const std::string& summaryFileName, 
      const std::string& timelineFileName, const std::string& platformName) :
        SummaryFileName(summaryFileName),
        PlatformName(platformName)
  {
    if (SummaryFileName != "") {
      assert(!Summary_ofs.is_open());
      SummaryFileName += FileExtension;
      openStream(Summary_ofs, SummaryFileName);
      writeDocumentHeader(Summary_ofs, "SDx Profile Summary");
    }
    
    // TODO: for now, timeline file is ignored
  }

  UnifiedCSVWriter::~UnifiedCSVWriter()
  {
    if (Summary_ofs.is_open()) {
      writeDocumentFooter(Summary_ofs);
      Summary_ofs.close();
    }
  }

  void UnifiedCSVWriter::writeSummary(RTProfile* profile)
  {
    // Sub-header
    writeDocumentSubHeader(getSummaryStream(), profile);
  
    // Table 1: Software Functions
    std::vector<std::string> SoftwareFunctionColumnLabels = { 
        "Function", "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)" };

    writeTableHeader(getSummaryStream(), "Software Functions", SoftwareFunctionColumnLabels);
    profile->writeAPISummary(this);
    writeTableFooter(getSummaryStream());

    // Table 2: Hardware Functions
    std::vector<std::string> HardwareFunctionColumnLabels = {
        "Function", "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)", 
        "Average Time (ms)", "Maximum Time (ms)" };

    std::string table2Caption = (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) ?
        "Hardware Functions (includes estimated device times)" : "Hardware Functions";
    writeTableHeader(getSummaryStream(), table2Caption, HardwareFunctionColumnLabels);
    profile->writeKernelSummary(this);
    writeTableFooter(getSummaryStream());

    // Table 3: Hardware Accelerators
    std::vector<std::string> HardwareAcceleratorColumnLabels = {
        "Location", "Accelerator", "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)", "Clock Frequency (MHz)" };

    std::string table3Caption = (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) ?
        "Hardware Accelerators (includes estimated device times)" : "Hardware Accelerators";
    writeTableHeader(getSummaryStream(), table3Caption, HardwareAcceleratorColumnLabels);
    profile->writeAcceleratorSummary(this);
    writeTableFooter(getSummaryStream());

    // Table 4: Top Hardware Function Executions
    std::vector<std::string> TopHardwareColumnLabels = {
        "Location", "Function", "Start Time (ms)", "Duration (ms)"};
    writeTableHeader(getSummaryStream(), "Top Hardware Function Executions",
        TopHardwareColumnLabels);
    profile->writeTopHardwareSummary(this);
    writeTableFooter(getSummaryStream());

    // Table 5: Data Transfer: Accelerators and DDR Memory
    std::vector<std::string> AcceleratorTransferColumnLabels = {
        "Location", "Accelerator/Port Name", "Accelerator Arguments", "Memory Resources",
		"Transfer Type", "Number Of Transfers", "Transfer Rate (MB/s)",
		"Average Bandwidth Utilization (%)", "Average Size (KB)", "Average Latency (ns)"
    };
    writeTableHeader(getSummaryStream(), "Data Transfer: Accelerators and DDR Memory",
        AcceleratorTransferColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeKernelTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());

    // Table 6: Top Data Transfer: Accelerators and DDR Memory
    std::vector<std::string> TopAcceleratorTransferColumnLabels = {
        "Location", "Accelerator", "Number of Transfers", "Average Bytes per Transfer",
        "Transfer Efficiency (%)", "Total Data Transfer (MB)", "Total Write (MB)",
        "Total Read (MB)", "Total Transfer Rate (MB/s)"
    };
    writeTableHeader(getSummaryStream(), "Top Data Transfer: Accelerators and DDR Memory",
        TopAcceleratorTransferColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeTopKernelTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());
    
    // Table 7: Data Transfer: Host and DDR Memory
    std::vector<std::string> HostTransferColumnLabels = {
        "Transfer Type", "Number Of Transfers", "Transfer Rate (MB/s)", 
        "Average Bandwidth Utilization (%)", "Average Size (KB)", "Average Time (ms)"
    };
    writeTableHeader(getSummaryStream(), "Data Transfer: Host and DDR Memory",
        HostTransferColumnLabels);
    if (XCL::RTSingleton::Instance()->getFlowMode() != XCL::RTSingleton::CPU
        && XCL::RTSingleton::Instance()->getFlowMode() != XCL::RTSingleton::COSIM_EM) {
      profile->writeHostTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());

    // Table 8: Top Memory Writes
    std::vector<std::string> TopHostWriteColumnLabels = {
        "Address", "Start Time (ms)", "Duration (ms)", 
        "Size (KB)", "Transfer Rate (MB/s)"};
    writeTableHeader(getSummaryStream(), "Top Memory Writes: Host and DDR Memory",
        TopHostWriteColumnLabels);
    profile->writeTopDataTransferSummary(this, false); // Writes
    writeTableFooter(getSummaryStream());

    // Table 9: Top Memory Reads
    std::vector<std::string> TopHostReadColumnLabels = {
        "Address", "Start Time (ms)", "Duration (ms)", 
        "Size (KB)", "Transfer Rate (MB/s)"};
    writeTableHeader(getSummaryStream(), "Top Memory Reads: Host and DDR Memory",
        TopHostReadColumnLabels);
    profile->writeTopDataTransferSummary(this, true); // Reads
    writeTableFooter(getSummaryStream());

    // Table 10: Parameters used in PRCs
    std::vector<std::string> PRCParameterColumnLabels = {
      "Parameter", "Element", "Value"
    };
    writeTableHeader(getSummaryStream(), "PRC Parameters", PRCParameterColumnLabels);
    profile->writeProfileRuleCheckSummary(this);
    writeTableFooter(getSummaryStream());
  }

  void UnifiedCSVWriter::writeDocumentHeader(std::ofstream& ofs,
      const std::string& docName)
  {
    if (!ofs.is_open())
      return;

    // Header of document
    ofs << docName << "\n";
    ofs << "Generated on: " << WriterI::getCurrentDateTime() << "\n";
    ofs << "Msec since Epoch: " << WriterI::getCurrentTimeMsec() << "\n";
    if (!WriterI::getCurrentExecutableName().empty()) {
      ofs << "Profiled application: " << WriterI::getCurrentExecutableName() << "\n";
    }
    ofs << "Target platform: " << PlatformName << "\n";
    ofs << "Tool version: " << getToolVersion() << "\n";
  }

  // Write sub-header to profile summary
  // NOTE: this part of the header must be written after a run is completed.
  void UnifiedCSVWriter::writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile)
  {
    if (!ofs.is_open())
      return;

    // Sub-header of profile summary
    ofs << "Target devices: " << profile->getDeviceNames() << "\n";

    std::string flowMode;
    XCL::RTSingleton::Instance()->getFlowModeName(flowMode);
    ofs << "Flow mode: " << flowMode << "\n";
  }

  void UnifiedCSVWriter::writeTableHeader(
      std::ofstream& ofs, const std::string& caption,
      const std::vector<std::string>& columnLabels)
  {
    if (!ofs.is_open())
      return;

    ofs << "\n" << caption << "\n";
    for (const auto& str : columnLabels) {
      ofs << str << ",";
    }
    ofs << "\n";
  }

  void UnifiedCSVWriter::writeDocumentFooter(std::ofstream& ofs)
  {
    if (ofs.is_open()) {
      // Close the document
      ofs << "\n";
    }
  }
  
  // Write top kernel summary
  void UnifiedCSVWriter::writeSummary(const KernelTrace& trace)
  {
    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), trace.getDeviceName(), trace.getKernelName(),
        trace.getStart(), trace.getDuration());
    writeTableRowEnd(getSummaryStream());
  }

  // Write top buffer summary (host to global memory)
  void UnifiedCSVWriter::writeSummary(const BufferTrace& trace)
  {
    std::string durationStr = std::to_string( trace.getDuration() );
    double rate = (double)(trace.getSize()) / (1000.0 * trace.getDuration());
    std::string rateStr = std::to_string(rate);
    if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::CPU
        || XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::COSIM_EM
        || XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) {
      durationStr = "N/A";
      rateStr = "N/A";
    }

    writeTableRowStart(getSummaryStream());

    writeTableCells(getSummaryStream(), trace.getAddress(), trace.getStart(),
        durationStr, (double)(trace.getSize())/1000.0, rateStr);

    writeTableRowEnd(getSummaryStream());
  }
  
  // Table 6: Data Transfer: Top Kernel & Global
  // Location, Accelerator, Number of Transfers, Average Bytes per Transfer,
  // Total Data Transfer (MB), Total Write (MB), Total Read (MB), Total Transfer Rate (MB/s)
  void UnifiedCSVWriter::writeTopKernelTransferSummary(
      const std::string& deviceName, const std::string& accelName,
      uint64_t totalWriteBytes, uint64_t totalReadBytes,
      uint64_t totalWriteTranx, uint64_t totalReadTranx,
      double totalWriteTimeMsec, double totalReadTimeMsec,
      uint32_t maxBytesPerTransfer, double maxTransferRateMBps)
  {
    double totalTimeMsec = (totalWriteTimeMsec > totalReadTimeMsec) ?
        totalWriteTimeMsec : totalReadTimeMsec;

    double transferRateMBps = (totalTimeMsec == 0) ? 0.0 :
        (double)(totalReadBytes + totalWriteBytes) / (1000.0 * totalTimeMsec);
#if 0
    double aveBWUtil = (100.0 * transferRateMBps) / maxTransferRateMBps;
    if (aveBWUtil > 100.0)
      aveBWUtil = 100.0;
#endif

    double aveBytesPerTransfer = ((totalReadTranx + totalWriteTranx) == 0) ? 0.0 :
        (double)(totalReadBytes + totalWriteBytes) / (totalReadTranx + totalWriteTranx);
    double transferEfficiency = (100.0 * aveBytesPerTransfer) / maxBytesPerTransfer;
    if (transferEfficiency > 100.0)
      transferEfficiency = 100.0;

    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(),
        deviceName, accelName, totalReadTranx + totalWriteTranx,
        aveBytesPerTransfer, transferEfficiency,
        (double)(totalReadBytes + totalWriteBytes) / 1.0e6,
        (double)(totalWriteBytes) / 1.0e6, (double)(totalReadBytes) / 1.0e6,
        transferRateMBps);
    writeTableRowEnd(getSummaryStream());
  }

  // Table 7: Data Transfer: Host & DDR Memory
  // Transfer Type, Number Of Transfers, Transfer Rate (MB/s),
  // Average Bandwidth Utilization (%), Average Size (KB), Average Time (ms)
  void UnifiedCSVWriter::writeHostTransferSummary(const std::string& name,
      const BufferStats& stats, uint64_t totalBytes, uint64_t totalTranx,
      double totalTimeMsec, double maxTransferRateMBps)
  {
    //double aveTimeMsec = stats.getAveTime();
    double aveTimeMsec = (totalTranx == 0) ? 0.0 : totalTimeMsec / totalTranx;

    // Get min/average/max bytes per transaction
    // NOTE: to remove the dependency on trace, we calculate it based on counter values
    //       also, v1.1 of Alpha Data DSA has incorrect AXI lengths so these will always be 16K
#if 0
    double minBytes = (double)(stats.getMin());
    double aveBytes = (double)(stats.getAverage());
    double maxBytes = (double)(stats.getMax());
#else
    double aveBytes = (totalTranx == 0) ? 0.0 : (double)(totalBytes) / totalTranx;
    //double minBytes = aveBytes;
    //double maxBytes = aveBytes;
#endif

    double transferRateMBps = (totalTimeMsec == 0) ? 0.0 :
        totalBytes / (1000.0 * totalTimeMsec);
    double aveBWUtil = (100.0 * transferRateMBps) / maxTransferRateMBps;
    if (aveBWUtil > 100.0)
      aveBWUtil = 100.0;

    if (aveBWUtil > 0) {
      XDP_LOG("%s: Transfered %u bytes in %.3f msec\n", name.c_str(), totalBytes, totalTimeMsec);
      XDP_LOG("  AveBWUtil = %.3f = %.3f / %.3f\n", aveBWUtil, transferRateMBps, maxTransferRateMBps);
    }

    // Don't show these values for HW emulation
    std::string transferRateStr = std::to_string(transferRateMBps);
    std::string aveBWUtilStr = std::to_string(aveBWUtil);
    std::string totalTimeStr = std::to_string(totalTimeMsec);
    std::string aveTimeStr = std::to_string(aveTimeMsec);
    if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) {
      transferRateStr = "N/A";
      aveBWUtilStr = "N/A";
      totalTimeStr = "N/A";
      aveTimeStr = "N/A";
    }

    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), name, totalTranx, transferRateStr, 
        aveBWUtilStr, aveBytes/1000.0, aveTimeStr);

    writeTableRowEnd(getSummaryStream());
  }
}


