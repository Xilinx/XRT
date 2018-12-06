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

// Copyright 2014 Xilinx, Inc. All rights reserved.
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <time.h>

#include "xdp_profile_results.h"
// #include <unistd.h>
#include "xdp_profile_writers.h"
/*
#include "rt_profile_device.h"
#include "../../driver/include/xclperf.h"
*/

XDP::KernelTrace* XDP::KernelTrace::RecycleHead = nullptr;
XDP::BufferTrace* XDP::BufferTrace::RecycleHead = nullptr;
XDP::DeviceTrace* XDP::DeviceTrace::RecycleHead = nullptr;

namespace XDP {

  //
  // BufferStats
  //

  void BufferStats::log (size_t size, double duration) {
    Average = (Average * Count + size ) / (Count + 1);
    AveTime = (AveTime * Count + duration) / (Count + 1);
    TotalSize += size;
    TotalTime += duration;

    // size is in bytes, divide that 1000 to get KB and then divide
    // by ms duration to get MB/s
    double transferRate = (size / (1000.0 * duration));
    AveTransferRate = (AveTransferRate * Count + transferRate) / (Count + 1);
    Count++;
    if (Max < size)
      Max = size;
    if(Min > size)
      Min = size;
  };

  void BufferStats::log(size_t size, double duration, uint32_t bitWidth, double clockFreqMhz) {
    BitWidth = bitWidth;
    ClockFreqMhz = clockFreqMhz;
    log(size, duration);
  }

  //
  // TimeStats
  //

  void TimeStats::logStart (double timePoint) {
    StartTime = timePoint;
    EndTime = 0;
  };

  void TimeStats::logEnd(double timePoint)
  {
    EndTime = timePoint;
    double time = EndTime - StartTime;
    TotalTime += time;
    AveTime = (AveTime * NoOfCalls + time) / (NoOfCalls + 1);
    NoOfCalls++;
    if (MaxTime < time)
      MaxTime = time;
    if (MinTime > time)
      MinTime = time;
  }

  void TimeStats::logStats(double totalTimeStat, double maxTimeStat, 
                  double minTimeStat, uint32_t totalCalls, uint32_t clockFreqMhz)
  {
    StartTime = 0;
    EndTime = totalTimeStat;
    TotalTime = totalTimeStat;
    AveTime = totalTimeStat / totalCalls;
    if (MaxTime < maxTimeStat)
      MaxTime = maxTimeStat;
    if (MinTime > minTimeStat || MinTime == 0)
      MinTime = minTimeStat;
    NoOfCalls = totalCalls;
    ClockFreqMhz = clockFreqMhz;
  }

  //
  // Kernel Trace
  //
  // Return a recycled object, if no recycled object available, it will
  // allocate a new one
  KernelTrace* KernelTrace::reuse()
  {
    if (RecycleHead) {
      KernelTrace* element = RecycleHead;
      RecycleHead = RecycleHead->Next;
      return element;
    }
    else {
      return new KernelTrace();
    }
  }

  // Return back the object to be reused later
  void KernelTrace::recycle(KernelTrace* object)
  {
    object->resetTimeStamps();
    object->Next = RecycleHead ? RecycleHead : nullptr;
    RecycleHead = object;
  }

  void KernelTrace::write(WriterI* writer) const
  {
    writer->writeSummary(*this);
  }

  //
  // Buffer Trace
  //
  BufferTrace* BufferTrace::reuse()
  {
    if (RecycleHead) {
      BufferTrace* element = RecycleHead;
      RecycleHead = RecycleHead->Next;
      return element;
    }
    else {
      return new BufferTrace();
    }
  }

  // Return back the object to be reused later
  void BufferTrace::recycle(BufferTrace* object)
  {
    object->resetTimeStamps();
    object->Next = RecycleHead ? RecycleHead : nullptr;
    RecycleHead = object;
  }


  void BufferTrace::write(WriterI* writer) const
  {
    writer->writeSummary(*this);
  }

  //
  // Device Trace
  //
  DeviceTrace* DeviceTrace::reuse()
  {
    if (RecycleHead) {
      DeviceTrace* element = RecycleHead;
      RecycleHead = RecycleHead->Next;
      return element;
    }
    else {
      return new DeviceTrace();
    }
  }

  // Return back the object to be reused later
  void DeviceTrace::recycle(DeviceTrace* object)
  {
    object->resetTimeStamps();
    object->Next = RecycleHead ? RecycleHead : nullptr;
    RecycleHead = object;
  }

  void DeviceTrace::write(WriterI* writer) const
  {
    writer->writeSummary(*this);
  }

}


