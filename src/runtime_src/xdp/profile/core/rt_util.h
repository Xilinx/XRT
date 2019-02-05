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

#ifndef __XDP_CORE_RT_UTIL_H
#define __XDP_CORE_RT_UTIL_H

#include "driver/include/xclperf.h"

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <limits>
#include <cstdint>
#include <thread>
#include <queue>

namespace xdp {
  class TimeTrace;

  class RTUtil {
  public:
    //This enum controls the "collection" of data.
    enum e_profile_mode {
      // Keep PROFILE_OFF 0 always
      PROFILE_OFF = 0x0,
      PROFILE_APPLICATION = 0x1 << 1,
      PROFILE_DEVICE_COUNTERS = 0x1 << 2,
      PROFILE_DEVICE_TRACE = 0x1 << 3,
      PROFILE_DEVICE = PROFILE_DEVICE_COUNTERS | PROFILE_DEVICE_TRACE,
      PROFILE_ALL = PROFILE_APPLICATION | PROFILE_DEVICE,
    };

    enum e_profile_command_kind {
      READ_BUFFER = 0x1,
      WRITE_BUFFER = 0x2,
      EXECUTE_KERNEL = 0x3,
      DEVICE_KERNEL_READ = 0x4,
      DEVICE_KERNEL_WRITE = 0x5,
      DEVICE_KERNEL_EXECUTE = 0x6,
      DEVICE_BUFFER_READ = 0x7,
      DEVICE_BUFFER_WRITE = 0x8,
      DEPENDENCY_EVENT = 0x9
    };

    enum e_profile_command_state {
      QUEUE = 0x1,
      SUBMIT = 0x2,
      START = 0x3,
      END = 0x4,
      COMPLETE = 0x5
    };

    enum e_write_file {
      FILE_SUMMARY = 0x1,
      FILE_TIMELINE_TRACE = 0x2
    };

    enum e_device_trace {
      DEVICE_TRACE_OFF = 0x0,
      DEVICE_TRACE_FINE = 0x1,
      DEVICE_TRACE_COARSE = 0x2
    };

    enum e_stall_trace {
      STALL_TRACE_OFF = 0x0,
      STALL_TRACE_EXT = 0x1,
      STALL_TRACE_INT = 0x1 << 1,
      STALL_TRACE_STR = 0x1 << 2,
      STALL_TRACE_ALL = STALL_TRACE_EXT | STALL_TRACE_INT | STALL_TRACE_STR
    };

    enum e_flow_mode {CPU = 0, COSIM_EM, HW_EM, DEVICE};

  public:
    RTUtil();
    ~RTUtil();

  public:
    static void commandKindToString(e_profile_command_kind objKind,
        std::string& commandString);
    static void commandStageToString(e_profile_command_state objStage,
        std::string& stageString);
    static void setTimeStamp(e_profile_command_state objStage, TimeTrace* traceObject,
    	double timeStamp);
    static xclPerfMonEventID getFunctionEventID(const std::string &functionName,
        long long queueAddress);
    static void getFlowModeName(e_flow_mode flowMode, std::string& str);

  };

} // xdp

#endif
