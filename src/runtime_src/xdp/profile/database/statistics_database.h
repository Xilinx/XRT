/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef VP_STATISTICS_DATABASE_DOT_H
#define VP_STATISTICS_DATABASE_DOT_H

#include <string>
#include <mutex>
#include <thread>
#include <map>
#include <vector>
#include <fstream>
#include <tuple>
#include <list>

// For the device results structures
#include "xclperf.h"

#include "xdp/config.h"

namespace xdp {

  // Forward declarations
  class VPDatabase ;

  // All of the statistics in this database will be used in 
  //  summary files.  Different plugins might use different
  //  information.  This information accumulates throughout 
  //  host code execution and should not be reset when
  //  information is dumped in continuous offload.

  // The BufferStatistics struct keeps track of aggregate information
  //  of all host to device buffer transfers.
  struct BufferStatistics
  {
    uint64_t count ; // Number of buffer transfers
    uint64_t minSize ; // Minimum number of bytes transferred at once
    uint64_t maxSize ; // Maximum number of bytes transferred at once
    //uint64_t contextId ;
    //std::string deviceName
    //uint64_t numDevices ;
    //uint64_t bitWidth ;
    uint64_t totalSize ; // Total number of bytes transferred
    double averageSize ; // Average number of bytes per transfer
    uint64_t totalTime ;
    double averageTime ;
    uint64_t maxTime ;
    uint64_t minTime ;
    //double averageTransferRate ;
    //double clockFreqMhz ;

    BufferStatistics() : 
      count(0),
      minSize((std::numeric_limits<uint64_t>::max)()),
      maxSize(0),
      totalSize(0),
      averageSize(0),
      totalTime(0),
      averageTime(0),
      maxTime(0),
      minTime((std::numeric_limits<uint64_t>::max)())
    { }

    void update(uint64_t size, uint64_t executionTime)
    {
      // Update size stats
      totalSize += size ;
      averageSize = ((averageSize * count) + size) / (count + 1) ;
      if (minSize > size) minSize = size ;
      if (maxSize < size) maxSize = size ;

      // Update time stats
      totalTime += executionTime ;
      averageTime = ((averageTime * count) + executionTime)/(count + 1) ;
      if (minTime > executionTime) minTime = executionTime ;
      if (maxTime < executionTime) maxTime = executionTime ;

      ++count ;
    }

  } ;

  // The struct BufferTransferStats keeps track of a single buffer transfer
  //  so we can report the top N transfers
  struct BufferTransferStats
  {
    uint64_t size ;
    uint64_t address ;
    uint64_t contextId ;
    uint64_t commandQueueId ;

    // Timestamps
    uint64_t startTime ;
    uint64_t duration ;

    BufferTransferStats() :
     size(0), address(0), contextId(0), commandQueueId(0),
     startTime(0), duration(0) 
    {
    }

    uint64_t getDuration() { return duration ; }
  } ;

  struct KernelExecutionStats
  {
    uint64_t kernelInstanceAddress ;
    std::string kernelName ;
    uint64_t contextId ;
    uint64_t commandQueueId ;
    std::string deviceName ;
    uint64_t startTime ;
    uint64_t duration ; 
    std::string globalWorkSize ;
    std::string localWorkSize ;

    KernelExecutionStats() :
     kernelInstanceAddress(0), kernelName(""), contextId(0), commandQueueId(0),
     deviceName(""), startTime(0), duration(0), globalWorkSize(""),
     localWorkSize("") { }
  } ;

  struct TimeStatistics
  {
    uint64_t totalTime ;
    double averageTime ;
    uint64_t maxTime ;
    uint64_t minTime ;
    uint32_t numExecutions ;

    TimeStatistics() : totalTime(0), averageTime(0), maxTime(0), 
      minTime((std::numeric_limits<uint64_t>::max)()), numExecutions(0) { }
    void update(uint64_t executionTime)
    {
      totalTime += executionTime ;
      averageTime = ((averageTime * numExecutions) + executionTime)/(numExecutions + 1) ;
      ++numExecutions ;
      if (maxTime < executionTime) maxTime = executionTime ;
      if (minTime > executionTime) minTime = executionTime ;
    }
  } ;

  struct MemoryChannelStatistics
  {
    uint64_t transactionCount ;
    size_t   totalByteCount ;

    MemoryChannelStatistics() : transactionCount(0), totalByteCount(0) { }
  } ;

  struct DeviceMemoryStatistics
  {
    enum ChannelType { 
      UNMANAGED_READ  = 0,
      UNMANAGED_WRITE = 1,
      XCLREAD         = 2,
      XCLWRITE        = 3,
      BUFFER_READ     = 4,
      BUFFER_WRITE    = 5
    } ;
    MemoryChannelStatistics channels[6] ;
  } ;

  class VPStatisticsDatabase 
  {
  private:
    VPDatabase* db ;

  private:
    // Statistics on API calls (OpenCL and HAL) have to be thread specific
    std::map<std::pair<std::string, std::thread::id>,
             std::vector<std::pair<double, double>>> callCount ;

    // **** HAL Statistics ****
    // For HAL, each device will have four different read/write
    //  channels that need to be kept track of.
    std::map<uint64_t, DeviceMemoryStatistics> memoryStats ;

    // **** OpenCL Statistics ****
    // Statistics on kernel enqueues and executions
    std::map<std::string, TimeStatistics> kernelExecutionStats ;
    std::map<std::string, uint64_t> maxExecutions ; // per kernel
    std::map<std::string, std::vector<std::string>> bufferInfo ;
    
    // Statistics on compute unit enqueues and executions
    //  The tuple is cuName, localWorkGroupConfig, globalWorkgroupConfig
    std::map<std::tuple<std::string, std::string, std::string>, 
             TimeStatistics> computeUnitExecutionStats ;

    // Statistics on specific OpenCL function calls
    uint64_t numMigrateMemCalls ;
    uint64_t numHostP2PTransfers ;
    uint64_t numObjectsReleased ;
    bool contextEnabled ;

    // Statistics on OpenCL constructs
    std::map<uint64_t, bool> commandQueuesAreOOO ;

    // Statistics on buffer transfers
    //  Keep track of reads and writes for every context+device pair
    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostReads ;
    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostWrites ;
    uint64_t totalHostReadTime ;
    uint64_t totalHostWriteTime ;
    uint64_t totalBufferStartTime ;
    uint64_t totalBufferEndTime ;

    // Keep track of the top ten buffer read and write transfers
    const uint64_t numTopTransfers = 10 ;
    std::list<BufferTransferStats> topHostReads ;
    std::list<BufferTransferStats> topHostWrites ;

    // Keep track of the top ten kernel executions
    const uint64_t numTopKernelExecutions = 10 ;
    std::list<KernelExecutionStats> topKernelExecutions ;

    // Keep track of the device start and end times
    std::map<std::string, std::pair<uint64_t, uint64_t>> deviceActiveTimes ;

    // Information used by trace parser
    double firstKernelStartTime ;
    double lastKernelEndTime ;

    // Since the host code can be multithreaded, we must protect 
    //  the data
    std::mutex dbLock ;

    // Helper functions for OpenCL
    void addTopHostRead(BufferTransferStats& transfer) ;
    void addTopHostWrite(BufferTransferStats& transfer) ;
    void addTopKernelExecution(KernelExecutionStats& exec) ;

  public:
    XDP_EXPORT VPStatisticsDatabase(VPDatabase* d) ;
    XDP_EXPORT ~VPStatisticsDatabase() ;

    // Getters and setters
    inline const std::map<std::pair<std::string, std::thread::id>,
                    std::vector<std::pair<double, double>>>& getCallCount() 
      { return callCount ; }
    inline const std::map<uint64_t, DeviceMemoryStatistics>& getMemoryStats() 
      { return memoryStats ; }
    inline const std::map<std::string, TimeStatistics>& getKernelExecutionStats() 
      { return kernelExecutionStats ; }
    inline const std::map<std::tuple<std::string, std::string, std::string>, 
                          TimeStatistics>& getComputeUnitExecutionStats() 
      { return computeUnitExecutionStats ; }
    inline std::map<std::pair<uint64_t, uint64_t>, BufferStatistics>& getHostReads() { return hostReads ; }
    inline std::map<std::pair<uint64_t, uint64_t>, BufferStatistics>& getHostWrites() { return hostWrites ; }
    inline std::list<BufferTransferStats>& getTopHostReads() { return topHostReads ; }
    inline std::list<BufferTransferStats>& getTopHostWrites() { return topHostWrites ; }
    inline std::list<KernelExecutionStats>& getTopKernelExecutions() { return topKernelExecutions ; }
    inline uint64_t getTotalHostReadTime() { return totalHostReadTime ; }
    inline uint64_t getTotalHostWriteTime() { return totalHostWriteTime ; }
    inline uint64_t getTotalBufferStartTime() { return totalBufferStartTime ; }
    inline void setTotalBufferStartTime(uint64_t t) { totalBufferStartTime = t;}
    inline void setTotalBufferEndTime(uint64_t t) { totalBufferEndTime = t ; }
    inline uint64_t getTotalBufferTxTime() { return totalBufferEndTime - totalBufferStartTime ; }

    // Functions specific to guidance statistics
    inline uint64_t getNumMigrateMemCalls()  { return numMigrateMemCalls ; } 
    inline void addMigrateMemCall()          { ++numMigrateMemCalls ; }
    inline uint64_t getNumHostP2PTransfers() { return numHostP2PTransfers ; }
    inline void addHostP2PTransfer()         { ++numHostP2PTransfers ; }
    inline uint64_t getNumOpenCLObjectsReleased() { return numObjectsReleased ;}
    inline void addOpenCLObjectReleased()    { ++numObjectsReleased ; }
    inline bool getContextEnabled()          { return contextEnabled ; }
    inline void setContextEnabled()          { contextEnabled = true ; }
    inline uint64_t getMaxExecutions(const std::string& kernelName)
      { return maxExecutions[kernelName] ; }
    inline std::map<std::string, uint64_t>& getAllMaxExecutions()
      { return maxExecutions ; }
    inline void logMaxExecutions(const std::string& kernelName, uint64_t num)
    { 
      if (maxExecutions.find(kernelName) == maxExecutions.end())
      {
	maxExecutions[kernelName] = num ;
	return ;
      }
      if (num > maxExecutions[kernelName]) maxExecutions[kernelName] = num ;
    }
    inline std::map<std::string, std::vector<std::string>>& getBufferInfo() 
      { return bufferInfo ; }
    inline std::map<uint64_t, bool>& getCommandQueuesAreOOO()
      { return commandQueuesAreOOO ; }
    inline void setCommandQueueOOO(uint64_t cq, bool value)
      { commandQueuesAreOOO[cq] = value ; }
    XDP_EXPORT uint64_t getDeviceActiveTime(const std::string& deviceName) ;

    // Functions specific to compute unit executions
    XDP_EXPORT
    std::vector<std::pair<std::string, TimeStatistics>> 
    getComputeUnitExecutionStats(const std::string& cuName) ;

    // Logging Functions
    XDP_EXPORT void logFunctionCallStart(const std::string& name, 
					 double timestamp) ;
    XDP_EXPORT void logFunctionCallEnd(const std::string& name, 
				       double timestamp) ;

    XDP_EXPORT void logMemoryTransfer(uint64_t deviceId, 
				      DeviceMemoryStatistics::ChannelType channelType,
				      size_t byteCount) ;

    // OpenCL level statistic logging
    XDP_EXPORT void logDeviceActiveTime(const std::string& deviceName,
					uint64_t startTime,
					uint64_t endTime) ;
    XDP_EXPORT void logKernelExecution(const std::string& kernelName, 
				       uint64_t executionTime,
				       uint64_t kernelInstanceAddress,
				       uint64_t contextId,
				       uint64_t commandQueueId,
				       const std::string& deviceName,
				       uint64_t startTime,
				       const std::string& globalWorkSize,
				       const std::string& localWorkSize,
				       const char** buffers,
				       uint64_t numBuffers) ;
    XDP_EXPORT void logComputeUnitExecution(const std::string& computeUnitName,
					    const std::string& localWorkGroup,
					    const std::string& globalWorkGroup,
					    uint64_t executionTime) ;
    XDP_EXPORT void logHostRead(uint64_t contextId, uint64_t deviceId,
				uint64_t size, uint64_t startTime,
				uint64_t transferTime,
				uint64_t address,
				uint64_t commandQueueId) ;
    XDP_EXPORT void logHostWrite(uint64_t contextId, uint64_t deviceId,
				 uint64_t size, uint64_t startTime,
				 uint64_t transferTime,
				 uint64_t address,
				 uint64_t commandQueueId) ;

    XDP_EXPORT void updateCounters(uint64_t deviceId, 
				   xclCounterResults& counters) ;
    XDP_EXPORT void updateCounters(xclCounterResults& counters) ;

    // Getters and setters on statistical information
    XDP_EXPORT void setFirstKernelStartTime(double startTime) ;
    inline double getFirstKernelStartTime() { return firstKernelStartTime ; }
    inline void setLastKernelEndTime(double endTime) { lastKernelEndTime = endTime ; }
    inline double getLastKernelEndTime() { return lastKernelEndTime ; }

    // Helper functions for printing out summary information temporarily
    XDP_EXPORT void dumpCallCount(std::ofstream& fout) ;
    XDP_EXPORT void dumpHALMemory(std::ofstream& fout) ;    
  } ;
}

#endif
