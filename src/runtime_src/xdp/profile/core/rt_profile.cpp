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

#include "rt_profile.h"
#include "rt_util.h"
#include "trace_logger.h"
#include "summary_writer.h"
#include "xdp/profile/profile_config.h"
#include "xdp/profile/collection/results.h"
#include "xdp/profile/collection/counters.h"
#include "xdp/profile/core/trace_parser.h"
#include "xdp/profile/writer/base_profile.h"
#include "xdp/profile/writer/base_trace.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#pragma warning(disable : 4996 4244 4702)
/* 4996 : Disable warning for use of "getenv" */
/* 4244 : Disable warning for conversion of int to char in header file <algorithm> included in one of the header files */
/* 4702 : Disable warning for unreachable code. This is a temporary workaround for a crash on Windows */
#endif

namespace xdp {
  // ***********************
  // Top-Level Profile Class
  // ***********************
  RTProfile::RTProfile(int& flags, std::shared_ptr<XDPPluginI> Plugin)
  : mProfileFlags(flags),
    mFileFlags(0),
    mDeviceTraceOption(RTUtil::DEVICE_TRACE_OFF),
    mStallTraceOption(RTUtil::STALL_TRACE_OFF),
    mPluginHandle(Plugin)
  {
    // Profile counters (to store counter results)
    mProfileCounters = new ProfileCounters();

    // Trace parser
    mTraceParser = new TraceParser(mPluginHandle.get());

    // Logger & writer
    mLogger = new TraceLogger(mProfileCounters, mTraceParser, mPluginHandle.get());
    mWriter = new SummaryWriter(mProfileCounters, mTraceParser, mPluginHandle.get());

    // Run Summary
    mRunSummary = new RunSummary();
  }

  RTProfile::~RTProfile()
  {
    if (mProfileFlags)
      writeProfileSummary();

    // Write out the run summary file (if there is data to write)
    mRunSummary->writeContent();

    delete mWriter;
    delete mLogger;
    delete mTraceParser;
    delete mProfileCounters;
    delete mRunSummary;
  }

  // ***************************************************************************
  // Runtime Settings
  // ***************************************************************************

  void RTProfile::setTransferTrace(const std::string& traceStr)
  {
    std::string option = traceStr;
    std::transform(option.begin(), option.end(), option.begin(), ::tolower);

    if (option.find("off") != std::string::npos)         mDeviceTraceOption = RTUtil::DEVICE_TRACE_OFF;
    else if (option.find("fine") != std::string::npos)   mDeviceTraceOption = RTUtil::DEVICE_TRACE_FINE;
    else if (option.find("coarse") != std::string::npos) mDeviceTraceOption = RTUtil::DEVICE_TRACE_COARSE;
    else {
      mPluginHandle->sendMessage(
        "The data_transfer_trace setting of " + traceStr + " is not recognized. Please use fine|coarse|off.");
    }

    if ((mDeviceTraceOption == RTUtil::DEVICE_TRACE_COARSE) && (std::getenv("XCL_EMULATION_MODE"))) {
      mPluginHandle->sendMessage(
        "The data_transfer_trace setting of " + traceStr + " is not supported in emulation. Fine will be used.");
      mDeviceTraceOption = RTUtil::DEVICE_TRACE_FINE;
    }
  }

  void RTProfile::setStallTrace(const std::string& traceStr) {
    std::string option = traceStr;
    std::transform(option.begin(), option.end(), option.begin(), ::tolower);

    // Memory = External AXI bus to memory
    // Dataflow = Intra-kernel stream
    // Pipe = Inter-Kernel pipes

    if (option.find("off") != std::string::npos)           mStallTraceOption = RTUtil::STALL_TRACE_OFF;
    else if (option.find("memory") != std::string::npos)   mStallTraceOption = RTUtil::STALL_TRACE_EXT;
    else if (option.find("dataflow") != std::string::npos) mStallTraceOption = RTUtil::STALL_TRACE_INT;
    else if (option.find("pipe") != std::string::npos)     mStallTraceOption = RTUtil::STALL_TRACE_STR;
    else if (option.find("all") != std::string::npos)      mStallTraceOption = RTUtil::STALL_TRACE_ALL;
    else {
      mPluginHandle->sendMessage(
        "The stall_trace setting of " + traceStr + " is not recognized. Please use memory|dataflow|pipe|all|off.");
    }
  }

  bool RTProfile::isDeviceProfileOn() const
  {
    // Device profiling is not valid in cpu flow or old emulation flow
    if (mPluginHandle->getFlowMode() == xdp::RTUtil::CPU
       || mPluginHandle->getFlowMode() == xdp::RTUtil::COSIM_EM)
      return false;

    //return mProfileFlags & RTUtil::PROFILE_DEVICE;
    return mProfileFlags & RTUtil::PROFILE_DEVICE_COUNTERS;
  }

  // ***************************************************************************
  // Device Settings (clock freqs, bit widths, etc.)
  // ***************************************************************************

  // Set kernel clock freq on device
  void RTProfile::setTraceClockFreqMHz(unsigned int kernelClockRateMHz) {
    if (mTraceParser == NULL)
      return;
    mTraceParser->setTraceClockFreqMHz(kernelClockRateMHz);
  }

  // Set device clock freq
  void RTProfile::setDeviceClockFreqMHz(double deviceClockRateMHz) {
    if (mTraceParser == NULL)
      return;

    mTraceParser->setDeviceClockFreqMHz(deviceClockRateMHz);

    mProfileCounters->setAllDeviceClockFreqMhz(deviceClockRateMHz);
    mProfileCounters->setAllDeviceBufferBitWidth(mTraceParser->getGlobalMemoryBitWidth());
    mProfileCounters->setAllDeviceKernelBitWidth(mTraceParser->getGlobalMemoryBitWidth());
  }

  // Set device trace clock freq
  void RTProfile::setDeviceTraceClockFreqMHz(double deviceTraceClockRateMHz) {
    if (mTraceParser == NULL)
      return;

    mTraceParser->setTraceClockFreqMHz(deviceTraceClockRateMHz);
  }

  // Set global memory bit width
  void RTProfile::setGlobalMemoryBitWidth(uint32_t bitWidth) {
    if (mTraceParser == NULL)
      return;

    mTraceParser->setGlobalMemoryBitWidth(bitWidth);
  }

  // Get global memory bit width
  uint32_t RTProfile::getGlobalMemoryBitWidth() {
    if (mTraceParser == NULL)
      return XPAR_AXI_PERF_MON_0_SLOT0_DATA_WIDTH;

    return mTraceParser->getGlobalMemoryBitWidth();
  }

  // Get threshold of device trace fifo where we decide to read it
  uint32_t RTProfile::getTraceSamplesThreshold() {
    if (mTraceParser == NULL)
      return 1000;

    return mTraceParser->getTraceSamplesThreshold();
  }

  // Get sample interval for reading device profile counters
  uint32_t RTProfile::getSampleIntervalMsec() {
    if (mTraceParser == NULL)
      return 10;

    return mTraceParser->getSampleIntervalMsec();
  }

  // Record wall-clock time points for start and end of profiling. Used to get an approximate total host time
  void RTProfile::setProfileStartTime(std::chrono::steady_clock::time_point t) { mProfileCounters->setProfileStartTime(t); }
  void RTProfile::setProfileEndTime(std::chrono::steady_clock::time_point t)   { mProfileCounters->setProfileEndTime(t); }
  double RTProfile::getTotalHostTimeInMilliSec() { return mProfileCounters->getTotalHostTimeInMilliSec(); }

  // ***************************************************************************
  // Profile & Trace Writers
  // ***************************************************************************

  void RTProfile::attach(ProfileWriterI* writer)
  {
    if (writer == nullptr)
      return;

    mWriter->attach(writer);

    // Gather data for RunSummary
    if ((mFileFlags & RTUtil::FILE_SUMMARY)) {
      const std::string fileName = writer->getFileName();
      if (!fileName.empty()) {
        mRunSummary->addFile(fileName, RunSummary::FT_PROFILE);
      }
    }
  }

  void RTProfile::attach(TraceWriterI* writer)
  {
    mLogger->attach(writer);

    // Gather data for TimingSummary
    if ((mFileFlags & RTUtil::FILE_TIMELINE_TRACE) && (writer != nullptr)) {
      mRunSummary->addFile(writer->getFileName(), RunSummary::FT_TRACE);
    }
  }

  void RTProfile::detach(ProfileWriterI* writer)
  {
    mWriter->detach(writer);
  }

  void RTProfile::detach(TraceWriterI* writer)
  {
    mLogger->detach(writer);
  }

  void RTProfile::writeProfileSummary() {
    if (!isApplicationProfileOn())
      return;

    mWriter->writeProfileSummary(this);
  }

  // ***************************************************************************
  // Names & Strings
  // ***************************************************************************

  std::string RTProfile::getDeviceNames(const std::string& sep) const
  {
    bool firstDevice = true;
    std::string deviceNames;

    for (auto deviceName : mDeviceNames) {
      deviceNames += (firstDevice) ? deviceName : (sep + deviceName);
      firstDevice = false;
    }
    return deviceNames;
  }

  std::string RTProfile::getProjectName() const
  {
    return mLogger->getCurrentBinaryName();
  }

  int RTProfile::getMigrateMemCalls() const
  {
    return mLogger->getMigrateMemCalls();
  }

  int RTProfile::getHostP2PTransfers() const
  {
    return mLogger->getHostP2PTransfers();
  }

  const std::set<std::thread::id>& RTProfile::getThreadIds()
  {
    return mLogger->getThreadIds();
  }

  // ***************************************************************************
  // Trace logging
  // ***************************************************************************

  void RTProfile::setLoggingTrace(int index, bool value)
  {
    if (index < XCL_PERF_MON_TOTAL_PROFILE)
      mLoggingTrace[index] = value;
  }

  bool RTProfile::getLoggingTrace(int index)
  {
    if (index >= XCL_PERF_MON_TOTAL_PROFILE)
      return false;
    return mLoggingTrace[index];
  }

  // ***************************************************************************
  // Guidance metadata
  // ***************************************************************************

  double RTProfile::getDeviceStartTime(const std::string& deviceName) const {
    return mProfileCounters->getDeviceStartTime(deviceName);
  }

  double RTProfile::getTotalKernelExecutionTime(const std::string& deviceName) const {
    return mProfileCounters->getTotalKernelExecutionTime(deviceName);
  }

  double RTProfile::getTotalApplicationKernelTimeMsec() const {
    if (mTraceParser == NULL)
      return 0;

    return mTraceParser->getTotalKernelTimeMsec();
  }

  uint32_t RTProfile::getComputeUnitCalls(const std::string& deviceName, const std::string& cuName) const {
    return mProfileCounters->getComputeUnitCalls(deviceName, cuName);
  }

  // ***************************************************************************
  // External access to writer
  // ***************************************************************************

  void RTProfile::logDeviceCounters(const std::string& deviceName, const std::string& binaryName, uint32_t programId,
      xclPerfMonType type, xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram)
  {
    mWriter->logDeviceCounters(deviceName, binaryName, programId, type, counterResults, timeNsec, firstReadAfterProgram);
  }

  void RTProfile::writeAPISummary(ProfileWriterI* writer) const
  {
    mWriter->writeAPISummary(writer);
  }
  void RTProfile::writeKernelSummary(ProfileWriterI* writer) const
  {
    mWriter->writeKernelSummary(writer);
  }
  void RTProfile::writeStallSummary(ProfileWriterI* writer) const
  {
    mWriter->writeStallSummary(writer);
  }
  void RTProfile::writeKernelStreamSummary(ProfileWriterI* writer)
  {
    mWriter->writeKernelStreamSummary(writer);
  }
  void RTProfile::writeComputeUnitSummary(ProfileWriterI* writer) const
  {
    return mWriter->writeComputeUnitSummary(writer);
  }
  void RTProfile::writeTransferSummary(ProfileWriterI* writer, RTUtil::e_monitor_type monitorType) const
  {
    mWriter->writeTransferSummary(writer, monitorType);
  }
  void RTProfile::writeKernelTransferSummary(ProfileWriterI* writer)
  {
    mWriter->writeKernelTransferSummary(writer);
  }
  void RTProfile::writeDeviceTransferSummary(ProfileWriterI* writer) const
  {
    mWriter->writeDeviceTransferSummary(writer);
  }
  void RTProfile::writeTopKernelSummary(ProfileWriterI* writer) const
  {
    mWriter->writeTopKernelSummary(writer);
  }
  void RTProfile::writeTopKernelTransferSummary(ProfileWriterI* writer) const
  {
    mWriter->writeTopKernelTransferSummary(writer);
  }
  void RTProfile::writeTopDataTransferSummary(ProfileWriterI* writer, bool isRead) const
  {
    mWriter->writeTopDataTransferSummary(writer, isRead);
  }
  void RTProfile::writeTopDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const
  {
    mWriter->writeTopDeviceTransferSummary(writer, isRead);
  }

  // ***************************************************************************
  // External access to logger
  // ***************************************************************************

  void RTProfile::logFunctionCallStart(const char* functionName, long long queueAddress, unsigned int functionID)
  {
    mLogger->logFunctionCallStart(functionName, queueAddress, functionID);
  }

  void RTProfile::logFunctionCallEnd(const char* functionName, long long queueAddress, unsigned int functionID)
  {
    mLogger->logFunctionCallEnd(functionName, queueAddress, functionID);
  }

  void RTProfile::logDataTransfer(uint64_t objId, RTUtil::e_profile_command_kind objKind,
      RTUtil::e_profile_command_state objStage, size_t objSize, uint32_t contextId,
      size_t numDevices, const std::string& deviceName, uint32_t commandQueueId,
      uint64_t srcAddress, const std::string& srcBank,
      uint64_t dstAddress, const std::string& dstBank,
      std::thread::id threadId, const std::string eventString,
      const std::string dependString, double timeStampMsec)
  {
    mLogger->logDataTransfer(objId, objKind, objStage, objSize, contextId, numDevices,
        deviceName, commandQueueId, srcAddress, srcBank, dstAddress, dstBank, threadId,
        eventString, dependString, timeStampMsec);
  }

  void RTProfile::logKernelExecution(uint64_t objId, uint32_t programId, uint64_t eventId,
      RTUtil::e_profile_command_state objStage, const std::string& kernelName, const std::string& xclbinName,
      uint32_t contextId, uint32_t commandQueueId, const std::string& deviceName, unsigned int uid,
      const size_t* globalWorkSize, size_t workGroupSize, const size_t* localWorkDim,
      const std::string& cu_name, const std::string eventString, const std::string dependString,
      double timeStampMsec)
  {
    mLogger->logKernelExecution(objId, programId, eventId, objStage, kernelName, xclbinName,
   	    contextId, commandQueueId, deviceName, uid, globalWorkSize, workGroupSize, localWorkDim,
		cu_name, eventString, dependString, timeStampMsec);
  }

  void RTProfile::logDependency(RTUtil::e_profile_command_kind objKind,
      const std::string& eventString, const std::string& dependString)
  {
    mLogger->logDependency(objKind, eventString, dependString);
  }

  void RTProfile::logDeviceTrace(const std::string& deviceName, const std::string& binaryName, xclPerfMonType type,
      xclTraceResultsVector& traceVector, bool endLog)
  {
    mLogger->logDeviceTrace(deviceName, binaryName, type, traceVector, endLog);
  }

} // xdp
