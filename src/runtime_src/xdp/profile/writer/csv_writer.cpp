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

#include "csv_writer.h"
#include "xdp/rt_singleton.h"

namespace XCL {

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