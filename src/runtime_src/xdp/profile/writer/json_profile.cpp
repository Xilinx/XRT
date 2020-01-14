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

#define XDP_SOURCE

#include "json_profile.h"
#include "util.h"

#include "xdp/profile/core/rt_profile.h"
#include "core/common/system.h"

namespace xdp {

JSONProfileWriter::JSONProfileWriter(XDPPluginI* Plugin,
    const std::string& platformName, const std::string& /*summaryFileName*/) :
  ProfileWriterI(Plugin, platformName, ""), // fileName intentionally blank.
  mTree(new boost::property_tree::ptree())
{
  writeDocumentHeader(Summary_ofs, "Profile Summary");
}

JSONProfileWriter::~JSONProfileWriter()
{
  writeDocumentFooter(Summary_ofs);
}

void JSONProfileWriter::writeSummary(RTProfile* profile)
{
  // The following code matches that in the base class ProfileWriterI::writeSummary.
  // We are not calling the base class method because it does lots of extra
  // stuff that we don't care about. Where it calls "Table 7," we've changed
  // that here to be Table A-7. Later there is code which matches that in
  // the subclasses, e.g. CSVProfileWriter. Where that calls "Table 7," we've
  // changed that here to be Table B-7. But note also that ProfileWriterI helper
  // methods that implement this are not consistently labeled, e.g.
  // Table 5: Data Transfer: Host & Global Memory
  // void ProfileWriterI::writeHostTransferSummary(const std::string& name,
  // Table 5: Data Transfer: Kernels & Global Memory
  // void ProfileWriterI::writeKernelTransferSummary(const std::string& deviceName,

  auto flowMode = mPluginHandle->getFlowMode();

  // Sub-header
  writeDocumentSubHeader(getStream(), profile);

  // Table A-1: API Call summary
  makeCurrentBranch("apiCalls");
  profile->writeAPISummary(this);

  // Table A-2: Kernel Execution Summary
  makeCurrentBranch("kernelEnqueues");
  profile->writeKernelSummary(this);

  // Table A-3: Compute Unit Utilization
  makeCurrentBranch("computeUnitUtilization");
  profile->writeComputeUnitSummary(this);

  // Table A-4: Compute Units: Stall Information
  if (mEnStallTable) {
    makeCurrentBranch("cuStallInfo");
    profile->writeStallSummary(this);
  }

  // Table A-5: Data Transfer: Host to Global Memory
  if ((flowMode != xdp::RTUtil::CPU) && (flowMode != xdp::RTUtil::COSIM_EM)) {
    makeCurrentBranch("hostToGlobalTransfers");
    profile->writeTransferSummary(this, xdp::RTUtil::MON_HOST_DYNAMIC);
    // which will eventually call writeShellTransferSummary(deviceName, ...)
  }

  // Table A-6: Data Transfer: Kernels to Global Memory
  if (profile->isDeviceProfileOn()) {
    makeCurrentBranch("kernelsToGlobalTransfers");
    profile->writeKernelTransferSummary(this);
  }

  // Table A-7 : Stream Data Transfers
  if (mEnStreamTable) {
    makeCurrentBranch("streams");
    profile->writeKernelStreamSummary(this);
  }

  if (mEnShellTables) {

    // Table A-8 : Data Transfer: DMA
    makeCurrentBranch("transfers.dma");
    profile->writeTransferSummary(this, xdp::RTUtil::MON_SHELL_XDMA);

    // Table A-9 : Data Transfer: DMA Bypass
    makeCurrentBranch("transfers.bypass");
    profile->writeTransferSummary(this, xdp::RTUtil::MON_SHELL_P2P);

    // Table A-10 : Data Transfer: Global Memory to Global Memory
    makeCurrentBranch("tranfers.global");
    profile->writeTransferSummary(this, xdp::RTUtil::MON_SHELL_KDMA);
  }

  // Table A-11 : Top Data Transfer: Kernel & Global
  if (profile->isDeviceProfileOn()) {
    makeCurrentBranch("transfers.kernelsToGlobal");
    profile->writeTopKernelTransferSummary(this);
  }

  // ---------------------------------------------------------------------
  // The following are implemented in the subclasses, e.g. CSVProfileWriter.

  // Table B-7: Top Kernel Execution
  makeCurrentBranch("kernels");
  profile->writeTopKernelSummary(this);

  // Table B-8: Top Memory Writes: Host to Global Memory
  // This will end up calling this object's writeBuffer method.
  // So we need to create and set current a branch of the property tree.
  makeCurrentBranch("hostGlobal.writes");
  profile->writeTopDataTransferSummary(this, false); // Writes

  // Table B-9: Top Memory Reads: Host to Global Memory
  // Like the above, but this is reads.
  makeCurrentBranch("hostGlobal.reads");
  profile->writeTopDataTransferSummary(this, true); // Reads

  // Table B-10: Parameters used in PRCs
  makeCurrentBranch("guidanceMetadata");
  writeGuidanceMetadataSummary(profile);
}

void JSONProfileWriter::writeDocumentHeader(std::ofstream& /*ofs*/,
    const std::string& docName)
{
  boost::property_tree::ptree header;
  header.put("name", docName);
  header.put("generated", xdp::WriterI::getCurrentDateTime());
  header.put("epochTime", xdp::WriterI::getCurrentTimeMsec());
  // #TODO: application name may be empty. Should we omit, or put in empty string?
  header.put("application", xdp::WriterI::getCurrentExecutableName());
  header.put("platform", mPlatformName);
  header.put("toolVersion", xdp::WriterI::getToolVersion());

  boost::property_tree::ptree xrtInfo;
  xrt_core::get_xrt_info(xrtInfo);
  header.put("XRT build version", xrtInfo.get<std::string>("version", "N/A"));
  header.put("Build version branch", xrtInfo.get<std::string>("branch", "N/A"));
  header.put("Build version hash", xrtInfo.get<std::string>("hash", "N/A"));
  header.put("Build version date", xrtInfo.get<std::string>("date", "N/A"));

  mTree->add_child("header", header);
}

// Write sub-header to profile summary
// NOTE: this part of the header must be written after a run is completed.
void JSONProfileWriter::writeDocumentSubHeader(std::ofstream& /*ofs*/, RTProfile* profile)
{
  // Let's just add to the existing header section.
  boost::property_tree::ptree& header = mTree->get_child("header");
  std::string flowMode;
  xdp::RTUtil::getFlowModeName(mPluginHandle->getFlowMode(), flowMode);
  header.put("flow", flowMode);
  std::vector<std::string> deviceNames = profile->getDeviceNames();
  boost::property_tree::ptree devices;
  for (std::string& name : deviceNames) {
    boost::property_tree::ptree child;
    child.put("", name);
    devices.push_back(std::make_pair("", child));
  }
  header.add_child("devices", devices);
  header.put("totalTime", profile->getTotalHostTimeInMilliSec());
}


// Tables 1 and 2: API Call and Kernel Execution Summary: Name, Number Of Calls,
// Total Time (ms), Minimum Time (ms), Average Time (ms), Maximum Time (ms)
void JSONProfileWriter::writeTimeStats(const std::string& name, const TimeStats& stats)
{
  boost::property_tree::ptree stat;
  stat.put("numCalls", stats.getNoOfCalls());
  stat.put("totalTime", stats.getTotalTime());
  stat.put("minTime", stats.getMinTime());
  stat.put("avgTime", stats.getAveTime());
  stat.put("maxTime", stats.getMaxTime());

  getCurrentBranch().add_child(name, stat);
}


//void JSONProfileWriter::writeBufferStats(const std::string& name,
//                                         const BufferStats& stats)
//{
//  boost::property_tree::ptree stat;
//  //stat.put("name", name);
//  stat.put("count", stats.getCount());
//  stat.put("totalTime", stats.getTotalTime());
//  stat.put("avgTime", stats.getAveTime());
//  stat.put("avgRate", stats.getAveTransferRate());
//  stat.put("min", (double)(stats.getMin()) / 1000.0);
//  stat.put("avg", (double)(stats.getAverage()) / 1000.0);
//  stat.put("max", (double)(stats.getMax()) / 1000.0);
//
//  boost::property_tree::ptree& branch = mTree->get_child(mCurrentBranch);
//  branch.add_child(name, stat);
//}


void JSONProfileWriter::writeKernel(const KernelTrace& trace)
{
  boost::property_tree::ptree kernel;
  kernel.put("name", trace.getKernelName());
  kernel.put("instanceAddress", trace.getAddress());
  kernel.put("contextId", trace.getContextId());
  kernel.put("commandQueueID", trace.getCommandQueueId());
  kernel.put("device", trace.getDeviceName());
  kernel.put("startTime", trace.getStart());
  kernel.put("duration", trace.getDuration());
  std::string globalWorkSize = std::to_string(trace.getGlobalWorkSizeByIndex(0))
    + ":" + std::to_string(trace.getGlobalWorkSizeByIndex(1))
    + ":" + std::to_string(trace.getGlobalWorkSizeByIndex(2));
  kernel.put("globalWorkSize", globalWorkSize);
  std::string localWorkSize = std::to_string(trace.getLocalWorkSizeByIndex(0))
    + ":" + std::to_string(trace.getLocalWorkSizeByIndex(1))
    + ":" + std::to_string(trace.getLocalWorkSizeByIndex(2));
  kernel.put("localWorkSize", localWorkSize);

  getCurrentBranch().push_back(std::make_pair("", kernel));
}


// Write buffer trace summary (host to global memory)
void JSONProfileWriter::writeBuffer(const BufferTrace& trace)
{
  std::string durationStr = std::to_string(trace.getDuration());
  double rate = (double)(trace.getSize()) / (1000.0 * trace.getDuration());
  std::string rateStr = std::to_string(rate);
  if (mPluginHandle->getFlowMode() == xdp::RTUtil::CPU
    || mPluginHandle->getFlowMode() == xdp::RTUtil::COSIM_EM
    || mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM) {
    durationStr = "N/A";
    rateStr = "N/A";
  }

  //"Buffer Address", "Context ID", "Command Queue ID", "Start Time (ms)",
  //  "Duration (ms)", "Buffer Size (KB)", "Writing Rate(MB/s)"

  boost::property_tree::ptree twig;
  twig.put("address", trace.getAddress());
  twig.put("contextID", trace.getContextId());
  twig.put("cmdQueueID", trace.getCommandQueueId());
  twig.put("startTime", trace.getStart());
  twig.put("duration", durationStr);
  twig.put("size", (double)(trace.getSize()) / 1000.0);
  twig.put("rate", rateStr);

  getCurrentBranch().push_back(std::make_pair("", twig));
}


//// Write device trace summary
// Not fully implemented, and commented out. See header file for explanation.
//void JSONProfileWriter::writeDeviceTransfer(const DeviceTrace& trace)
//{
//  boost::property_tree::ptree twig;
//  twig.put("name", trace.Name);
//  twig.put("contextId", trace.ContextId);
//  twig.put("startTime", trace.Start);
//  twig.put("burstLength", trace.BurstLength);
//  twig.put("duration", (trace.EndTime - trace.StartTime));
//  twig.put("rate", 1000.0*(trace.End - trace.Start));
//
//  boost::property_tree::ptree& branch = mTree->get_child(mCurrentBranch);
//}


void JSONProfileWriter::writeComputeUnitSummary(const std::string& name, const TimeStats& stats)
{
  if (stats.getTotalTime() == 0.0)
    return;

  // "name" is of the form "deviceName|kernelName|globalSize|localSize|cuName"
  size_t first_index = name.find_first_of("|");
  size_t second_index = name.find('|', first_index + 1);
  size_t third_index = name.find('|', second_index + 1);
  size_t fourth_index = name.find_last_of("|");

  auto cuName = name.substr(fourth_index + 1);
  auto deviceName = name.substr(0, first_index);
  auto maxParallelIter = stats.getMetadata();
  std::string isDataflow = stats.getFlags() ? "Yes" : "No";
  double speedup = (stats.getAveTime() * stats.getNoOfCalls()) / stats.getTotalTime();
  std::string speedup_string = std::to_string(speedup) + "x";

  boost::property_tree::ptree cu;
  cu.put("name", cuName);
  cu.put("device", deviceName);
  // kernelName
  cu.put("kernel", name.substr(first_index + 1, second_index - first_index - 1));
  // globalSize
  cu.put("globalWorkSize", name.substr(second_index + 1, third_index - second_index - 1));
  // localSize
  cu.put("localWorkSize", name.substr(third_index + 1, fourth_index - third_index - 1));
  cu.put("numberOfCalls", stats.getNoOfCalls());
  cu.put("dataflow", isDataflow);
  cu.put("maxOverlappingExecutions", maxParallelIter);
  cu.put("dataflowAcceleration", speedup_string);
  cu.put("totalTime", stats.getTotalTime());
  cu.put("minTime", stats.getMinTime());
  cu.put("avgTime", stats.getAveTime());
  cu.put("maxTime", stats.getMaxTime());
  cu.put("frequency", stats.getClockFreqMhz());

  getCurrentBranch().push_back(std::make_pair("", cu));
}


void JSONProfileWriter::writeHostTransferSummary(const std::string& name,
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

  boost::property_tree::ptree trans;
  trans.put("contextDevices", contextDevices);
  trans.put("name", name);
  trans.put("numTransfers", totalTranx);
  trans.put("rate", transferRateStr);
  trans.put("util", aveBWUtilStr);
  trans.put("avgSize", aveBytes / 1000.0);
  trans.put("totalTime", totalTimeStr);
  trans.put("avgLatency", aveTimeStr);

  getCurrentBranch().push_back(std::make_pair("", trans));
}


void JSONProfileWriter::writeShellTransferSummary(const std::string& deviceName, const std::string& transferType,
  uint64_t totalBytes, uint64_t totalTranx, double totalLatencyNsec, double totalTimeMsec)
{
  double totalMB = totalBytes / 1.0e6;
  double transferRateMBps = (totalTimeMsec == 0) ? 0.0 :
    totalBytes / (1000.0 * totalTimeMsec);
  double aveBytes = (totalTranx == 0) ? 0.0 : (double)(totalBytes) / totalTranx;
  double aveLatencyNsec = (totalTranx == 0) ? 0.0 : totalLatencyNsec / totalTranx;

  // Don't show these values for HW emulation or when invalid
  std::string transferRateStr = std::to_string(transferRateMBps);
  std::string totalTimeStr = std::to_string(totalTimeMsec);
  std::string aveLatencyStr = std::to_string(aveLatencyNsec);
  if ((mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM)
    || (totalTimeMsec == 0) || (totalTranx == 0)) {
    transferRateStr = "N/A";
    totalTimeStr = "N/A";
    aveLatencyStr = "N/A";
  }

  boost::property_tree::ptree trans;
  trans.put("deviceName", deviceName);
  trans.put("type", transferType);
  trans.put("numTransfers", totalTranx);
  trans.put("rate", transferRateStr);
  trans.put("totalSize", totalMB);
  trans.put("totalTime", totalTimeStr);
  trans.put("avgSize", aveBytes / 1000.0);
  trans.put("avgLatency", aveLatencyStr);

  getCurrentBranch().push_back(std::make_pair("", trans));
}


void JSONProfileWriter::writeKernelTransferSummary(const std::string& deviceName,
  const std::string& cuPortName, const std::string& argNames, const std::string& memoryName,
  const std::string& transferType, uint64_t totalBytes, uint64_t totalTranx,
  double totalKernelTimeMsec, double totalTransferTimeMsec, double maxTransferRateMBps)
{
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
  size_t index = cuPortName.find_last_of(IP_LAYOUT_SEP);
  if (index != std::string::npos) {
    cuPortName2 = cuPortName.substr(0, index);
    memoryName2 = cuPortName.substr(index + 1);
  }

  boost::property_tree::ptree trans;
  trans.put("deviceName", deviceName);
  trans.put("cuPortName", cuPortName2);
  trans.put("args", argNames);
  trans.put("memory", memoryName2);
  trans.put("type", transferType);
  trans.put("numTransfers", totalTranx);
  trans.put("rate", transferRateMBps);
  trans.put("util", aveBWUtil);
  trans.put("avgSize", aveBytes / 1000.0);
  trans.put("avgLatency", 1.0e6*aveTimeMsec);

  getCurrentBranch().push_back(std::make_pair("", trans));
}

void JSONProfileWriter::writeStallSummary(std::string& cuName, uint32_t cuRunCount,
  double cuRunTimeMsec, double cuStallExt, double cuStallStr, double cuStallInt)
{
  // Note that the following are not in the order of the arguments, but based
  // on the column headers and code like the following:
  //  std::vector<std::string> KernelStallLabels = {
  //    "Compute Unit", "Execution Count", "Running Time (ms)", "Intra-Kernel Dataflow Stalls (ms)",
  //    "External Memory Stalls (ms)", "Inter-Kernel Pipe Stalls (ms)"
  //  };
  //writeTableCells(getStream(), cuName, cuRunCount, cuRunTimeMsec,
  //  cuStallInt, cuStallExt, cuStallStr);
  boost::property_tree::ptree stall;
  stall.put("cuName", cuName);
  stall.put("runCount", cuRunCount);
  stall.put("runTime", cuRunTimeMsec);
  stall.put("interKernelDataflowStall",  cuStallInt);
  stall.put("externalMemStall", cuStallExt);
  stall.put("interKernelPipeStall", cuStallStr);

  getCurrentBranch().push_back(std::make_pair("", stall));
}


void JSONProfileWriter::writeKernelStreamSummary(
  const std::string& deviceName, const std::string& MasterPort, const std::string& MasterArgs,
  const std::string& SlavePort, const std::string& SlaveArgs, uint64_t strNumTranx,
  double transferRateMBps, double avgSize, double avgUtil,
  double linkStarve, double linkStall)
{
  boost::property_tree::ptree stream;
  stream.put("device", deviceName);
  stream.put("masterPort", MasterPort);
  stream.put("masterArgs", MasterArgs);
  stream.put("slavePort", SlavePort);
  stream.put("slaveArgs", SlaveArgs);
  stream.put("numTransfers", strNumTranx);
  stream.put("rate", transferRateMBps);
  stream.put("avgSize", avgSize);
  stream.put("util", avgUtil);
  stream.put("starve", linkStarve);
  stream.put("stall", linkStall);

  getCurrentBranch().push_back(std::make_pair("", stream));
}


void JSONProfileWriter::writeTopKernelTransferSummary(
  const std::string& deviceName, const std::string& cuName,
  uint64_t totalWriteBytes, uint64_t totalReadBytes,
  uint64_t totalWriteTranx, uint64_t totalReadTranx,
  double totalWriteTimeMsec, double totalReadTimeMsec,
  uint32_t maxBytesPerTransfer, double /*maxTransferRateMBps*/)
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

  boost::property_tree::ptree trans;
  trans.put("deviceName", deviceName);
  trans.put("cuName", cuName);
  trans.put("numTransfers", totalReadTranx + totalWriteTranx);
  trans.put("avgSize", aveBytesPerTransfer);
  trans.put("efficiency", transferEfficiency);
  trans.put("totalSize", (double)(totalReadBytes + totalWriteBytes) / 1.0e6);
  trans.put("totalWrite", (double)(totalWriteBytes) / 1.0e6);
  trans.put("totalRead", (double)(totalReadBytes) / 1.0e6);
  trans.put("rate",  transferRateMBps);

  getCurrentBranch().push_back(std::make_pair("", trans));
}


void JSONProfileWriter::writeGuidanceMetadataSummary(RTProfile *profile)
{
  //std::vector<std::string> GuidanceColumnLabels = {
  //  "Parameter", "Element", "Value"
  //};
  boost::property_tree::ptree& metadata = getCurrentBranch();

  auto deviceExecTimesMap = mPluginHandle->getDeviceExecTimesMap();
  auto computeUnitCallsMap = mPluginHandle->getComputeUnitCallsMap();
  auto kernelCountsMap = mPluginHandle->getKernelCountsMap();

  std::string checkName;

  // 1. Device execution times
  XDPPluginI::getGuidanceName(XDPPluginI::DEVICE_EXEC_TIME, checkName);
  boost::property_tree::ptree& check = metadata.add_child(checkName, boost::property_tree::ptree());

  for (auto& itr : deviceExecTimesMap) {
    // Sometimes the key value (iter->first) can contain a '.' in it, which boost
    // normally interprets as a hierarchy separator. We don't want hierarchy--the
    // '.' should appear in the key, so we use path_type with '\0' as the
    // hierarchy separator (since '\0' won't actually appear in the string).
    check.put(boost::property_tree::ptree::path_type(itr.first, '\0'), itr.second);
  }

  // 2. Compute Unit calls
  XDPPluginI::getGuidanceName(XDPPluginI::CU_CALLS, checkName);
  boost::property_tree::ptree& check2 = metadata.add_child(checkName, boost::property_tree::ptree());

  for (auto& itr : computeUnitCallsMap) {
    // See above use of path_type for explanation.
    check2.put(boost::property_tree::ptree::path_type(itr.first, '\0'), itr.second);
  }

  // 3. Global memory bit widths
  XDPPluginI::getGuidanceName(XDPPluginI::MEMORY_BIT_WIDTH, checkName);
  boost::property_tree::ptree& check3 = metadata.add_child(checkName, boost::property_tree::ptree());
  uint32_t bitWidth = profile->getGlobalMemoryBitWidth();

  for (auto& itr : deviceExecTimesMap) {
    // See above use of path_type for explanation.
    check3.put(boost::property_tree::ptree::path_type(itr.first, '\0'), bitWidth);
  }

  // 4. Usage of MigrateMemObjects
  XDPPluginI::getGuidanceName(XDPPluginI::MIGRATE_MEM, checkName);
  boost::property_tree::ptree& check4 = metadata.add_child(checkName, boost::property_tree::ptree());
  int migrateMemCalls = profile->getMigrateMemCalls();
  check4.put("host", migrateMemCalls);

  // 5. Usage of memory resources
  XDPPluginI::getGuidanceName(XDPPluginI::MEMORY_USAGE, checkName);
  boost::property_tree::ptree& check5 = metadata.add_child(checkName, boost::property_tree::ptree());
  auto cuPortVector = mPluginHandle->getCUPortVector();
  std::map<std::string, int> cuPortsToMemory;
  for (auto& cuPort : cuPortVector) {
    auto memoryName = std::get<3>(cuPort);
    auto iter = cuPortsToMemory.find(memoryName);
    int numPorts = (iter == cuPortsToMemory.end()) ? 1 : (iter->second + 1);
    cuPortsToMemory[memoryName] = numPorts;
  }

  for (auto& itr : cuPortsToMemory) {
    // See above use of path_type for explanation.
    check5.put(boost::property_tree::ptree::path_type(itr.first, '\0'), itr.second);
  }
  cuPortsToMemory.clear();

  // 5a. PLRAM device
  XDPPluginI::getGuidanceName(XDPPluginI::PLRAM_DEVICE, checkName);
  boost::property_tree::ptree& check5a = metadata.add_child(checkName, boost::property_tree::ptree());
  int isPlram = (mPluginHandle->isPlramDevice()) ? 1 : 0;
  check5a.put("all", isPlram);

  // 5b. HBM device
  XDPPluginI::getGuidanceName(XDPPluginI::HBM_DEVICE, checkName);
  boost::property_tree::ptree& check5b = metadata.add_child(checkName, boost::property_tree::ptree());
  int isHbm = (mPluginHandle->isHbmDevice()) ? 1 : 0;
  check5b.put("all", isHbm);

  // 5c. KDMA device
  XDPPluginI::getGuidanceName(XDPPluginI::KDMA_DEVICE, checkName);
  boost::property_tree::ptree& check5c = metadata.add_child(checkName, boost::property_tree::ptree());
  int isKdma = (mPluginHandle->isKdmaDevice()) ? 1 : 0;
  check5c.put("all", isKdma);

  // 5d. P2P device
  XDPPluginI::getGuidanceName(XDPPluginI::P2P_DEVICE, checkName);
  boost::property_tree::ptree& check5d = metadata.add_child(checkName, boost::property_tree::ptree());
  int isP2P = (mPluginHandle->isP2PDevice()) ? 1 : 0;
  check5d.put("all", isP2P);

  // 5e. Host transfers from P2P buffers
  XDPPluginI::getGuidanceName(XDPPluginI::P2P_HOST_TRANSFERS, checkName);
  boost::property_tree::ptree& check5e = metadata.add_child(checkName, boost::property_tree::ptree());
  int hostP2PTransfers = profile->getHostP2PTransfers();
  check5e.put("host", hostP2PTransfers);

  // 6. Port data widths
  XDPPluginI::getGuidanceName(XDPPluginI::PORT_BIT_WIDTH, checkName);
  boost::property_tree::ptree& check6 = metadata.add_child(checkName, boost::property_tree::ptree());
  for (auto& cuPort : cuPortVector) {
    auto cu = std::get<0>(cuPort);
    auto port = std::get<1>(cuPort);
    std::string portName = cu + "/" + port;
    auto portWidth = std::get<4>(cuPort);
    // See above use of path_type for explanation.
    check6.put(boost::property_tree::ptree::path_type(portName, '\0'), portWidth);
  }

  // 7. Kernel CU counts
  XDPPluginI::getGuidanceName(XDPPluginI::KERNEL_COUNT, checkName);
  boost::property_tree::ptree& check7 = metadata.add_child(checkName, boost::property_tree::ptree());
  for (auto kernelCount : kernelCountsMap) {
    check7.put(kernelCount.first, kernelCount.second);
  }

  // 8. OpenCL objects released
  XDPPluginI::getGuidanceName(XDPPluginI::OBJECTS_RELEASED, checkName);
  boost::property_tree::ptree& check8 = metadata.add_child(checkName, boost::property_tree::ptree());
  int numReleased = (mPluginHandle->isObjectsReleased()) ? 1 : 0;
  check8.put("all", numReleased);
}


void JSONProfileWriter::writeTableHeader(std::ofstream& /*ofs*/, const std::string& /*caption*/,
                                        const std::vector<std::string>& /*columnLabels*/)
{
  // Nothing to do, but this is a pure virtual in the base class, so we have
  // to provide an override.
}

void JSONProfileWriter::makeCurrentBranch(const std::string& name)
{
  // #TODO: Put in code to check if the branch already exists.
  mCurrentBranch = name;
  mTree->add_child(mCurrentBranch, boost::property_tree::ptree());
}

boost::property_tree::ptree& JSONProfileWriter::getCurrentBranch()
{
  // #TODO: Put in code to check if the branch exists.
  return mTree->get_child(mCurrentBranch);
}

} // namespace xdp
