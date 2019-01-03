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

#include "base_plugin.h"
#include "base_profile.h"
#include "xdp/profile/core/rt_profile.h"
#include "xdp/rt_singleton.h"

#include <chrono>

namespace xdp {
  //****************
  // Base XDP Plugin
  //****************
  XDPPluginI::XDPPluginI()
  {
    mComputeUnitKernelTraceMap.clear();
  }
  
  void XDPPluginI::~XDPPluginI()
  {
    mDeviceExecTimesMap.clear();
    mComputeUnitCallsMap.clear();
    mKernelCountsMap.clear();
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
  void XDPPluginI::setArgumentsBank(const std::string& deviceName)
  {
    // do nothing
  }

  // Get the arguments and memory resource for a given device/CU/port
  void XDPPluginI::getArgumentsBank(const std::string& deviceName, const std::string& cuName,
   	                                const std::string& portName, std::string& argNames,
   				                    std::string& memoryName)
  {
    argNames = "All";
    memoryName = "DDR";
  }

  void XDPPluginI::getGuidanceMetadata(RTProfile *profile)
  {
    // do nothing
  }

  // Write out param/value map
  // NOTE: don't clear after this since it can be called with multiple writers
  void XDPPluginI::writeGuidanceMetadataSummary(ProfileWriterI* writer, RTProfile *profile)
  {
    writer->writeGuidanceMetadataSummary(profile, mDeviceExecTimesMap, mComputeUnitCallsMap, mKernelCountsMap);
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
      case MEMORY_BIT_WIDTH:
        name = "MEMORY_BIT_WIDTH";
        break;
      case MIGRATE_MEM:
        name = "MIGRATE_MEM";
        break;
      case DDR_BANKS:
        name = "DDR_BANKS";
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
      default:
        assert(0);
        break;
    }
  }

} // xdp
