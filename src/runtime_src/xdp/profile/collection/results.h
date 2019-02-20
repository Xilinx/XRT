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

#ifndef __XDP_COLLECTION_RESULTS_H
#define __XDP_COLLECTION_RESULTS_H

#include <limits>
#include <cstdint>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <fstream>
#include <cassert>
#include <thread>
#include <mutex>
#include <CL/opencl.h>

// Use these classes to store results from run time user
// services functions such as debugging and profiling

namespace xdp {
  class ProfileWriterI;

  // Class to record stats on buffer read and writes
  // All sizes are in bytes and times are in ms
  class BufferStats {
  public:
    BufferStats()
      : Count( 0 ),
        Min( (std::numeric_limits<size_t>::max)() ),
        Max( 0 ),
        ContextId( 0 ),
        NumDevices( 1 ),
        BitWidth( 0 ),
        TotalSize( 0.0 ),
        Average( 0.0 ),
        TotalTime( 0.0 ),
        AveTime( 0.0 ),
        AveTransferRate( 0.0 ),
        ClockFreqMhz( 0.0 )
      {};
    ~BufferStats() {};
  public:
    void log(size_t size, double duration);
    void log(size_t size, double duration, uint32_t bitWidth, double clockFreqMhz);
    inline size_t getCount() const { return Count; }
    inline size_t getAverage() const { return static_cast<size_t>(Average); }
    inline size_t getMax() const {return Max; }
    inline size_t getMin() const {return (Count > 0) ? Min : 0; }
    inline uint32_t getContextId() const { return ContextId; }
    inline uint32_t getNumDevices() const { return NumDevices; }
    inline uint32_t getBitWidth() const { return BitWidth; }
    inline uint64_t getTotalSize() const { return TotalSize; }
    inline double getTotalTime() const { return TotalTime; }
    inline double getAveTime() const { return AveTime; }
    inline double getAveTransferRate() const { return AveTransferRate; }
    inline double getAveBWUtil() const {
      // NOTES: MBps = bytes / (1000 * msec)
      //        Ave Util % = 100 * MBps / (max MBps)
      double transferRateMBps = (double)TotalSize / (1000.0 * TotalTime);
      double maxTransferRateMBps = ClockFreqMhz * (BitWidth / 8);
      if (maxTransferRateMBps == 0 || TotalTime == 0)
        return 0.0;
      return ((100.0 * transferRateMBps) / maxTransferRateMBps);
    }
    inline double getClockFreqMhz() const { return ClockFreqMhz; }
    inline std::string getDeviceName() const { return DeviceName; }

    inline void setContextId(uint32_t contextId) { ContextId = contextId; }
    inline void setNumDevices(uint32_t numDevices) { NumDevices = numDevices; }
    inline void setBitWidth(uint32_t bitWidth) { BitWidth = bitWidth; }
    inline void setClockFreqMhz(double clockFreqMhz) { ClockFreqMhz = clockFreqMhz; }
    inline void setDeviceName(std::string& deviceName) { DeviceName = deviceName; }

  private:
    size_t Count;
    size_t Min;
    size_t Max;
    uint32_t ContextId;
    uint32_t NumDevices;
    uint32_t BitWidth;
    uint64_t TotalSize;
    double Average;
    double TotalTime;
    double AveTime;
    // Unit: MB/s
    double AveTransferRate;
    double ClockFreqMhz;
    std::string DeviceName;
  };

  // Class to record stats on time such as time spent in an API call
  // or time spent on kernel execution
  // All stored times are in ms
  class TimeStats {
  public:
    TimeStats()
      : TotalTime( 0 ),
        StartTime( 0 ),
        EndTime( 0 ),
        AveTime( 0 ),
        MaxTime( 0 ),
        MinTime( (std::numeric_limits<double>::max)() ),
        NoOfCalls( 0 ),
        ClockFreqMhz( 300 )
      {};
    ~TimeStats() {};
  public:
    void logStart(double timePoint);
    void logEnd(double timePoint);
    void logStats(double totalTimeStat, double maxTimeStat, 
                  double minTimeStat, uint32_t totalCalls, uint32_t clockFreqMhz);
    inline double getTotalTime() const { return TotalTime; }
    inline double getAveTime() const {return AveTime; }
    inline double getMaxTime() const {return MaxTime; }
    inline double getMinTime() const {return MinTime; }
    inline uint32_t getNoOfCalls() const {return NoOfCalls; }
    inline uint32_t getClockFreqMhz() const { return ClockFreqMhz; }
  private:
    double TotalTime;
    double StartTime;
    double EndTime;
    double AveTime;
    double MaxTime;
    double MinTime;
    uint32_t NoOfCalls;
    uint32_t ClockFreqMhz;
  };

  // Class to store time trace of kernel execution, buffer read, or buffer write
  // Timestamp is in double precision value of unit ms
  class TimeTrace {
  public:
    TimeTrace()
      : ContextId( 0 )
      , CommandQueueId( 0 )
      , Queue( 0.0 )
      , Submit( 0.0 )
      , Start( 0.0 )
      , End( 0.0 )
      , Complete( 0.0 )
      {}
    ~TimeTrace() {};
  public:
    double getDuration() const { return End - Start; }
    uint32_t getContextId() const { return ContextId; }
    uint32_t getCommandQueueId() const { return CommandQueueId; }
    double getQueue() const { return Queue; }
    double getSubmit() const { return Submit; }
    double getStart() const { return Start; }
    double getEnd() const { return End; }
    double getComplete() const { return Complete; }

  protected:
    void resetTimeStamps()
    {
      Queue = 0.0;
      Submit = 0.0;
      Start = 0.0;
      End = 0.0;
      Complete = 0.0;
    }

    virtual void write(ProfileWriterI* writer) const = 0;

  public:
    // Ids
    uint32_t ContextId;
    uint32_t CommandQueueId;

    // Timestamps
    double Queue;
    double Submit;
    double Start;
    double End;
    double Complete;

  };

  class KernelTrace : public TimeTrace {
  public:
    KernelTrace()
      : Next( nullptr ),
        Address( 0 ),
        WorkGroupSize( 0 )
      {};
    ~KernelTrace() {};
  public:
    // Return a recycled object, if no recycled object available, it will
    // allocate a new one
    static KernelTrace* reuse();
    static void recycle(KernelTrace* object);
  public:
    uint64_t getAddress() const {return Address; }
    const std::string& getKernelName() const { return KernelName;}
    const std::string& getDeviceName() const { return DeviceName;}
    size_t getWorkGroupSize() const { return WorkGroupSize; }
    size_t getGlobalWorkSize() const { return GlobalWorkSize[0] * GlobalWorkSize[1] * GlobalWorkSize[2]; }
    size_t getGlobalWorkSizeByIndex(unsigned int i) const { return GlobalWorkSize[i]; }
    size_t getLocalWorkSizeByIndex(unsigned int i) const { return LocalWorkSize[i]; }

  public:
    void write(ProfileWriterI* writer) const override;

  public:
    // KernelTrace* Next = nullptr;
    KernelTrace* Next;
    // Singly linked list to pool and recycle the TimeTrace objects
    static KernelTrace* RecycleHead;

  public:
    uint64_t Address;
    std::string KernelName;
    std::string DeviceName;
    size_t WorkGroupSize;
    size_t GlobalWorkSize[3];
    size_t LocalWorkSize[3];
  };

  class BufferTrace: public TimeTrace {
  public:
    BufferTrace()
      : Next( nullptr ),
        Size( 0 ),
        Address( 0 )
      {};
    ~BufferTrace() {};
  public:
    static BufferTrace* reuse();
    static void recycle(BufferTrace* object);
  public:
    size_t getSize() const { return Size; }
    uint64_t getAddress() const {return Address; }
  public:
    void write(ProfileWriterI* writer) const override;
  public:
    // BufferTrace* Next = nullptr;
    BufferTrace* Next;
    size_t Size;
    uint64_t Address;
    // Singly linked list to pool and recycle the BufferTrace objects
    static BufferTrace* RecycleHead;
  };

  class DeviceTrace: public TimeTrace {
  public:
    DeviceTrace()
      {};
    ~DeviceTrace() {};
  public:
    static DeviceTrace* reuse();
    static void recycle(DeviceTrace* object);
  public:
    size_t getSize() const { return Size; }
  public:
    void write(ProfileWriterI* writer) const override;
  public:
    // DeviceTrace* Next = nullptr;
    DeviceTrace* Next;
    size_t Size;
    // Singly linked list to pool and recycle the DeviceTrace objects
    static DeviceTrace* RecycleHead;

    enum e_device_kind {
      DEVICE_KERNEL = 0x1,
      DEVICE_BUFFER = 0x2,
      DEVICE_STREAM = 0x3
    };

    std::string Name;
    std::string DeviceName;
    std::string Type;
    e_device_kind Kind = DEVICE_KERNEL;
    uint16_t SlotNum = 0;
    uint16_t BurstLength = 0;
    uint16_t NumBytes = 0;
    uint64_t StartTime = 0;
    uint64_t EndTime = 0;
    double TraceStart = 0.0;
  };

} // xdp

#endif
