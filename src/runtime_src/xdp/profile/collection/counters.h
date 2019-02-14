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

#ifndef __XDP_COLLECTION_COUNTERS_H
#define __XDP_COLLECTION_COUNTERS_H

#include "results.h"
#include <limits>
#include <cstdint>
#include <map>
#include <list>
#include <string>

//#define BUFFER_STAT_PER_CONTEXT 1

// Use this class to build run time user services functions
// such as debugging and profiling

namespace xdp {
  class ProfileWriterI;
  class TraceWriterI;

  // Sorted list that keeps top 10 most time taking (end - start) Kernel/Buffer Trace
  // A simple singly linked list where a linear search is done to ensure sorted
  // list. This is expected to have at max 10 elements
  template <typename T>
  class TimeTraceSortedTopUsage {

  public:
    TimeTraceSortedTopUsage()
      : Limit(10) {};
    ~TimeTraceSortedTopUsage() {};

    void push(T* newElement);
    void writeTopUsageSummary(ProfileWriterI* writer) const;

  private:
    size_t Limit;   // Maximum numbers of elements allowed
    std::list<T*> Storage;
  };

  // Performance counters
  class ProfileCounters {
  public:
    ProfileCounters();
    ~ProfileCounters() {};
  public:
    void setAllDeviceBufferBitWidth(uint32_t bitWidth);
    void setAllDeviceKernelBitWidth(uint32_t bitWidth);
    void setAllDeviceClockFreqMhz(double clockFreqMhz);

    // Functions required by guidance
    double getDeviceStartTime(const std::string& deviceName) const;
    double getTotalKernelExecutionTime(const std::string& deviceName) const;
    uint32_t getComputeUnitCalls(const std::string& deviceName, const std::string& cuName) const;
    double getComputeUnitTotalTime(const std::string& deviceName, const std::string& cuName) const;

  public:
    void logBufferRead(size_t size, double duration, uint32_t contextId, uint32_t numDevices);
    void logBufferWrite(size_t size, double duration, uint32_t contextId, uint32_t numDevices);
    void logDeviceRead(size_t size, double duration);
    void logDeviceWrite(size_t size, double duration);
    void logDeviceKernel(size_t size, double duration);
    void logDeviceKernelTransfer(std::string& deviceName, std::string& kernelName, size_t size, double duration,
                                 uint32_t bitWidth, double clockFreqMhz, bool isRead);
    void logFunctionCallStart(const std::string& functionName, double timePoint);
    void logFunctionCallEnd(const std::string& functionName, double timePoint);
    void logKernelExecutionStart(const std::string& kernelName, const std::string& deviceName, double timePoint);
    void logKernelExecutionEnd(const std::string& kernelName, const std::string& deviceName, double timePoint);
    void logComputeUnitDeviceStart(const std::string& deviceName, double timePoint);
    void logComputeUnitExecutionStart(const std::string& cuName, double timePoint);
    void logComputeUnitExecutionEnd(const std::string& cuName, double timePoint);
    void logComputeUnitStats(const std::string& cuName, const std::string& kernelName, double totalTimeStat, 
                              double maxTimeStat, double minTimeStat, uint32_t totalCalls, uint32_t clockFreqMhz);
    void logDeviceEvent(std::string deviceName, std::string kernelName, size_t size,
                        double duration, uint32_t bitWidth, double clockFreqMhz,
                        bool isKernel, bool isRead, bool isKernelTransfer);

  public:
    void pushToSortedTopUsage(KernelTrace* trace);
    void pushToSortedTopUsage(BufferTrace* trace, bool isRead);
    void pushToSortedTopUsage(DeviceTrace* trace, bool isRead, bool isKernel);

    // Profile summary writers
    void writeAPISummary(ProfileWriterI* writer) const;
    void writeKernelSummary(ProfileWriterI* writer) const;
    void writeComputeUnitSummary(ProfileWriterI* writer) const;
    void writeTopKernelTransferSummary(
        ProfileWriterI* writer, std::string &deviceName, std::string &cuName,
        uint64_t totalWriteBytes, uint64_t totalReadBytes,
        uint64_t totalWriteTranx, uint64_t totalReadTranx,
        double totalWriteTimeMsec, double totalReadTimeMsec,
        uint32_t maxBytesPerTransfer, double maxTransferRateMBps) const;
    void writeHostTransferSummary(ProfileWriterI* writer, bool isRead, uint64_t totalBytes,
        double totalTimeMsec, double maxTransferRateMBps) const;
    void writeKernelTransferSummary(ProfileWriterI* writer, std::string& deviceName,
    	std::string& cuPortName, const std::string& argNames, const std::string& memoryName,
    	bool isRead, uint64_t totalBytes, uint64_t totalTranx, double totalKernelTimeMsec,
        double totalTransferTimeMsec, double maxTransferRateMBps) const;
    void writeDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const;

    void writeAcceleratorSummary(ProfileWriterI* writer) const;
    void writeTopHardwareSummary(ProfileWriterI* writer) const;
    void writeTopKernelSummary(ProfileWriterI* writer) const;
    void writeTopDataTransferSummary(ProfileWriterI* writer, bool isRead) const;
    void writeTopDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const;

  private:
	void writeBufferStat(ProfileWriterI* writer, const std::string transferType,
	    const BufferStats &bufferStat, double maxTransferRateMBps) const;

  private:
    BufferStats DeviceBufferReadStat;
    BufferStats DeviceBufferWriteStat;
    BufferStats DeviceKernelStat;
#ifdef BUFFER_STAT_PER_CONTEXT
    std::map<uint32_t, BufferStats> BufferReadStat;
    std::map<uint32_t, BufferStats> BufferWriteStat;
#else
    BufferStats BufferReadStat;
    BufferStats BufferWriteStat;
#endif
    std::map<std::string, double> DeviceCUStartTimes;
    std::map<std::string, double> DeviceStartTimes;
    std::map<std::string, double> DeviceEndTimes;
    std::map<std::string, TimeStats> CallCount;
    std::map<std::string, TimeStats> KernelExecutionStats;
    std::map<std::string, TimeStats> ComputeUnitExecutionStats;
    std::map<std::string, BufferStats> DeviceKernelReadSummaryStats;
    std::map<std::string, BufferStats> DeviceKernelWriteSummaryStats;
    TimeTraceSortedTopUsage<KernelTrace> TopKernelTimes;
    TimeTraceSortedTopUsage<BufferTrace> TopBufferReadTimes;
    TimeTraceSortedTopUsage<BufferTrace> TopBufferWriteTimes;
    TimeTraceSortedTopUsage<DeviceTrace> TopKernelReadTimes;
    TimeTraceSortedTopUsage<DeviceTrace> TopKernelWriteTimes;
    TimeTraceSortedTopUsage<DeviceTrace> TopDeviceBufferReadTimes;
    TimeTraceSortedTopUsage<DeviceTrace> TopDeviceBufferWriteTimes;
  };

} // xdp

#endif
