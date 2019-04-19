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

#include "rt_util.h"
#include "xdp/profile/collection/results.h"

#include <cassert>

namespace xdp {
  // ******************************
  // XDP Profile Core Utility Class
  // ******************************

  void RTUtil::commandKindToString(e_profile_command_kind objKind,
                                   std::string& commandString)
  {
    switch (objKind) {
    case READ_BUFFER:
      commandString = "READ_BUFFER";
      break;
    case WRITE_BUFFER:
      commandString = "WRITE_BUFFER";
      break;
    case COPY_BUFFER:
      commandString = "COPY_BUFFER";
      break;
    case COPY_BUFFER_P2P:
      commandString = "COPY_BUFFER_P2P";
      break;
    case EXECUTE_KERNEL:
      commandString = "KERNEL";
      break;
    case DEVICE_KERNEL_READ:
      commandString = "KERNEL_READ";
      break;
    case DEVICE_KERNEL_WRITE:
      commandString = "KERNEL_WRITE";
      break;
    case DEVICE_KERNEL_EXECUTE:
      commandString = "KERNEL_EXECUTE";
      break;
    case DEVICE_BUFFER_READ:
      commandString = "READ_BUFFER_DEVICE";
      break;
    case DEVICE_BUFFER_WRITE:
      commandString = "WRITE_BUFFER_DEVICE";
      break;
    case DEPENDENCY_EVENT:
      commandString = "DEPENDENCY_EVENT";
      break;
    default:
      assert(0);
      break;
    }
  }

  void RTUtil::commandStageToString(e_profile_command_state objStage,
                                    std::string& stageString)
  {
    switch (objStage) {
    case QUEUE:
      stageString = "QUEUE";
      break;
    case SUBMIT:
      stageString = "SUBMIT";
      break;
    case START:
      stageString = "START";
      break;
    case END:
      stageString = "END";
      break;
    case COMPLETE:
      stageString = "COMPLETE";
      break;
    default:
      assert(0);
      break;
    }
  }

  // Convert monitor type to string name to detect types
  // NOTE: these strings must match those in VPL
  void RTUtil::monitorTypeToString(e_monitor_type monitorType,
                                   std::string& monitorString)
  {
    switch (monitorType) {
    case MON_HOST_DYNAMIC:
      monitorString = "HOST";
      break;
    case MON_SHELL_KDMA:
      monitorString = "Memory to Memory";
      break;
    case MON_SHELL_XDMA:
      monitorString = "Host to Device";
      break;
    case MON_SHELL_P2P:
      monitorString = "Device to Device";
      break;
    default:
      assert(0);
      break;
    }
  }

  void RTUtil::setTimeStamp(e_profile_command_state objStage,
                            TimeTrace* traceObject, double timeStamp)
  {
    switch (objStage) {
    case QUEUE:
      traceObject->Queue= timeStamp;
      break;
    case SUBMIT:
      traceObject->Submit= timeStamp;
      break;
    case START:
      traceObject->Start= timeStamp;
      break;
    case END:
      traceObject->End= timeStamp;
      break;
    case COMPLETE:
      traceObject->Complete= timeStamp;
      break;
    default:
      assert(0);

      break;
    }
  }

  xclPerfMonEventID
  RTUtil::getFunctionEventID(const std::string &functionName, long long queueAddress)
  {
    // Ignore 'release' functions
    if (functionName.find("Release") != std::string::npos)
      return XCL_PERF_MON_IGNORE_EVENT;

#if 0
    if (queueAddress == 0)
      return XCL_PERF_MON_GENERAL_ID;
    else
      return XCL_PERF_MON_QUEUE_ID;
#endif

    // Get function-specific ID
    // NOTE: similar to list in convertApiState() in tools/sda2wdb/wdbWriter.cxx
    if (functionName.find("clGetPlatformIDs") != std::string::npos)
      return XCL_PERF_MON_API_GET_PLATFORM_ID;
    else if (functionName.find("clGetPlatformInfo") != std::string::npos)
      return XCL_PERF_MON_API_GET_PLATFORM_INFO_ID;
    else if (functionName.find("clGetDeviceIDs") != std::string::npos)
      return XCL_PERF_MON_API_GET_DEVICE_ID;
    else if (functionName.find("clGetDeviceInfo") != std::string::npos)
      return XCL_PERF_MON_API_GET_DEVICE_INFO_ID;
    else if (functionName.find("clBuildProgram") != std::string::npos)
      return XCL_PERF_MON_API_BUILD_PROGRAM_ID;
    else if (functionName.find("clCreateContextFromType") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_CONTEXT_TYPE_ID;
    else if (functionName.find("clCreateContext") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_CONTEXT_ID;
    else if (functionName.find("clCreateCommandQueue") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_COMMAND_QUEUE_ID;
    else if (functionName.find("clCreateProgramWithBinary") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_PROGRAM_BINARY_ID;
    else if (functionName.find("clCreateBuffer") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_BUFFER_ID;
    else if (functionName.find("clCreateImage") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_IMAGE_ID;
    else if (functionName.find("clCreateKernel") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_KERNEL_ID;
    else if (functionName.find("clSetKernelArg") != std::string::npos)
      return XCL_PERF_MON_API_KERNEL_ARG_ID;
    else if (functionName.find("clWaitForEvents") != std::string::npos)
      return XCL_PERF_MON_API_WAIT_FOR_EVENTS_ID;
    else if (functionName.find("clEnqueueReadBuffer") != std::string::npos)
      return XCL_PERF_MON_API_READ_BUFFER_ID;
    else if (functionName.find("clEnqueueWriteBuffer") != std::string::npos)
      return XCL_PERF_MON_API_WRITE_BUFFER_ID;
    else if (functionName.find("clEnqueueReadImage") != std::string::npos)
      return XCL_PERF_MON_API_READ_IMAGE_ID;
    else if (functionName.find("clEnqueueWriteImage") != std::string::npos)
      return XCL_PERF_MON_API_WRITE_IMAGE_ID;
    else if (functionName.find("clEnqueueMigrateMemObjects") != std::string::npos)
      return XCL_PERF_MON_API_MIGRATE_MEM_OBJECTS_ID;
    else if (functionName.find("clEnqueueMigrateMem") != std::string::npos)
      return XCL_PERF_MON_API_MIGRATE_MEM_ID;
    else if (functionName.find("clEnqueueMapBuffer") != std::string::npos)
      return XCL_PERF_MON_API_MAP_BUFFER_ID;
    else if (functionName.find("clEnqueueUnmapMemObject") != std::string::npos)
      return XCL_PERF_MON_API_UNMAP_MEM_OBJECT_ID;
    else if (functionName.find("clEnqueueNDRangeKernel") != std::string::npos)
      return XCL_PERF_MON_API_NDRANGE_KERNEL_ID;
    else if (functionName.find("clEnqueueTask") != std::string::npos)
      return XCL_PERF_MON_API_TASK_ID;

    // Function not in reported list so ignore
    return XCL_PERF_MON_IGNORE_EVENT;
  }

  void RTUtil::getFlowModeName(e_flow_mode flowMode, std::string& str)
  {
    if (flowMode == CPU)
      str = "CPU Emulation";
    else if (flowMode == COSIM_EM)
      str = "Co-Sim Emulation";
    else if (flowMode == HW_EM)
      str = "Hardware Emulation";
    else
      str = "System Run";
  }

  // Same as defined in vpl tcl
  uint32_t RTUtil::getDevTraceBufferSize(uint32_t property)
  {
    switch(property) {
      case 0 : return 8192;
      case 1 : return 1024;
      case 2 : return 2048;
      case 3 : return 4096;
      case 4 : return 16384;
      case 5 : return 32768;
      case 6 : return 65536;
      case 7 : return 131072;
      default : break;
    }
    return 8192;
  }

} // xdp
