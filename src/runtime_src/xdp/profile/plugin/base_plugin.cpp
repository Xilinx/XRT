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

#define XDP_SOURCE

#include "base_plugin.h"

namespace xdp {
  //****************
  // Base XDP Plugin
  //****************
  XDPPluginI::XDPPluginI()
  {
    mComputeUnitKernelTraceMap.clear();
  }

  XDPPluginI::~XDPPluginI()
  {
  }

  // **********
  // Trace time
  // **********
  double XDPPluginI::getTraceTime() {
    using namespace std::chrono;
    typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
    duration_ns time_span =
        duration_cast<duration_ns>(high_resolution_clock::now().time_since_epoch());
    uint64_t timeNsec = time_span.count();
    return getTimestampMsec(timeNsec);
  }

  // *************************
  // Accelerator port metadata
  // *************************
  void XDPPluginI::setArgumentsBank(const std::string& /*deviceName*/)
  {
    // do nothing
  }

  // Get the arguments and memory resource for a given device/CU/port
  void XDPPluginI::getArgumentsBank(const std::string& /*deviceName*/, const std::string& /*cuName*/,
   	                                const std::string& /*portName*/, std::string& argNames,
   				                          std::string& memoryName)
  {
    argNames = "All";
    memoryName = "DDR";
  }

  void XDPPluginI::getGuidanceMetadata(RTProfile* /*profile*/)
  {
    // do nothing
  }

  void XDPPluginI::logBufferEvent(double timestamp, bool isRead, bool isStart)
  {
    // Total Active time = Last buffer event - First buffer event
    if (mActiveTimeStartMs == 0.0)
      mActiveTimeStartMs = timestamp;
    mActiveTimeEndMs = timestamp;
    if (isRead) {
      // Total Read time = Sum(Read Activity Intervals)
      mReadTimeStartMs = isStart ? timestamp : mReadTimeStartMs;
      mReadTimeMs += (timestamp - mReadTimeStartMs);
    } else {
      // Total Write time = Sum(Write Activity Intervals)
      mWriteTimeStartMs = isStart ? timestamp : mWriteTimeStartMs;
      mWriteTimeMs += (timestamp - mWriteTimeStartMs);
    }
  }

  // Total Active time =  Last buffer event - First buffer event
  double XDPPluginI::getBufferActiveTimeMs()
  {
    return mActiveTimeEndMs - mActiveTimeStartMs;
  }

  // Get name string of guidance
  void XDPPluginI::getGuidanceName(e_guidance check, std::string& name)
  {
    switch (check) {
      case DEVICE_EXEC_TIME:
        name = "DEVICE_EXEC_TIME";
        break;
      case CU_CALLS:
        name = "CU_CALLS";
        break;
      case MIGRATE_MEM:
        name = "MIGRATE_MEM";
        break;
      case MEMORY_USAGE:
        name = "MEMORY_USAGE";
        break;
      case PLRAM_DEVICE:
        name = "PLRAM_DEVICE";
        break;
      case HBM_DEVICE:
        name = "HBM_DEVICE";
        break;
      case KDMA_DEVICE:
        name = "KDMA_DEVICE";
        break;
      case P2P_DEVICE:
        name = "P2P_DEVICE";
        break;
      case P2P_HOST_TRANSFERS:
        name = "P2P_HOST_TRANSFERS";
        break;
      case PORT_BIT_WIDTH:
        name = "PORT_BIT_WIDTH";
        break;
      case KERNEL_COUNT:
        name = "KERNEL_COUNT";
        break;
      case OBJECTS_RELEASED:
        name = "OBJECTS_RELEASED";
        break;
      case CU_CONTEXT_EN:
        name = "CU_CONTEXT_EN";
        break;
      case TRACE_MEMORY:
        name = "TRACE_MEMORY";
        break;
      case MAX_PARALLEL_KERNEL_ENQUEUES:
        name = "MAX_PARALLEL_KERNEL_ENQUEUES";
        break;
      case COMMAND_QUEUE_OOO:
        name = "COMMAND_QUEUE_OOO";
        break;
      case PLRAM_SIZE_BYTES:
        name = "PLRAM_SIZE_BYTES";
        break;
      case KERNEL_BUFFER_INFO:
        name = "KERNEL_BUFFER_INFO";
        break;
      case TRACE_BUFFER_FULL:
        name = "TRACE_BUFFER_FULL";
        break;
      case MEMORY_TYPE_BIT_WIDTH:
        name = "MEMORY_TYPE_BIT_WIDTH";
        break;
      case XRT_INI_SETTING:
        name = "XRT_INI_SETTING";
        break;
      case BUFFER_RD_ACTIVE_TIME_MS:
        name = "BUFFER_RD_ACTIVE_TIME_MS";
        break;
      case BUFFER_WR_ACTIVE_TIME_MS:
        name = "BUFFER_WR_ACTIVE_TIME_MS";
        break;
      case BUFFER_TX_ACTIVE_TIME_MS:
        name = "BUFFER_TX_ACTIVE_TIME_MS";
        break;
      case APPLICATION_RUN_TIME_MS:
        name = "APPLICATION_RUN_TIME_MS";
        break;
      case TOTAL_KERNEL_RUN_TIME_MS:
        name = "TOTAL_KERNEL_RUN_TIME_MS";
        break;
      default:
        assert(0);
        break;
    }
  }

  void XDPPluginI::sendMessage(const std::string &msg)
  {
    std::cout << msg;
  }

} // xdp
