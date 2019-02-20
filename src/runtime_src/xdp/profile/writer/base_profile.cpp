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

#include "base_profile.h"

namespace xdp {
  //********************
  // Base Profile Writer
  //********************
  ProfileWriterI::ProfileWriterI()
  {
  }
  
  void ProfileWriterI::openStream(std::ofstream& ofs, const std::string& fileName)
  {
    ofs.open(fileName);
    if (!ofs.is_open()) {
      throw std::runtime_error("Unable to open profile report for writing");
    }
  }

  void ProfileWriterI::writeSummary(RTProfile* profile)
  {
    auto flowMode = mPluginHandle->getFlowMode();

    // Sub-header
    writeDocumentSubHeader(getStream(), profile);

    //Table 1: API Call summary
    std::vector<std::string> APICallSummaryColumnLabels = { "API Name",
        "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)" };

    writeTableHeader(getStream(), "OpenCL API Calls", APICallSummaryColumnLabels);
    profile->writeAPISummary(this);
    writeTableFooter(getStream());

    // Table 2: Kernel Execution Summary
    std::vector<std::string> KernelExecutionSummaryColumnLabels = {
        "Kernel", "Number Of Enqueues", "Total Time (ms)",
        "Minimum Time (ms)", "Average Time (ms)", "Maximum Time (ms)" };

    std::string table2Caption = (flowMode == xdp::RTUtil::HW_EM) ?
        "Kernel Execution (includes estimated device times)" : "Kernel Execution";
    writeTableHeader(getStream(), table2Caption, KernelExecutionSummaryColumnLabels);
    profile->writeKernelSummary(this);
    writeTableFooter(getStream());

    // Table 3: Compute Unit Utilization
    std::vector<std::string> ComputeUnitExecutionSummaryColumnLabels = {
        "Device", "Compute Unit", "Kernel", "Global Work Size", "Local Work Size",
        "Number Of Calls", "Total Time (ms)", "Minimum Time (ms)",
        "Average Time (ms)", "Maximum Time (ms)", "Clock Frequency (MHz)" };

    std::string table3Caption = (flowMode == xdp::RTUtil::HW_EM) ?
        "Compute Unit Utilization (includes estimated device times)" : "Compute Unit Utilization";
    writeTableHeader(getStream(), table3Caption, ComputeUnitExecutionSummaryColumnLabels);
    profile->writeComputeUnitSummary(this);
    writeTableFooter(getStream());

    if (mEnStallTable) {
      std::vector<std::string> KernelStallLabels = {
        "Compute Unit", "Execution Count", "Running Time (ms)", "Intra-Kernel Dataflow Stalls (ms)", 
        "External Memory Stalls (ms)", "Inter-Kernel Pipe Stalls (ms)"
      };

      writeTableHeader(getStream(), "Compute Units: Stall Information", KernelStallLabels);
      profile->writeStallSummary(this);
      writeTableFooter(getStream());
    }

    // Table 5: Data Transfer: Host & Global
    std::vector<std::string> DataTransferSummaryColumnLabels = {
        "Context:Number of Devices", "Transfer Type", "Number Of Transfers",
        "Transfer Rate (MB/s)", "Average Bandwidth Utilization (%)",
        "Average Size (KB)", "Total Time (ms)", "Average Time (ms)"
    };
    writeTableHeader(getStream(), "Data Transfer: Host and Global Memory",
        DataTransferSummaryColumnLabels);
    if ((flowMode != xdp::RTUtil::CPU) && (flowMode != xdp::RTUtil::COSIM_EM)) {
      profile->writeHostTransferSummary(this);
    }
    writeTableFooter(getStream());

    // Table 6: Data Transfer: Kernel & Global
    std::vector<std::string> KernelDataTransferSummaryColumnLabels = {
        "Device", "Compute Unit/Port Name", "Kernel Arguments", "Memory Resources",
		"Transfer Type", "Number Of Transfers", "Transfer Rate (MB/s)",
		"Average Bandwidth Utilization (%)", "Average Size (KB)", "Average Latency (ns)"
    };
    writeTableHeader(getStream(), "Data Transfer: Kernels and Global Memory",
        KernelDataTransferSummaryColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeKernelTransferSummary(this);
    }
    writeTableFooter(getStream());

    // Table 6.1 : Stream Data Transfers
    if (mEnStallTable) {
    std::vector<std::string> StreamTransferSummaryColumnLabels = {
        "Device", "Compute Unit/Port Name", "Kernel Arguments", "Number Of Transfers", "Transfer Rate (MB/s)",
        "Average Size (KB)", "Link Utilization (%)", "Link Starve (%)", "Link Stall (%)"
        };
      writeTableHeader(getStream(), "Data Transfer: Streams between Host and Kernels", StreamTransferSummaryColumnLabels);
      profile->writeKernelStreamSummary(this);
      writeTableFooter(getStream());
    }

    // Table 7: Top Data Transfer: Kernel & Global
    std::vector<std::string> TopKernelDataTransferSummaryColumnLabels = {
        "Device", "Compute Unit", "Number of Transfers", "Average Bytes per Transfer",
        "Transfer Efficiency (%)", "Total Data Transfer (MB)", "Total Write (MB)",
        "Total Read (MB)", "Total Transfer Rate (MB/s)"
    };
    writeTableHeader(getStream(), "Top Data Transfer: Kernels and Global Memory",
        TopKernelDataTransferSummaryColumnLabels);
    if (profile->isDeviceProfileOn()) {
      profile->writeTopKernelTransferSummary(this);
    }
    writeTableFooter(getStream());
  }

  // Tables 1 and 2: API Call and Kernel Execution Summary: Name, Number Of Calls,
  // Total Time (ms), Minimum Time (ms), Average Time (ms), Maximum Time (ms)
  void ProfileWriterI::writeTimeStats(const std::string& name, const TimeStats& stats)
  {
    writeTableRowStart(getStream());
    writeTableCells(getStream(), name, stats.getNoOfCalls(),
        stats.getTotalTime(), stats.getMinTime(),
        stats.getAveTime(), stats.getMaxTime());
    writeTableRowEnd(getStream());
  }

  void ProfileWriterI::writeStallSummary(std::string& cuName, uint32_t cuRunCount,
      double cuRunTimeMsec, double cuStallExt, double cuStallStr, double cuStallInt)
  {
    writeTableRowStart(getStream());
    writeTableCells(getStream(),cuName,cuRunCount, cuRunTimeMsec,
     cuStallInt, cuStallExt, cuStallStr);
    writeTableRowEnd(getStream());
  }

  void ProfileWriterI::writeKernelStreamSummary(std::string& deviceName, std::string& cuPortName, std::string& argNames,
      uint64_t strNumTranx, double transferRateMBps, double avgSize, double avgUtil,
      double linkStarve, double linkStall)
  {
    writeTableRowStart(getStream());
    writeTableCells(getStream(), deviceName , cuPortName, argNames,
        strNumTranx, transferRateMBps, avgSize, avgUtil, linkStarve, linkStall);
    writeTableRowEnd(getStream());
  }

  // Table 4: Data Transfer: Host & Global Memory
  // Context ID, Transfer Type, Number Of Transfers, Transfer Rate (MB/s),
  // Average Size (KB), Total Time (ms), Average Time (ms)
  void ProfileWriterI::writeHostTransferSummary(const std::string& name,
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

    std::string contextDevices = "context" + std::to_string(stats.getContextId())
        + ":" + std::to_string(stats.getNumDevices());

    writeTableRowStart(getStream());
    writeTableCells(getStream(), contextDevices, name, totalTranx,
        transferRateStr, aveBWUtilStr, aveBytes/1000.0, totalTimeStr, aveTimeStr);

    writeTableRowEnd(getStream());
  }

  // Table 5: Data Transfer: Kernels & Global Memory
  // Device, CU Port, Kernel Arguments, DDR Bank, Transfer Type, Number Of Transfers,
  // Transfer Rate (MB/s), Average Size (KB), Maximum Size (KB), Average Latency (ns)
  void ProfileWriterI::writeKernelTransferSummary(const std::string& deviceName,
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
    size_t index = cuPortName.find_last_of(PORT_MEM_SEP);
    if (index != std::string::npos) {
      cuPortName2 = cuPortName.substr(0, index);
      memoryName2 = cuPortName.substr(index+1);
    }

    writeTableRowStart(getStream());
    writeTableCells(getStream(), deviceName, cuPortName2, argNames, memoryName2,
    	transferType, totalTranx, transferRateMBps, aveBWUtil,
        aveBytes/1000.0, 1.0e6*aveTimeMsec);

    writeTableRowEnd(getStream());
  }

  // Table 6: Data Transfer: Top Kernel & Global
  // Device, Compute Unit, Number of Transfers, Average Bytes per Transfer,
  // Total Data Transfer (MB), Total Write (MB), Total Read (MB), Total Transfer Rate (MB/s)
  void ProfileWriterI::writeTopKernelTransferSummary(
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

    writeTableRowStart(getStream());
    writeTableCells(getStream(),
        deviceName, cuName,
        totalReadTranx + totalWriteTranx,
        aveBytesPerTransfer, transferEfficiency,
        (double)(totalReadBytes + totalWriteBytes) / 1.0e6,
        (double)(totalWriteBytes) / 1.0e6, (double)(totalReadBytes) / 1.0e6,
        transferRateMBps);
    writeTableRowEnd(getStream());
  }

  void ProfileWriterI::writeKernel(const KernelTrace& trace)
  {
    writeTableRowStart(getStream());

    std::string globalWorkSize = std::to_string(trace.getGlobalWorkSizeByIndex(0))
        + ":" + std::to_string(trace.getGlobalWorkSizeByIndex(1))
        + ":" + std::to_string(trace.getGlobalWorkSizeByIndex(2) ) ;

    std::string localWorkSize = std::to_string(trace.getLocalWorkSizeByIndex(0))  + ":" + std::to_string(trace.getLocalWorkSizeByIndex(1))
    + ":" + std::to_string(trace.getLocalWorkSizeByIndex(2) ) ;

    writeTableCells(getStream(), trace.getAddress(), trace.getKernelName(),
        trace.getContextId(), trace.getCommandQueueId(),
        trace.getDeviceName(), trace.getStart(), trace.getDuration(),
        //globalWorkSize, trace.getWorkGroupSize());
        globalWorkSize, localWorkSize);
    writeTableRowEnd(getStream());
  }

  // Write buffer trace summary (host to global memory)
  void ProfileWriterI::writeBuffer(const BufferTrace& trace)
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

    writeTableCells(getStream(), trace.getAddress(), trace.getContextId(),
        trace.getCommandQueueId(), trace.getStart(), durationStr,
        (double)(trace.getSize())/1000.0, rateStr);

    writeTableRowEnd(getStream());
  }

  // Write device trace summary
  void ProfileWriterI::writeDeviceTransfer(const DeviceTrace& trace)
  {
    writeTableRowStart(getStream());
    writeTableCells(getStream(), trace.Name, trace.ContextId, trace.Start,
        trace.BurstLength, (trace.EndTime - trace.StartTime),
        1000.0*(trace.End - trace.Start));
    writeTableRowEnd(getStream());
  }

  void ProfileWriterI::writeComputeUnitSummary(const std::string& name, const TimeStats& stats)
  {
    if (stats.getTotalTime() == 0.0)
      return;
    //"name" is of the form "deviceName|kernelName|globalSize|localSize|cuName"
    size_t first_index = name.find_first_of("|");
    size_t second_index = name.find('|', first_index+1);
    size_t third_index = name.find('|', second_index+1);
    size_t fourth_index = name.find_last_of("|");

    std::string deviceName = name.substr(0, first_index);
    writeTableRowStart(getStream());
    writeTableCells(getStream(), deviceName,
        name.substr(fourth_index+1), // cuName
        name.substr(first_index+1, second_index - first_index -1), // kernelName
        name.substr(second_index+1, third_index - second_index -1), // globalSize
        name.substr(third_index+1, fourth_index - third_index -1), // localSize
        stats.getNoOfCalls(), stats.getTotalTime(), stats.getMinTime(),
        stats.getAveTime(), stats.getMaxTime(), stats.getClockFreqMhz());
    writeTableRowEnd(getStream());
  }

  void ProfileWriterI::writeAcceleratorSummary(const std::string& name, const TimeStats& stats)
  {
    //"name" is of the form "deviceName|kernelName|globalSize|localSize|cuName"
    size_t first_index = name.find_first_of("|");
    //size_t second_index = name.find('|', first_index+1);
    //size_t third_index = name.find('|', second_index+1);
    size_t fourth_index = name.find_last_of("|");

    std::string deviceName = name.substr(0, first_index);
    auto clockFreqMHz = mPluginHandle->getKernelClockFreqMHz(deviceName);

    writeTableRowStart(getStream());
    writeTableCells(getStream(), deviceName,
        name.substr(fourth_index+1), // cuName
        stats.getNoOfCalls(), stats.getTotalTime(), stats.getMinTime(),
        stats.getAveTime(), stats.getMaxTime(), clockFreqMHz);
    writeTableRowEnd(getStream());
  }

  void ProfileWriterI::writeBufferStats(const std::string& name,
      const BufferStats& stats)
  {
    writeTableRowStart(getStream());
    writeTableCells(getStream(), name, stats.getCount(),
        stats.getTotalTime(), stats.getAveTime(),
        stats.getAveTransferRate(), (double)(stats.getMin())/1000.0,
        (double)(stats.getAverage())/1000.0, (double)(stats.getMax())/1000.0);
    writeTableRowEnd(getStream());
  }

  void ProfileWriterI::writeGuidanceMetadataSummary(RTProfile *profile,
      const XDPPluginI::GuidanceMap  &deviceExecTimesMap,
      const XDPPluginI::GuidanceMap  &computeUnitCallsMap,
      const XDPPluginI::GuidanceMap2 &kernelCountsMap)
  {
    // 1. Device execution times
    std::string checkName;
    XDPPluginI::getGuidanceName(XDPPluginI::DEVICE_EXEC_TIME, checkName);

    auto iter = deviceExecTimesMap.begin();
    for (; iter != deviceExecTimesMap.end(); ++iter) {
      std::string deviceName = iter->first;
      std::string value = iter->second;

      writeTableRowStart(getStream());
      writeTableCells(getStream(), checkName, deviceName, value);
      writeTableRowEnd(getStream());
    }

    // 2. Compute Unit calls
    std::string checkName2;
    XDPPluginI::getGuidanceName(XDPPluginI::CU_CALLS, checkName2);

    auto iter2 = computeUnitCallsMap.begin();
    for (; iter2 != computeUnitCallsMap.end(); ++iter2) {
      std::string cuName = iter2->first;
      std::string value = iter2->second;

      writeTableRowStart(getStream());
      writeTableCells(getStream(), checkName2, cuName, value);
      writeTableRowEnd(getStream());
    }

    // 3. Global memory bit widths
    std::string checkName3;
    XDPPluginI::getGuidanceName(XDPPluginI::MEMORY_BIT_WIDTH, checkName3);
    uint32_t bitWidth = profile->getGlobalMemoryBitWidth();

    auto iter3 = deviceExecTimesMap.begin();
    for (; iter3 != deviceExecTimesMap.end(); ++iter3) {
      std::string deviceName = iter3->first;

      writeTableRowStart(getStream());
      writeTableCells(getStream(), checkName3, deviceName, bitWidth);
      writeTableRowEnd(getStream());
    }

    // 4. Usage of MigrateMemObjects
    std::string checkName4;
    XDPPluginI::getGuidanceName(XDPPluginI::MIGRATE_MEM, checkName4);
    int migrateMemCalls = profile->getMigrateMemCalls();
    writeTableCells(getStream(), checkName4, "host", migrateMemCalls);
    writeTableRowEnd(getStream());

    // 5. Usage of memory resources
    std::string checkName5;
    XDPPluginI::getGuidanceName(XDPPluginI::DDR_BANKS, checkName5);

    auto cuPortVector = mPluginHandle->getCUPortVector();
    std::map<std::string, int> cuPortsToMemory;

    for (auto& cuPort : cuPortVector) {
      auto memoryName = std::get<3>(cuPort);
      auto iter = cuPortsToMemory.find(memoryName);
      int numPorts = (iter == cuPortsToMemory.end()) ? 1 : (iter->second + 1);
      cuPortsToMemory[memoryName] = numPorts;
    }

    auto memoryIter = cuPortsToMemory.begin();
    for (; memoryIter != cuPortsToMemory.end(); ++memoryIter) {
      writeTableCells(getStream(), checkName5, memoryIter->first, memoryIter->second);
      writeTableRowEnd(getStream());
    }
    cuPortsToMemory.clear();

    // 6. Port data widths
    std::string checkName6;
    XDPPluginI::getGuidanceName(XDPPluginI::PORT_BIT_WIDTH, checkName6);

    for (auto& cuPort : cuPortVector) {
      auto cu    = std::get<0>(cuPort);
      auto port  = std::get<1>(cuPort);
      std::string portName = cu + "/" + port;
      auto portWidth = std::get<4>(cuPort);
      writeTableCells(getStream(), checkName6, portName, portWidth);
      writeTableRowEnd(getStream());
    }

    // 7. Kernel CU counts
    std::string checkName7;
    XDPPluginI::getGuidanceName(XDPPluginI::KERNEL_COUNT, checkName7);

    for (auto kernelCount : kernelCountsMap) {
      writeTableCells(getStream(), checkName7, kernelCount.first, kernelCount.second);
      writeTableRowEnd(getStream());
    }

    // 8. OpenCL objects released
    std::string checkName8;
    XDPPluginI::getGuidanceName(XDPPluginI::OBJECTS_RELEASED, checkName8);
    int numReleased = (mPluginHandle->isObjectsReleased()) ? 1 : 0;
    writeTableCells(getStream(), checkName8, "all", numReleased);
  }

} // xdp
