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

// For the device results structures
#include "xclperf.h"

#include "xdp/config.h"

namespace xdp {

  // All of the statistics in this database will be used in 
  //  summary files.  Different plugins might use different
  //  information.  This information accumulates throughout 
  //  host code execution and should not be reset when
  //  information is dumped in continuous offload.

  struct TimeStatistics
  {
    double totalTime ;
    double averageTime ;
    double maxTime ;
    double minTime ;
    uint32_t numExecutions ;

    TimeStatistics() : totalTime(0), averageTime(0), maxTime(0), 
      minTime((std::numeric_limits<double>::max)()), numExecutions(0) { }
    void update(double executionTime)
    {
      totalTime += executionTime ;
      averageTime = ((averageTime * numExecutions) + executionTime)/numExecutions ;
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
    // Statistics on API calls (OpenCL and HAL) have to be thread specific
    std::map<std::pair<std::string, std::thread::id>,
             std::vector<std::pair<double, double>>> callCount ;

    // For HAL, each device will have four different read/write
    //  channels that need to be kept track of.
    std::map<uint64_t, DeviceMemoryStatistics> memoryStats ;

    // Statistics on kernel enqueues and executions
    std::map<std::string, TimeStatistics> kernelExecutionStats ;
    
    // Statistics on compute unit enqueues and executions
    std::map<std::string, TimeStatistics> computeUnitExecutionStats ;

    // Information used by trace parser
    double firstKernelStartTime ;

    // Since the host code can be multithreaded, we must protect 
    //  the data
    std::mutex dbLock ;

  public:
    XDP_EXPORT VPStatisticsDatabase() ;
    XDP_EXPORT ~VPStatisticsDatabase() ;

    // Getters and setters
    inline const std::map<std::pair<std::string, std::thread::id>,
                    std::vector<std::pair<double, double>>>& getCallCount() 
      { return callCount ; }
    inline const std::map<uint64_t, DeviceMemoryStatistics>& getMemoryStats() 
      { return memoryStats ; }
    inline const std::map<std::string, TimeStatistics>& getKernelExecutionStats() 
      { return kernelExecutionStats ; }
    inline const std::map<std::string, TimeStatistics>& getComputeUnitExecutionStats() 
      { return computeUnitExecutionStats ; }

    // Logging Functions
    XDP_EXPORT void logFunctionCallStart(const std::string& name, 
					 double timestamp) ;
    XDP_EXPORT void logFunctionCallEnd(const std::string& name, 
				       double timestamp) ;

    XDP_EXPORT void logMemoryTransfer(uint64_t deviceId, 
				      DeviceMemoryStatistics::ChannelType channelType,
				      size_t byteCount) ;

    XDP_EXPORT void logKernelExecution(const std::string& kernelName, 
				       double executionTime) ;
    XDP_EXPORT void logComputeUnitExecution(const std::string& computeUnitName,
					    double executionTime) ;

    XDP_EXPORT void updateCounters(uint64_t deviceId, 
				   xclCounterResults& counters) ;
    XDP_EXPORT void updateCounters(xclCounterResults& counters) ;

    // Getters and setters on statistical information
    XDP_EXPORT void setFirstKernelStartTime(double startTime) ;

    // Helper functions for printing out summary information temporarily
    XDP_EXPORT void dumpCallCount(std::ofstream& fout) ;
    XDP_EXPORT void dumpHALMemory(std::ofstream& fout) ;    
  } ;
}

#endif
