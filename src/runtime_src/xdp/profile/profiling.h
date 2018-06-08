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

/**
 * COPYRIGHT NOTICE
 * Copyright 2016 Xilinx Inc. All Rights Reserved.
 *
 * Author : Paul Schumacher (paul.schumacher@xilinx.com)
 *
 * SDAccel profiling top level
 **/
#ifndef __XILINX_RT_PROFILING_H
#define __XILINX_RT_PROFILING_H

#include <CL/opencl.h>
#include <string>

// Use Profiling::Profiler::Instance() to get to the singleton runtime object
// Runtime code base can access the singleton and make decisions based on the
// contents of the singleton

namespace Profiling {

  bool active();

  class Profiler {
  public:
    static Profiler* Instance();
    static bool InstanceExists() {
      return (mRTInstance != nullptr);
    }

  public:
    Profiler();
    ~Profiler();

  public:
    void startDeviceProfiling(size_t numComputeUnits);
    void endDeviceProfiling();
    void getDeviceCounters(bool firstReadAfterProgram, bool forceReadCounters);
    void getDeviceTrace(bool forceReadTrace);
    void resetDeviceProfilingFlag() {mEndDeviceProfilingCalled = false;}

  public:
    bool isProfileRunning() {return mProfileRunning;}

  private:
    uint32_t getTimeDiffUsec(std::chrono::steady_clock::time_point start,
                             std::chrono::steady_clock::time_point end);

  private:
    bool mProfileRunning = false;
    bool mEndDeviceProfilingCalled = false;
    static Profiler* mRTInstance;

  };
};

#endif


