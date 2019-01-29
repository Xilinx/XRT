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

#include "unified_csv_profile.h"
#include "util.h"

namespace xdp {
  // **************************
  // Unified CSV Profile Writer
  // **************************
  UnifiedCSVProfileWriter::UnifiedCSVProfileWriter(const std::string& summaryFileName,
                                                   const std::string& platformName,
                                                   XDPPluginI* Plugin) :
      SummaryFileName(summaryFileName),
      PlatformName(platformName)
  {
    mPluginHandle = Plugin;
    if (SummaryFileName != "") {
      assert(!Summary_ofs.is_open());
      SummaryFileName += FileExtension;
      openStream(Summary_ofs, SummaryFileName);
      writeDocumentHeader(Summary_ofs, "SDx Profile Summary");
    }
  }

  UnifiedCSVProfileWriter::~UnifiedCSVProfileWriter()
  {
    if (Summary_ofs.is_open()) {
      writeDocumentFooter(Summary_ofs);
      Summary_ofs.close();
    }
  }

  void UnifiedCSVProfileWriter::writeSummary(RTProfile* profile)
  {
    // Sub-header
    writeDocumentSubHeader(getStream(), profile);
  
    // Table 1: Software Functions
    std::vector<std::string> SoftwareFunctionColumnLabels = { 
        "Function", "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)" };

    writeTableHeader(getStream(), "Software Functions", SoftwareFunctionColumnLabels);
    profile->writeAPISummary(this);
    writeTableFooter(getStream());

    // Table 2: Hardware Functions
    std::vector<std::string> HardwareFunctionColumnLabels = {
        "Function", "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)", 
        "Average Time (ms)", "Maximum Time (ms)" };

    std::string table2Caption = (mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM) ?
        "Hardware Functions (includes estimated device times)" : "Hardware Functions";
    writeTableHeader(getStream(), table2Caption, HardwareFunctionColumnLabels);
    profile->writeKernelSummary(this);
    writeTableFooter(getStream());

    // Table 3: Hardware Accelerators
    std::vector<std::string> HardwareAcceleratorColumnLabels = {
        "Location", "Accelerator", "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)", "Clock Frequency (MHz)" };

    std::string table3Caption = (mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM) ?
        "Hardware Accelerators (includes estimated device times)" : "Hardware Accelerators";
    writeTableHeader(getStream(), table3Caption, HardwareAcceleratorColumnLabels);
    profile->writeAcceleratorSummary(this);
    writeTableFooter(getStream());

    // Table 4: Top Hardware Function Executions
    std::vector<std::string> TopHardwareColumnLabels = {
        "Location", "Function", "Start Time (ms)", "Duration (ms)"};
    writeTableHeader(getStream(), "Top Hardware Function Executions",
        TopHardwareColumnLabels);
    profile->writeTopHardwareSummary(this);
    writeTableFooter(getStream());

    // Table 5: Data Transfer: Accelerators and DDR Memory
    std::vector<std::string> AcceleratorTransferColumnLabels = {
        "Location", "Accelerator/Port Name", "Accelerator Arguments", "Memory Resources",
		"Transfer Type", "Number Of Transfers", "Transfer Rate (MB/s)",
		"Average Bandwidth Utilization (%)", "Average Size (KB)", "Average Latency (ns)"
    };
    writeTableHeader(getStream(), "Data Transfer: Accelerators and DDR Memory",
        AcceleratorTransferColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeKernelTransferSummary(this);
    }
    writeTableFooter(getStream());

    // Table 6: Top Data Transfer: Accelerators and DDR Memory
    std::vector<std::string> TopAcceleratorTransferColumnLabels = {
        "Location", "Accelerator", "Number of Transfers", "Average Bytes per Transfer",
        "Transfer Efficiency (%)", "Total Data Transfer (MB)", "Total Write (MB)",
        "Total Read (MB)", "Total Transfer Rate (MB/s)"
    };
    writeTableHeader(getStream(), "Top Data Transfer: Accelerators and DDR Memory",
        TopAcceleratorTransferColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeTopKernelTransferSummary(this);
    }
    writeTableFooter(getStream());
    
    // Table 7: Data Transfer: Host and DDR Memory
    std::vector<std::string> HostTransferColumnLabels = {
        "Transfer Type", "Number Of Transfers", "Transfer Rate (MB/s)", 
        "Average Bandwidth Utilization (%)", "Average Size (KB)", "Average Time (ms)"
    };
    writeTableHeader(getStream(), "Data Transfer: Host and DDR Memory",
        HostTransferColumnLabels);
    if (mPluginHandle->getFlowMode() != xdp::RTUtil::CPU
        && mPluginHandle->getFlowMode() != xdp::RTUtil::COSIM_EM) {
      profile->writeHostTransferSummary(this);
    }
    writeTableFooter(getStream());

    // Table 8: Top Memory Writes
    std::vector<std::string> TopHostWriteColumnLabels = {
        "Address", "Start Time (ms)", "Duration (ms)", 
        "Size (KB)", "Transfer Rate (MB/s)"};
    writeTableHeader(getStream(), "Top Memory Writes: Host and DDR Memory",
        TopHostWriteColumnLabels);
    profile->writeTopDataTransferSummary(this, false); // Writes
    writeTableFooter(getStream());

    // Table 9: Top Memory Reads
    std::vector<std::string> TopHostReadColumnLabels = {
        "Address", "Start Time (ms)", "Duration (ms)", 
        "Size (KB)", "Transfer Rate (MB/s)"};
    writeTableHeader(getStream(), "Top Memory Reads: Host and DDR Memory",
        TopHostReadColumnLabels);
    profile->writeTopDataTransferSummary(this, true); // Reads
    writeTableFooter(getStream());

    // Table 10: Parameters used in PRCs
    std::vector<std::string> PRCParameterColumnLabels = {
      "Parameter", "Element", "Value"
    };
    writeTableHeader(getStream(), "PRC Parameters", PRCParameterColumnLabels);
    writeGuidanceMetadataSummary(profile, mPluginHandle->getDeviceExecTimesMap(), 
                                          mPluginHandle->getComputeUnitCallsMap(),
                                          mPluginHandle->getKernelCountsMap());
    writeTableFooter(getStream());
  }

  void UnifiedCSVProfileWriter::writeDocumentHeader(std::ofstream& ofs,
      const std::string& docName)
  {
    if (!ofs.is_open())
      return;

    // Header of document
    ofs << docName << "\n";
    ofs << "Generated on: " << xdp::WriterI::getCurrentDateTime() << "\n";
    ofs << "Msec since Epoch: " << xdp::WriterI::getCurrentTimeMsec() << "\n";
    if (!xdp::WriterI::getCurrentExecutableName().empty()) {
      ofs << "Profiled application: " << xdp::WriterI::getCurrentExecutableName() << "\n";
    }
    ofs << "Target platform: " << PlatformName << "\n";
    ofs << "Tool version: " << xdp::WriterI::getToolVersion() << "\n";
  }

  // Write sub-header to profile summary
  // NOTE: this part of the header must be written after a run is completed.
  void UnifiedCSVProfileWriter::writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile)
  {
    if (!ofs.is_open())
      return;

    // Sub-header of profile summary
    ofs << "Target devices: " << profile->getDeviceNames(", ") << "\n";

    std::string flowMode;
    xdp::RTUtil::getFlowModeName(mPluginHandle->getFlowMode(), flowMode);
    ofs << "Flow mode: " << flowMode << "\n";
  }

  void UnifiedCSVProfileWriter::writeTableHeader(std::ofstream& ofs, const std::string& caption,
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

  void UnifiedCSVProfileWriter::writeDocumentFooter(std::ofstream& ofs)
  {
    if (ofs.is_open()) {
      // Close the document
      ofs << "\n";
    }
  }
  
  // Write top kernel summary
  void UnifiedCSVProfileWriter::writeKernel(const KernelTrace& trace)
  {
    writeTableRowStart(getStream());
    writeTableCells(getStream(), trace.getDeviceName(), trace.getKernelName(),
        trace.getStart(), trace.getDuration());
    writeTableRowEnd(getStream());
  }

  // Write top buffer summary (host to global memory)
  void UnifiedCSVProfileWriter::writeBuffer(const BufferTrace& trace)
  {
    std::string durationStr = std::to_string( trace.getDuration() );
    double rate = (double)(trace.getSize()) / (1000.0 * trace.getDuration());
    std::string rateStr = std::to_string(rate);
    if  (  mPluginHandle->getFlowMode() == xdp::RTUtil::CPU
        || mPluginHandle->getFlowMode() == xdp::RTUtil::COSIM_EM
        || mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM) {
      durationStr = "N/A";
      rateStr = "N/A";
    }

    writeTableRowStart(getStream());

    writeTableCells(getStream(), trace.getAddress(), trace.getStart(),
        durationStr, (double)(trace.getSize())/1000.0, rateStr);

    writeTableRowEnd(getStream());
  }
  
  // Table 6: Data Transfer: Top Kernel & Global
  // Location, Accelerator, Number of Transfers, Average Bytes per Transfer,
  // Total Data Transfer (MB), Total Write (MB), Total Read (MB), Total Transfer Rate (MB/s)
  void UnifiedCSVProfileWriter::writeTopKernelTransferSummary(
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

    writeTableRowStart(getStream());
    writeTableCells(getStream(),
        deviceName, accelName, totalReadTranx + totalWriteTranx,
        aveBytesPerTransfer, transferEfficiency,
        (double)(totalReadBytes + totalWriteBytes) / 1.0e6,
        (double)(totalWriteBytes) / 1.0e6, (double)(totalReadBytes) / 1.0e6,
        transferRateMBps);
    writeTableRowEnd(getStream());
  }

  // Table 7: Data Transfer: Host & DDR Memory
  // Transfer Type, Number Of Transfers, Transfer Rate (MB/s),
  // Average Bandwidth Utilization (%), Average Size (KB), Average Time (ms)
  void UnifiedCSVProfileWriter::writeHostTransferSummary(const std::string& name,
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
    if (mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM) {
      transferRateStr = "N/A";
      aveBWUtilStr = "N/A";
      totalTimeStr = "N/A";
      aveTimeStr = "N/A";
    }

    writeTableRowStart(getStream());
    writeTableCells(getStream(), name, totalTranx, transferRateStr,
        aveBWUtilStr, aveBytes/1000.0, aveTimeStr);

    writeTableRowEnd(getStream());
  }

} // xdp
