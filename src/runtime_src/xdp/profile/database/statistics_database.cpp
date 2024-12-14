/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include <vector>
#include <thread>
#include <iostream>

#define XDP_CORE_SOURCE

#include "xdp/profile/database/statistics_database.h"

namespace xdp {

  VPStatisticsDatabase::VPStatisticsDatabase(VPDatabase* d) :
    db(d), numMigrateMemCalls(0), numHostP2PTransfers(0),
    numObjectsReleased(0), contextEnabled(false),
    totalHostReadTime(0), totalHostWriteTime(0), totalBufferStartTime(0),
    totalBufferEndTime(0), firstKernelStartTime(0.0), lastKernelEndTime(0.0)
  {
  }

  VPStatisticsDatabase::~VPStatisticsDatabase()
  {
  }

  void VPStatisticsDatabase::addTopHostRead(BufferTransferStats& transfer)
  {
    // Edge case: First read.
    if (topHostReads.size() == 0)
    {
      topHostReads.push_back(transfer) ;
      return ;
    }
    
    // Standard case: Insert in sorted order
    bool inserted = false ;
    for (std::list<BufferTransferStats>::iterator iter = topHostReads.begin() ;
         iter != topHostReads.end() ;
         ++iter)
    {
      if (transfer.getDuration() > (*iter).getDuration())
      {
        topHostReads.insert(iter, transfer) ;
        inserted = true ;
        break ;
      }
    }

    // Edge case: Transfer is smaller than currently stored values
    if (!inserted)
    {
      topHostReads.push_back(transfer) ;
    }

    // Clean up any extra elements
    while (topHostReads.size() > numTopTransfers)
    {
      topHostReads.pop_back() ;
    }
  }

  void VPStatisticsDatabase::addTopHostWrite(BufferTransferStats& transfer)
  {
    // Edge case: First write.
    if (topHostWrites.size() == 0)
    {
      topHostWrites.push_back(transfer) ;
      return ;
    }
    
    // Standard case: Insert in sorted order
    bool inserted = false ;
    for (std::list<BufferTransferStats>::iterator iter = topHostWrites.begin() ;
         iter != topHostWrites.end() ;
         ++iter)
    {
      if (transfer.getDuration() > (*iter).getDuration())
      {
        topHostWrites.insert(iter, transfer) ;
        inserted = true ;
        break ;
      }
    }

    // Edge case: Transfer is smaller than currently stored values
    if (!inserted)
    {
      topHostWrites.push_back(transfer) ;
    }

    // Clean up any extra elements
    while (topHostWrites.size() > numTopTransfers)
    {
      topHostWrites.pop_back() ;
    }
  }

  void VPStatisticsDatabase::addTopKernelExecution(KernelExecutionStats& exec)
  {
    // Edge case: First execution
    if (topKernelExecutions.size() == 0)
    {
      topKernelExecutions.push_back(exec) ;
      return ;
    }
    
    // Standard case: Insert in sorted order
    bool inserted = false ;
    for (std::list<KernelExecutionStats>::iterator iter = topKernelExecutions.begin() ;
         iter != topKernelExecutions.end() ;
         ++iter)
    {
      if (exec.duration > (*iter).duration)
      {
        topKernelExecutions.insert(iter, exec) ;
        inserted = true ;
        break ;
      }
    }

    // Edge case: Transfer is smaller than currently stored values
    if (!inserted)
    {
      topKernelExecutions.push_back(exec) ;
    }

    // Clean up any extra elements
    while (topKernelExecutions.size() > numTopKernelExecutions)
    {
      topKernelExecutions.pop_back() ;
    }
  }

  // For a given CU identified by name, collect all the global work group
  //  configurations + statistics
  std::vector<std::pair<std::string, TimeStatistics>>
  VPStatisticsDatabase::getComputeUnitExecutionStats(const std::string& cuName)
  {
    std::vector<std::pair<std::string, TimeStatistics>> calls;
    for (const auto& element : computeUnitExecutionStats) {
      if (0 == cuName.compare(std::get<0>(element.first))) {
        calls.push_back(std::make_pair(std::get<2>(element.first), element.second));
      }
    }
    return calls;
  }

  uint64_t VPStatisticsDatabase::getDeviceActiveTime(const std::string& deviceName)
  {
    if (deviceActiveTimes.find(deviceName) == deviceActiveTimes.end())
      return 0 ;
    std::pair<uint64_t, uint64_t> time = deviceActiveTimes[deviceName] ;
    return time.second - time.first ;
  }

  void VPStatisticsDatabase::addEventCount(const char* label)
  {
    std::string converted = "" ;
    if (label != nullptr) {
      converted = label ;
    }
    if (eventCounts.find(converted) == eventCounts.end()) {
      eventCounts[converted] = 0 ;
    }

    eventCounts[converted] += 1 ;
  }

  void VPStatisticsDatabase::addRangeCount(std::pair<const char*, const char*> desc)
  {
    if (rangeCounts.find(desc) == rangeCounts.end()) {
      rangeCounts[desc] = 0 ;
    }

    rangeCounts[desc] += 1 ;
  }

  void VPStatisticsDatabase::recordRangeDuration(std::pair<const char*, const char*> desc, uint64_t duration)
  {
    if (minRangeDurations.find(desc) == minRangeDurations.end()) {
      // First time seeing this particular range
      minRangeDurations[desc]   = duration ;
      maxRangeDurations[desc]   = duration ;
      totalRangeDurations[desc] = duration ;
    }
    else {
      if (duration < minRangeDurations[desc])
        minRangeDurations[desc] = duration ;
      if (duration > maxRangeDurations[desc])
        maxRangeDurations[desc] = duration ;
      totalRangeDurations[desc] += duration ;
    }
  }

  void VPStatisticsDatabase::logFunctionCallStart(const std::string& name,
                                                  double timestamp)
  {
    std::lock_guard<std::mutex> lock(dbLock);

    // Each function that we are tracking will have two distinct entry
    // points that we need to keep track of, the starting point
    // and the ending point.  In this function, we log the starting point
    // of a function call.  Since the calls could be coming in simultaneously
    // from different threads, we also use the thread id to create
    // a unique identifier.

    auto threadId = std::this_thread::get_id();
    auto key      = std::make_pair(name, threadId);
    std::pair<double, double>value {timestamp, 0.0};

    // Since a single thread can call a function multiple times, we store
    // the starts in a vector.  If the thread makes a recursive call, we'll
    // have multiple elements where the start value is set but the end value
    // needs to be filled in.
    if (callCount.find(key) == callCount.end()) {
      std::vector<std::pair<double, double>> newVector;
      newVector.push_back(value);
      callCount[key] = newVector;
    }
    else
      callCount[key].push_back(value);

    // OpenCL specific information 
    if (name == "clEnqueueMigrateMemObjects")
      addMigrateMemCall();
  }

  void VPStatisticsDatabase::logFunctionCallEnd(const std::string& name,
                                                double timestamp)
  {
    std::lock_guard<std::mutex> lock(dbLock);

    auto threadId = std::this_thread::get_id();
    auto key      = std::make_pair(name, threadId);

    // Since some calls might be recursive, we must go backwards to find
    // the first call that has a start time set but no end time.  Since we've
    // incorporated the thread id as part of our key, we will match recursive
    // calls correctly
    for (auto iter = callCount[key].rbegin();
         iter != callCount[key].rend();
         ++iter) {
      if ((*iter).second == 0) {
        (*iter).second = timestamp;
        break;
      }
    }
  }

  void VPStatisticsDatabase::logMemoryTransfer(uint64_t deviceId,
                                                DeviceMemoryStatistics::ChannelType channelNum,
                                                size_t count)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    
    if (memoryStats.find(deviceId) == memoryStats.end())
    {
      DeviceMemoryStatistics blank ;
      memoryStats[deviceId] = blank ;
    }

    (memoryStats[deviceId]).channels[channelNum].transactionCount++;
    (memoryStats[deviceId]).channels[channelNum].totalByteCount += count;
  }

  void VPStatisticsDatabase::logDeviceActiveTime(const std::string& deviceName,
                                                 uint64_t startTime,
                                                 uint64_t endTime)
  {
    if (deviceActiveTimes.find(deviceName) == deviceActiveTimes.end())
    {
      std::pair<uint64_t, uint64_t> execution =
        std::make_pair(startTime, endTime) ;
      deviceActiveTimes[deviceName] = execution ;
    }
    else
    {
      // Don't change the start time, only update the end time
      deviceActiveTimes[deviceName].second = endTime ;
    }
  }

  void VPStatisticsDatabase::logKernelExecution(const std::string& kernelName,
                                                uint64_t executionTime,
                                                uint64_t kernelInstanceAddress,
                                                uint64_t contextId,
                                                uint64_t commandQueueId,
                                                const std::string& deviceName,
                                                uint64_t startTime,
                                                const std::string& globalWorkSize,
                                                const std::string& localWorkSize,
                                                const char** buffers,
                                                uint64_t numBuffers)
  {
    if (kernelExecutionStats.find(kernelName) == kernelExecutionStats.end())
    {
      TimeStatistics blank ;
      kernelExecutionStats[kernelName] = blank ;
    }
    (kernelExecutionStats[kernelName]).update(executionTime) ;
    kernelGlobalWorkGroups[kernelName] = globalWorkSize ;

    // Also keep track of top kernel executions
    KernelExecutionStats exec ;
    exec.kernelInstanceAddress = kernelInstanceAddress ;
    exec.kernelName = kernelName ;
    exec.contextId = contextId ;
    exec.commandQueueId = commandQueueId ;
    exec.deviceName = deviceName ;
    exec.startTime = startTime ;
    exec.duration = executionTime ;
    exec.globalWorkSize = globalWorkSize ;
    exec.localWorkSize = localWorkSize ;
    addTopKernelExecution(exec) ;

    // Also keep track of kernel buffers
    if (bufferInfo.find(kernelName) == bufferInfo.end()) {
      std::vector<std::string> blank ;
      bufferInfo[kernelName] = blank ;
      for (uint64_t i = 0 ; i < numBuffers ; ++i) {
        std::string convert = buffers[i] ;
        bufferInfo[kernelName].push_back(convert) ;
      }
    }
  }

  void VPStatisticsDatabase::logComputeUnitExecution(const std::string& computeUnitName,
                                                     const std::string& kernelName,
                                                     const std::string& localWorkGroup,
                                                     const std::string& globalWorkGroup,
                                                     uint64_t executionTime)
  {
    // If global work size is not known, then we need to get it from the latest enqueue 
    // of the associated kernel.
    std::string globalWork = globalWorkGroup;
    if (globalWorkGroup.empty()) {
      auto globalIter = kernelGlobalWorkGroups.find(kernelName);
      globalWork = (globalIter != kernelGlobalWorkGroups.end()) ? globalIter->second : localWorkGroup;
    }

    std::tuple<std::string, std::string, std::string> combinedName =
      std::make_tuple(computeUnitName, localWorkGroup, globalWork) ;

    if (computeUnitExecutionStats.find(combinedName) == computeUnitExecutionStats.end())
    {
      TimeStatistics blank ;
      computeUnitExecutionStats[combinedName] = blank ;
    }
    (computeUnitExecutionStats[combinedName]).update(executionTime) ;
  }

  void VPStatisticsDatabase::logHostRead(uint64_t contextId, uint64_t deviceId,
                                         uint64_t size, uint64_t startTime,
                                         uint64_t transferTime,
                                         uint64_t address,
                                         uint64_t commandQueueId)
  {
    std::lock_guard<std::mutex> lock(readsLock) ;

    std::pair<uint64_t, uint64_t> identifier = 
      std::make_pair(contextId, deviceId) ;
    
    if (hostReads.find(identifier) == hostReads.end())
    {
      BufferStatistics blank ;
      hostReads[identifier] = blank ;
    }

    hostReads[identifier].update(size, transferTime) ;

    totalHostReadTime += transferTime ;

    // Also keep track of the top host reads
    BufferTransferStats transfer ;
    transfer.size = size ;
    transfer.address = address ;
    transfer.contextId = contextId ;
    transfer.commandQueueId = commandQueueId ;
    transfer.startTime = startTime ;
    transfer.duration = transferTime ;
    addTopHostRead(transfer) ;
  }

  void VPStatisticsDatabase::logHostWrite(uint64_t contextId, uint64_t deviceId,
                                          uint64_t size, uint64_t startTime,
                                          uint64_t transferTime,
                                          uint64_t address,
                                          uint64_t commandQueueId)
  {
    std::lock_guard<std::mutex> lock(writesLock) ;

    std::pair<uint64_t, uint64_t> identifier = 
      std::make_pair(contextId, deviceId) ;
    
    if (hostWrites.find(identifier) == hostWrites.end())
    {
      BufferStatistics blank ;
      hostWrites[identifier] = blank ;
    }

    hostWrites[identifier].update(size, transferTime) ;

    totalHostWriteTime += transferTime ;

    // Also keep track of the top host writes
    BufferTransferStats transfer ;
    transfer.size = size ;
    transfer.address = address ;
    transfer.contextId = contextId ;
    transfer.commandQueueId = commandQueueId ;
    transfer.startTime = startTime ;
    transfer.duration = transferTime ;
    addTopHostWrite(transfer) ;
  }

  void VPStatisticsDatabase::updateCounters(uint64_t /*deviceId*/,
                                            xdp::CounterResults& /*counters*/)
  {
  }

  void VPStatisticsDatabase::updateCounters(xdp::CounterResults& /*counters*/)
  {
  }

  void VPStatisticsDatabase::setFirstKernelStartTime(double startTime)
  {
    if (firstKernelStartTime != 0.0) return ;
    firstKernelStartTime = startTime ;
  }

  void VPStatisticsDatabase::dumpCallCount(std::ofstream& fout)
  {
    // For each function call, across all of the threads, find out
    //  the number of calls
    std::map<std::string, uint64_t> counts ;

    for (const auto& c : callCount)
    {
      if (counts.find(c.first.first) == counts.end())
      {
        counts[c.first.first] = c.second.size() ;
      }
      else
      {
        counts[c.first.first] += c.second.size() ;
      }
    }

    for (const auto& i : counts)
    {
      fout << i.first << "," << i.second << std::endl ;
    }
  }

  void VPStatisticsDatabase::dumpHALMemory(std::ofstream& fout)
  {
    unsigned int i = 0 ; 
    for (const auto& m : memoryStats)
    {
      fout << "Device " << i << std::endl ;

      fout << "\tUnmanaged Reads: " 
           << m.second.channels[0].transactionCount
           << " transactions, "
           << m.second.channels[0].totalByteCount
           << " bytes transferred" << std::endl ;
      fout << "\tUnmanaged Writes: " 
           << m.second.channels[1].transactionCount
           << " transactions, "
           << m.second.channels[1].totalByteCount
           << " bytes transferred" << std::endl ;

      fout << "\txclRead: " 
           << m.second.channels[2].transactionCount
           << " transactions, "
           << m.second.channels[2].totalByteCount
           << " bytes transferred" << std::endl ;
      fout << "\txclWrite: " 
           << m.second.channels[3].transactionCount
           << " transactions, "
           << m.second.channels[3].totalByteCount
           << " bytes transferred" << std::endl ;
     
      fout << "\treadBuffer: " 
           << m.second.channels[4].transactionCount
           << " transactions, "
           << m.second.channels[4].totalByteCount
           << " bytes transferred" << std::endl ;
      fout << "\twriteBuffer: " 
           << m.second.channels[5].transactionCount
           << " transactions, "
           << m.second.channels[5].totalByteCount
           << " bytes transferred" << std::endl ;
    }
  }

} // end namespace xdp
