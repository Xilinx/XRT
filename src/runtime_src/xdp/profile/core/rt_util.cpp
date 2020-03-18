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
    case READ_BUFFER_P2P:
      commandString = "READ_BUFFER_P2P";
      break;
    case WRITE_BUFFER:
      commandString = "WRITE_BUFFER";
      break;
    case WRITE_BUFFER_P2P:
      commandString = "WRITE_BUFFER_P2P";
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
      monitorString = "Peer to Peer";
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

  void RTUtil::getFlowModeName(e_flow_mode flowMode, std::string& str)
  {
    if (flowMode == CPU)
      str = "Software Emulation";
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
