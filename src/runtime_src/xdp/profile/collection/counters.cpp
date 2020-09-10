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

#include "counters.h"
#include "results.h"
#include "xdp/profile/profile_config.h"
#include "xdp/profile/writer/base_profile.h"
#include "xdp/profile/writer/base_trace.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>

namespace xdp {
  // ****************
  // Sorted Top Usage
  // ****************
  template <typename T>
  void TimeTraceSortedTopUsage<T>::push(T* newElement)
  {
    auto itr = Storage.begin();
    while (itr != Storage.end()) {
      if ((*itr)->getDuration() < newElement->getDuration()) {
        break;
      }
      itr++;
    }
    if (itr == Storage.end()) {
      Storage.push_back(newElement);
    }
    else {
      Storage.insert(itr, newElement);
    }
    if (Storage.size() > Limit) {
      T::recycle(Storage.back());
      Storage.pop_back();
    }
  }

  template <typename T>
  void TimeTraceSortedTopUsage<T>::writeTopUsageSummary(ProfileWriterI* writer) const
  {
    for (const auto &it : Storage) {
      it->write(writer);
    }
  }

  // ********************
  // Performance Counters
  // ********************
  ProfileCounters::ProfileCounters() :
    TopKernelTimes(), TopBufferReadTimes(), TopBufferWriteTimes(),
    TopKernelReadTimes(), TopKernelWriteTimes(),
    TopDeviceBufferReadTimes(), TopDeviceBufferWriteTimes()
  {
    // do nothing
  }

  void ProfileCounters::logBufferTransfer(RTUtil::e_profile_command_kind kind, size_t size, double duration,
                                          uint32_t contextId, size_t numDevices)
  {
    if (BufferTransferStats.find(kind) == BufferTransferStats.end()) {
      BufferStats bufferStats;
      BufferTransferStats[kind] = bufferStats;
    }

    XOCL_DEBUGF("logBufferTransfer: kind=%d, size=%d, duration=%.3f, context=%d\n", kind, size, duration, contextId);
    BufferTransferStats[kind].log(size, duration);
    BufferTransferStats[kind].setContextId(contextId);
    BufferTransferStats[kind].setNumDevices(numDevices);
  }

  void ProfileCounters::logDeviceRead(size_t size, double duration)
  {
    DeviceBufferReadStat.log(size, duration);
  }

  void ProfileCounters::logDeviceWrite(size_t size, double duration)
  {
    DeviceBufferWriteStat.log(size, duration);
  }

  void ProfileCounters::logDeviceKernel(size_t size, double duration)
  {
    DeviceKernelStat.log(size, duration);
  }

  void ProfileCounters::logDeviceKernelTransfer(std::string& /*deviceName*/, std::string& /*kernelName*/,
      size_t size, double duration, uint32_t bitWidth, double clockFreqMhz, bool isRead)
  {
    // For now, classify under 'ALL'
    std::string name = "ALL";

    if (isRead)
      DeviceKernelReadSummaryStats[name].log(size, duration, bitWidth, clockFreqMhz);
    else
      DeviceKernelWriteSummaryStats[name].log(size, duration, bitWidth, clockFreqMhz);
  }

  void ProfileCounters::logFunctionCallStart(const std::string& functionName, double timePoint)
  {
    auto threadId = std::this_thread::get_id() ;
    auto key      = std::make_pair(functionName, threadId) ;
    auto value    = std::make_pair(timePoint, (double)0.0) ;

    if (CallCount.find(key) == CallCount.end())
    {
      std::vector<std::pair<double, double>> newValue ;
      newValue.push_back(value) ;
      CallCount[key] = newValue ;
    }
    else
    {
      CallCount[key].push_back(value);
    }
  }

  void ProfileCounters::logFunctionCallEnd(const std::string& functionName, double timePoint)
  {
    auto threadId = std::this_thread::get_id() ;
    auto key = std::make_pair(functionName, threadId) ;

    CallCount[key].back().second = timePoint ;
  }

  void ProfileCounters::logKernelExecutionStart(const std::string& kernelName, const std::string& deviceName,
                                                   double timePoint)
  {
    KernelExecutionStats[kernelName].logStart(timePoint);

    auto iter = DeviceStartTimes.find(deviceName);
    if (iter == DeviceStartTimes.end())
      DeviceStartTimes[deviceName] = timePoint;
    else if (timePoint < iter->second)
      DeviceStartTimes[deviceName] = timePoint;
  }

  void ProfileCounters::logKernelExecutionEnd(const std::string& kernelName, const std::string& deviceName,
                                                 double timePoint)
  {
    KernelExecutionStats[kernelName].logEnd(timePoint);

    auto iter = DeviceEndTimes.find(deviceName);
    if (iter == DeviceEndTimes.end())
      DeviceEndTimes[deviceName] = timePoint;
    else if (timePoint > iter->second)
      DeviceEndTimes[deviceName] = timePoint;
  }

  void ProfileCounters::logComputeUnitDeviceStart(const std::string& deviceName, double timePoint)
  {
    auto iter = DeviceCUStartTimes.find(deviceName);
    if (iter == DeviceCUStartTimes.end())
      DeviceCUStartTimes[deviceName] = timePoint;
    else if (timePoint < iter->second)
      DeviceCUStartTimes[deviceName] = timePoint;
  }

  void ProfileCounters::logComputeUnitExecutionStart(const std::string& cuName, double timePoint)
  {
    ComputeUnitExecutionStats[cuName].logStart(timePoint);
  }

  void ProfileCounters::logComputeUnitExecutionEnd(const std::string& cuName, double timePoint)
  {
    ComputeUnitExecutionStats[cuName].logEnd(timePoint);
  }

  void ProfileCounters::logComputeUnitStats(const std::string& cuName, const std::string& kernelName,
                                            double totalTimeStat, double avgTimeStat, double maxTimeStat,
                                            double minTimeStat, uint32_t totalCalls, uint32_t clockFreqMhz,
                                            uint32_t flags, uint64_t maxParallelIter,
                                            std::string& deviceName, std::string & xclbinName)
  {
    std::string newCU;
    bool foundKernel = false;
    for (const auto &pair : ComputeUnitExecutionStats) {
      auto fullName = pair.first;
      size_t first_index = fullName.find_first_of("|");
      size_t second_index = fullName.find('|', first_index+1);
      size_t third_index = fullName.find('|', second_index+1);
      size_t fourth_index = fullName.find("|", third_index+1);
      size_t fifth_index = fullName.find_last_of("|");
      std::string currCUName = fullName.substr(fourth_index + 1, fifth_index - fourth_index - 1);
      std::string currKernelName = fullName.substr(first_index + 1, second_index - first_index - 1);
      std::string currDeviceName = fullName.substr(0, first_index);
      std::string currBinName = fullName.substr(fifth_index + 1);

      if (currDeviceName != deviceName || currBinName != xclbinName)
        continue;
      if (currCUName == cuName) {
        ComputeUnitExecutionStats[fullName].logStats(totalTimeStat, avgTimeStat, maxTimeStat, minTimeStat,
                                                     totalCalls, clockFreqMhz, flags, maxParallelIter);
        return;
      }
      else if (currKernelName == kernelName) {
        newCU = fullName.substr(0, fourth_index) + "|" + cuName + "|" + fullName.substr(fifth_index + 1);
        foundKernel = true;
      }
    }
    // CR 1003380 - Runtime does not send all CU Names so we create a key
    if (foundKernel && totalTimeStat > 0.0) {
      ComputeUnitExecutionStats[newCU].logStats(totalTimeStat, avgTimeStat, maxTimeStat, minTimeStat,
                                                totalCalls, clockFreqMhz, flags, maxParallelIter);
    }
  }

  void ProfileCounters::logDeviceEvent(std::string deviceName, std::string kernelName, size_t size,
                                          double duration, uint32_t bitWidth, double clockFreqMhz,
                                          bool isKernel, bool isRead, bool isKernelTransfer)
  {
    if (isKernel) {
      logDeviceKernel(size, duration);
    }
    else {
      if (isKernelTransfer) {
        logDeviceKernelTransfer(deviceName, kernelName, size, duration, bitWidth, clockFreqMhz, isRead);
      }
      else {
        if (isRead)
          logDeviceRead(size, duration);
        else
          logDeviceWrite(size, duration);
      }
    }
  }

  void ProfileCounters::pushToSortedTopUsage(KernelTrace* trace)
  {
    TopKernelTimes.push(trace);
  }

  void ProfileCounters::pushToSortedTopUsage(BufferTrace* trace, bool isRead)
  {
    if (isRead)
      TopBufferReadTimes.push(trace);
    else
      TopBufferWriteTimes.push(trace);
  }

  void ProfileCounters::pushToSortedTopUsage(DeviceTrace* trace, bool isRead, bool isKernel)
  {
    if (isKernel) {
      if (isRead)
        TopKernelReadTimes.push(trace);
      else
        TopKernelWriteTimes.push(trace);
    }
    else {
      if (isRead)
        TopDeviceBufferReadTimes.push(trace);
      else
        TopDeviceBufferWriteTimes.push(trace);
    }
  }

  //
  // Writers
  //
  void ProfileCounters::writeTopKernelSummary(ProfileWriterI* writer) const {
    TopKernelTimes.writeTopUsageSummary(writer);
  }

  void ProfileCounters::writeTopDataTransferSummary(ProfileWriterI* writer, bool isRead) const {
    if (isRead)
      TopBufferReadTimes.writeTopUsageSummary(writer);
    else
      TopBufferWriteTimes.writeTopUsageSummary(writer);
  }

  void ProfileCounters::writeTopDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const {
    if (isRead)
      TopDeviceBufferReadTimes.writeTopUsageSummary(writer);
    else
      TopDeviceBufferWriteTimes.writeTopUsageSummary(writer);
  }

  void ProfileCounters::setAllDeviceBufferBitWidth(uint32_t bitWidth)
  {
    DeviceBufferReadStat.setBitWidth(bitWidth);
    DeviceBufferWriteStat.setBitWidth(bitWidth);

    for (auto iter : BufferTransferStats) {
      iter.second.setBitWidth(bitWidth);
    }
  }

  void ProfileCounters::setAllDeviceKernelBitWidth(uint32_t bitWidth)
  {
    DeviceKernelStat.setBitWidth(bitWidth);
  }

  void ProfileCounters::setAllDeviceClockFreqMhz(double clockFreqMhz)
  {
    DeviceBufferReadStat.setClockFreqMhz(clockFreqMhz);
    DeviceBufferWriteStat.setClockFreqMhz(clockFreqMhz);
    DeviceKernelStat.setClockFreqMhz(clockFreqMhz);

    for (auto iter : BufferTransferStats) {
      iter.second.setClockFreqMhz(clockFreqMhz);
    }
  }

  // Get device start time (NOTE: when first CU starts)
  double ProfileCounters::getDeviceStartTime(const std::string& deviceName) const
  {
    auto startIter = DeviceStartTimes.find(deviceName);

    // If device is not found, return 0
    if (startIter == DeviceStartTimes.end())
      return 0.0;

    return startIter->second;
  }

  double ProfileCounters::getTotalKernelExecutionTime(const std::string& deviceName) const
  {
    auto startIter = DeviceStartTimes.find(deviceName);
    auto endIter = DeviceEndTimes.find(deviceName);

    // If device is not found, return 0
    if ((startIter == DeviceStartTimes.end()) || (endIter == DeviceEndTimes.end()))
      return 0.0;

#if 1
    double totalTime = endIter->second - startIter->second;
#else
    // FYI, method used pre-2015.4
    double totalTime = 0.0;
    for (const auto &pair : KernelExecutionStats) {
      TimeStats stats = pair.second;
      totalTime += stats.getTotalTime();
    }
#endif

    XDP_LOG("getTotalKernelExecutionTime: total kernel time = %f - %f = %f for device: %s\n",
            endIter->second, startIter->second, totalTime, deviceName.c_str());
    return totalTime;
  }

  uint32_t ProfileCounters::getComputeUnitCalls(const std::string& deviceName,
      const std::string& cuName) const
  {
    auto cuIter = ComputeUnitExecutionStats.begin();
    auto cuEnd  = ComputeUnitExecutionStats.end();

    while (cuIter != cuEnd) {
      //"name" is of the form "deviceName|kernelName|globalSize|localSize|cuName|objId"
      std::string name = cuIter->first;
      name = name.substr(0, name.find_last_of("|"));
      if (name.find(deviceName) != std::string::npos && name.find(cuName) != std::string::npos) {
        TimeStats cuStats = cuIter->second;
        return cuStats.getNoOfCalls();
      }
      cuIter++;
    }

    // If CU is not found, return 0
    return 0;
  }

  double ProfileCounters::getComputeUnitTotalTime(const std::string& deviceName,
                                                     const std::string& cuName) const
  {
    for (const auto &pair : ComputeUnitExecutionStats) {
      auto fullName = pair.first;
      if (fullName.find(deviceName) != std::string::npos
          && fullName.find(cuName)  != std::string::npos) {
        auto stat = pair.second;
        return stat.getTotalTime();
      }
    }
    return getTotalKernelExecutionTime(deviceName);
  }

  double ProfileCounters::getBufferTransferTotalTime(RTUtil::e_profile_command_kind kind)
  {
    if (BufferTransferStats.find(kind) != BufferTransferStats.end()) {
      return BufferTransferStats[kind].getTotalTime();
    }

    return 0.0;
  }

  void ProfileCounters::writeKernelSummary(ProfileWriterI* writer) const
  {
    for (const auto &pair : KernelExecutionStats) {
      auto fullName = pair.first;
      auto kernelName = fullName.substr(0, fullName.find_first_of("|"));
      writer->writeTimeStats(kernelName, pair.second);
    }
  }

  void ProfileCounters::writeComputeUnitSummary(ProfileWriterI* writer) const
  {
    for (const auto &pair : ComputeUnitExecutionStats) {
      auto fullName = pair.first;
      auto cuName = fullName.substr(0, fullName.find_last_of("|"));
      writer->writeComputeUnitSummary(cuName, pair.second);
    }
  }

  void ProfileCounters::writeAPISummary(ProfileWriterI* writer) const
  {
    using std::vector;
    using std::pair;
    using std::sort;
    using std::string;
    
    // Go through all of the call values and consolidate all of the
    //  API calls from different threads into a single TimeStats object
    std::map<std::string, TimeStats> consolidated ;
    for (auto iter : CallCount)
    {
      auto functionName = iter.first.first ;
      auto pairVector   = iter.second ;

      for (unsigned int i = 0 ; i < pairVector.size() ; ++i)
      {
	auto doublePair = pairVector[i] ;
	consolidated[functionName].logStart(doublePair.first) ;
	consolidated[functionName].logEnd(doublePair.second) ;
      }
    }

    // Print it in sorted order of Total Time. To sort it by duration
    // populate a vector and then using lambda function sort it by duration

    vector<pair<string, TimeStats>> callPairs(consolidated.begin(),
        consolidated.end());
    sort(callPairs.begin(), callPairs.end(),
        [](const pair<string, TimeStats>& A, const pair<string, TimeStats>& B) {
      return A.second.getTotalTime() > B.second.getTotalTime();
    });

    for (const auto &pair : callPairs) {
      writer->writeTimeStats(pair.first, pair.second);
    }
  }

  // CR 951564: Use APM counters to calculate write throughput (i.e., byte count / total latency)
  // CR 956489: APM is reporting 2x the write bytes; let's use the buffer stat bytes instead
  void ProfileCounters::writeBufferStat(ProfileWriterI* writer, const std::string transferType,
      const BufferStats &bufferStat, double maxTransferRateMBps) const
  {
    uint64_t totalTranx  = bufferStat.getCount();
    uint64_t totalBytes  = bufferStat.getTotalSize();

    double minTotalTimeMsec = totalBytes / (1000.0 * maxTransferRateMBps);
    double totalTimeMsec = (bufferStat.getTotalTime() < minTotalTimeMsec) ?
        minTotalTimeMsec : bufferStat.getTotalTime();

    writer->writeHostTransferSummary(transferType, bufferStat, totalBytes, totalTranx,
      totalTimeMsec, maxTransferRateMBps);

  }

  // Write data transfer stats for: host, XDMA, KDMA, and P2P
  void ProfileCounters::writeTransferSummary(ProfileWriterI* writer, const std::string& deviceName,
      RTUtil::e_monitor_type monitorType, bool isRead, uint64_t totalBytes, uint64_t totalTranx,
	  double totalLatencyNsec, double totalTimeMsec, double maxTransferRateMBps) const
  {
    std::string transferType = (isRead) ? "READ" : "WRITE";

    // Host transfers
    if (monitorType == RTUtil::MON_HOST_DYNAMIC) {
      RTUtil::e_profile_command_kind kind = (isRead) ? RTUtil::READ_BUFFER : RTUtil::WRITE_BUFFER;

      if (BufferTransferStats.find(kind) != BufferTransferStats.end()) {
        writeBufferStat(writer, transferType, BufferTransferStats.at(kind), maxTransferRateMBps);
      }
      return;
    }

    // Now write results from shell monitors (i.e., KDMA/XDMA/P2P)
    if (monitorType == RTUtil::MON_SHELL_P2P)
      transferType = (isRead) ? "OUT" : "IN";

    writer->writeShellTransferSummary(deviceName, transferType, totalBytes, totalTranx,
                                      totalLatencyNsec, totalTimeMsec);
  }

  void ProfileCounters::writeKernelTransferSummary(ProfileWriterI* writer, std::string& deviceName,
      std::string& cuPortName, const std::string& argNames, const std::string& memoryName,
    bool isRead, uint64_t totalBytes, uint64_t totalTranx, double totalTxTimeMsec,
    double totalTxLatencyMsec, double maxTransferRateMBps) const
  {
    std::string transferType = (isRead) ? "READ" : "WRITE";

    writer->writeKernelTransferSummary(deviceName, cuPortName, argNames, memoryName, transferType,
      totalBytes, totalTranx, totalTxTimeMsec, totalTxLatencyMsec, maxTransferRateMBps);
  }

  void ProfileCounters::writeTopKernelTransferSummary(
      ProfileWriterI* writer, std::string& deviceName, std::string &cuName,
      uint64_t totalWriteBytes, uint64_t totalReadBytes,
      uint64_t totalWriteTranx, uint64_t totalReadTranx,
      double totalWriteTimeMsec, double totalReadTimeMsec,
      uint32_t maxBytesPerTransfer, double maxTransferRateMBps) const
  {
    writer->writeTopKernelTransferSummary(
        deviceName, cuName,
        totalWriteBytes, totalReadBytes,
        totalWriteTranx, totalReadTranx,
        totalWriteTimeMsec, totalReadTimeMsec,
        maxBytesPerTransfer, maxTransferRateMBps);
  }

  void ProfileCounters::writeDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const
  {
    std::string transferType("DEVICE WRITE BUFFER");
    const BufferStats* bufferStat = &DeviceBufferWriteStat;
    if (isRead) {
      transferType = "DEVICE READ BUFFER";
      bufferStat = &DeviceBufferReadStat;
    }
    writer->writeBufferStats(transferType, *bufferStat);
  }

} // xdp
