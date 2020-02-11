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

#include <vector>
#include <thread>

#define XDP_SOURCE

#include "xdp/profile/database/statistics_database.h"

namespace xdp {

  VPStatisticsDatabase::VPStatisticsDatabase() : 
    firstKernelStartTime(0.0)
  {
  }

  VPStatisticsDatabase::~VPStatisticsDatabase()
  {
  }

  void VPStatisticsDatabase::logFunctionCallStart(const std::string& name,
						   double timestamp)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    auto threadId = std::this_thread::get_id() ;
    auto key      = std::make_pair(name, threadId) ;
    auto value    = std::make_pair(timestamp, (double)0.0) ;

    if (callCount.find(key) == callCount.end())
    {
      std::vector<std::pair<double, double>> newVector ;
      newVector.push_back(value) ;
      callCount[key] = newVector ;
    }
    else
    {
      callCount[key].push_back(value) ;
    }
  }

  void VPStatisticsDatabase::logFunctionCallEnd(const std::string& name,
						 double timestamp)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    auto threadId = std::this_thread::get_id() ;
    auto key      = std::make_pair(name, threadId) ;
    
    callCount[key].back().second = timestamp ;
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

  void VPStatisticsDatabase::logKernelExecution(const std::string& kernelName,
						 double executionTime)
  {
    if (kernelExecutionStats.find(kernelName) == kernelExecutionStats.end())
    {
      TimeStatistics blank ;
      kernelExecutionStats[kernelName] = blank ;
    }
    (kernelExecutionStats[kernelName]).update(executionTime) ;
  }

  void VPStatisticsDatabase::logComputeUnitExecution(const std::string& /*computeUnitName*/, 
						      double /*executionTime*/)
  {
  }

  void VPStatisticsDatabase::updateCounters(uint64_t /*deviceId*/,
					     xclCounterResults& /*counters*/)
  {
  }

  void VPStatisticsDatabase::updateCounters(xclCounterResults& /*counters*/)
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

    for (auto c : callCount)
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

    for (auto i : counts)
    {
      fout << i.first << "," << i.second << std::endl ;
    }
  }

  void VPStatisticsDatabase::dumpHALMemory(std::ofstream& fout)
  {
    unsigned int i = 0 ; 
    for (auto m : memoryStats)
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
