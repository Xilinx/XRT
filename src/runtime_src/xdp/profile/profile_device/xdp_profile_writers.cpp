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

#include "xdp_profile.h"
#include "xdp_profile_writers.h"
#include "xdp_profile_results.h"
#include "driver/include/xclperf.h"

namespace XDP {
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

  void WriterI::writeSummary(XDPProfile* profile)
  {
    auto flowMode = profile->getFlowMode();

    // Sub-header
    writeDocumentSubHeader(getSummaryStream(), profile);

    // Table: Kernel Execution Summary
    std::vector<std::string> KernelExecutionSummaryColumnLabels = {
        "Kernel", "Number Of Enqueues", "Total Time (ms)",
        "Minimum Time (ms)", "Average Time (ms)", "Maximum Time (ms)" };

    std::string table2Caption = (flowMode == XDP::XDPProfile::HW_EM) ?
        "Kernel Execution (includes estimated device times)" : "Kernel Execution";
    writeTableHeader(getSummaryStream(), table2Caption, KernelExecutionSummaryColumnLabels);
    profile->writeKernelSummary(this);
    writeTableFooter(getSummaryStream());

    // Table: Compute Unit Utilization
    std::vector<std::string> ComputeUnitExecutionSummaryColumnLabels = {
        "Device", "Compute Unit", "Kernel", "Global Work Size", "Local Work Size",
        "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)", "Clock Frequency (MHz)" };

    std::string table3Caption = (flowMode == XDP::XDPProfile::HW_EM) ?
        "Compute Unit Utilization (includes estimated device times)" : "Compute Unit Utilization";
    writeTableHeader(getSummaryStream(), table3Caption, ComputeUnitExecutionSummaryColumnLabels);
    profile->writeComputeUnitSummary(this);
    writeTableFooter(getSummaryStream());

#if 0
    // Table 4: CU Stalls only for HW Runs
    // NOTE: only display this table if
    //   * device counter profiling is turned on (default: true)
    //   * it was run on a board
    //   * at least one device has stall profiling in the dynamic region
    unsigned numStallSlots = 0;
    auto platform = rts->getcl_platform_id();
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      numStallSlots += rts->getProfileNumberSlots(XCL_PERF_MON_STALL, deviceName);
    }

    if (profile->isDeviceProfileOn() && 
      (flowMode == XDP::XDPProfile::DEVICE) &&
      (numStallSlots > 0)) {
      std::vector<std::string> KernelStallLabels = {
        "Compute Unit", "Execution Count", "Running Time (ms)", "Intra-Kernel Dataflow Stalls (ms)", 
        "External Memory Stalls (ms)", "Inter-Kernel Pipe Stalls (ms)"
      };

      writeTableHeader(getSummaryStream(), "Compute Units: Stall Information", KernelStallLabels);
      profile->writeStallSummary(this);
      writeTableFooter(getSummaryStream());
    }
#endif

    // Table: Data Transfer: Host & Global
    std::vector<std::string> DataTransferSummaryColumnLabels = {
        "Context:Number of Devices", "Transfer Type", "Number Of Transfers",
        "Transfer Rate (MB/s)", "Average Bandwidth Utilization (%)",
        "Average Size (KB)", "Total Time (ms)", "Average Time (ms)"
    };
    writeTableHeader(getSummaryStream(), "Data Transfer: Host and Global Memory",
        DataTransferSummaryColumnLabels);
    if (flowMode != XDP::XDPProfile::CPU) {
      profile->writeHostTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());

    // Table: Data Transfer: Kernel & Global
    std::vector<std::string> KernelDataTransferSummaryColumnLabels = {
        "Device", "Compute Unit/Port Name", "Kernel Arguments", "DDR Bank",
		"Transfer Type", "Number Of Transfers", "Transfer Rate (MB/s)",
		"Average Bandwidth Utilization (%)", "Average Size (KB)", "Average Latency (ns)"
    };
    writeTableHeader(getSummaryStream(), "Data Transfer: Kernels and Global Memory",
        KernelDataTransferSummaryColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeKernelTransferSummary(this);
    }
    writeTableFooter(getSummaryStream());

    // Table: Top Data Transfer: Kernel & Global
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
    double minBytes = aveBytes;
    double maxBytes = aveBytes;
#endif

    double transferRateMBps = (totalTimeMsec == 0) ? 0.0 :
        totalBytes / (1000.0 * totalTimeMsec);
    double aveBWUtil = (100.0 * transferRateMBps) / maxTransferRateMBps;
    if (aveBWUtil > 100.0)
      aveBWUtil = 100.0;

    if (aveBWUtil > 0) {
      printf("%s: Transfered %u bytes in %.3f msec\n", name.c_str(), totalBytes, totalTimeMsec);
      printf("  AveBWUtil = %.3f = %.3f / %.3f\n", aveBWUtil, transferRateMBps, maxTransferRateMBps);
    }

    // Don't show these values for HW emulation
    std::string transferRateStr = std::to_string(transferRateMBps);
    std::string aveBWUtilStr = std::to_string(aveBWUtil);
    std::string totalTimeStr = std::to_string(totalTimeMsec);
    std::string aveTimeStr = std::to_string(aveTimeMsec);
#if 0
    if (XDP::XDPProfile::Instance()->getFlowMode() == XDP::XDPProfile::HW_EM) {
      transferRateStr = "N/A";
      aveBWUtilStr = "N/A";
      totalTimeStr = "N/A";
      aveTimeStr = "N/A";
    }
#endif

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
      const std::string& cuPortName, const std::string& argNames, uint32_t ddrBank,
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
    double minBytes = aveBytes;
    double maxBytes = aveBytes;
#endif

    double transferRateMBps = (totalKernelTimeMsec == 0) ? 0.0 :
        totalBytes / (1000.0 * totalKernelTimeMsec);
    double aveBWUtil = (100.0 * transferRateMBps) / maxTransferRateMBps;
    if (aveBWUtil > 100.0)
      aveBWUtil = 100.0;

    if (aveBWUtil > 0) {
      printf("Kernel %s: Transfered %u bytes in %.3f msec (device: %s)\n",
          transferType.c_str(), totalBytes, totalKernelTimeMsec, deviceName.c_str());
      printf("  AveBWUtil = %.3f = %.3f / %.3f\n",
          aveBWUtil, transferRateMBps, maxTransferRateMBps);
    }

    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), deviceName, cuPortName, argNames, ddrBank,
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
#if 0
    if (XDP::XDPProfile::Instance()->getFlowMode() == XDP::XDPProfile::CPU
        || XDP::XDPProfile::Instance()->getFlowMode() == XDP::XDPProfile::COSIM_EM
        || XDP::XDPProfile::Instance()->getFlowMode() == XDP::XDPProfile::HW_EM) {
      durationStr = "N/A";
      rateStr = "N/A";
    }
#endif

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

  void WriterI::writeSummary(const std::string& name,
      const BufferStats& stats)
  {
    writeTableRowStart(getSummaryStream());
    writeTableCells(getSummaryStream(), name, stats.getCount(),
        stats.getTotalTime(), stats.getAveTime(),
        stats.getAveTransferRate(), (double)(stats.getMin())/1000.0,
        (double)(stats.getAverage())/1000.0, (double)(stats.getMax())/1000.0);
    writeTableRowEnd(getSummaryStream());
  }

  // **********
  // CSV Writer
  // **********
  CSVWriter::CSVWriter(const std::string& summaryFileName, const std::string& platformName) :
    SummaryFileName(summaryFileName),
    PlatformName(platformName)
  {
    if (SummaryFileName != "") {
      assert(!Summary_ofs.is_open());
      SummaryFileName += FileExtension;
      openStream(Summary_ofs, SummaryFileName);
      writeDocumentHeader(Summary_ofs, "XMA Profile Summary");
    }
  }

  CSVWriter::~CSVWriter()
  {
    if (Summary_ofs.is_open()) {
      writeDocumentFooter(Summary_ofs);
      Summary_ofs.close();
    }
  }

  void CSVWriter::writeSummary(XDPProfile* profile)
  {
    WriterI::writeSummary(profile);

    // Table: Top Kernel Summary
    std::vector<std::string> TopKernelSummaryColumnLabels = {
        "Kernel Instance Address", "Kernel", "Context ID", "Command Queue ID",
        "Device", "Start Time (ms)", "Duration (ms)",
        "Global Work Size", "Local Work Size"};
    writeTableHeader(getSummaryStream(), "Top Kernel Execution",
        TopKernelSummaryColumnLabels);
    profile->writeTopKernelSummary(this);
    writeTableFooter(getSummaryStream());

    // Table: Top Buffer Write Summary
    std::vector<std::string> TopBufferWritesColumnLabels = {
        "Buffer Address", "Context ID", "Command Queue ID", "Start Time (ms)",
        "Duration (ms)", "Buffer Size (KB)", "Writing Rate(MB/s)"};
    writeTableHeader(getSummaryStream(), "Top Buffer Writes",
        TopBufferWritesColumnLabels);
    profile->writeTopDataTransferSummary(this, false); // Writes
    writeTableFooter(getSummaryStream());

    // Table: Top Buffer Read Summary
    std::vector<std::string> TopBufferReadsColumnLabels = {
        "Buffer Address", "Context ID", "Command Queue ID", "Start Time (ms)",
        "Duration (ms)", "Buffer Size (KB)", "Reading Rate(MB/s)"};
    writeTableHeader(getSummaryStream(), "Top Buffer Reads",
        TopBufferReadsColumnLabels);
    profile->writeTopDataTransferSummary(this, true); // Reads
    writeTableFooter(getSummaryStream());

#if 0
    // Table: Parameters used in PRCs
    std::vector<std::string> PRCSummaryColumnLabels = {
      "Parameter", "Element", "Value"
    };
    writeTableHeader(getSummaryStream(), "PRC Parameters", PRCSummaryColumnLabels);
    profile->writeProfileRuleCheckSummary(this);
    writeTableFooter(getSummaryStream());
#endif
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
  void CSVWriter::writeDocumentSubHeader(std::ofstream& ofs, XDPProfile* profile)
  {
    if (!ofs.is_open())
      return;

    // Sub-header of profile summary
    ofs << "Target devices: " << profile->getDeviceName() << "\n";

    std::string flowMode;
    profile->getFlowModeName(flowMode);
    ofs << "Flow mode: " << flowMode << "\n";
  }

  void CSVWriter::writeTableHeader(std::ofstream& ofs, const std::string& caption,
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
}


