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
#include "rt_profile_rule_checks.h"
#include "rt_profile_writers.h"
#include "rt_profile.h"
#include "xdp/rt_singleton.h"

#include "xocl/core/device.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace XCL {
  // Gather statistics and put into param/value map
  void ProfileRuleChecks::getProfileRuleCheckSummary(RTProfile *profile)
  {
    // 1. Device execution times (and unused devices)
    getDeviceExecutionTimes(profile);

    // 2. Unused CUs
    getUnusedComputeUnits(profile);

    // 3. Kernel counts
    getKernelCounts(profile);
  }

  // Write out param/value map
  // NOTE: don't clear here since it's called twice (original & unified CSV writers)
  void ProfileRuleChecks::writeProfileRuleCheckSummary(WriterI* writer, RTProfile *profile)
  {
    writer->writeProfileRuleCheckSummary(profile, mDeviceExecTimesMap, mComputeUnitCallsMap, mKernelCountsMap);
    //clear();
  }

  void ProfileRuleChecks::clear()
  {
    mDeviceExecTimesMap.clear();
    mComputeUnitCallsMap.clear();
    mKernelCountsMap.clear();
  }

  void ProfileRuleChecks::getRuleCheckName(e_rule_check check, std::string& name)
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
      default:
        assert(0);
        break;
    }
  }

  void ProfileRuleChecks::getDeviceExecutionTimes(RTProfile *profile)
  {
    auto platform = XCL::RTSingleton::Instance()->getcl_platform_id();

    // Traverse all devices in this platform
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      if (!profile->isDeviceActive(deviceName))
        continue;

      // Get execution time for this device
      // NOTE: if unused, then this returns 0.0
      double deviceExecTime = profile->getTotalKernelExecutionTime(deviceName);
      mDeviceExecTimesMap[deviceName] = std::to_string(deviceExecTime);
    }
  }

  void ProfileRuleChecks::getUnusedComputeUnits(RTProfile *profile)
  {
    auto platform = XCL::RTSingleton::Instance()->getcl_platform_id();

    // Traverse all devices in this platform
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      if (!profile->isDeviceActive(deviceName))
        continue;

      // Traverse all CUs on current device
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto cuName = cu->get_name();

        // Get number of calls for current CU
        int numCalls = profile->getComputeUnitCalls(deviceName, cuName);
        std::string cuFullName = deviceName + "|" + cuName;
        mComputeUnitCallsMap[cuFullName] = std::to_string(numCalls);
      }
    }
  }

  void ProfileRuleChecks::getKernelCounts(RTProfile *profile)
  {
    auto platform = XCL::RTSingleton::Instance()->getcl_platform_id();

    // Traverse all devices in this platform
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      if (!profile->isDeviceActive(deviceName))
        continue;

      // Traverse all CUs on current device
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto kernelName = cu->get_kernel_name();

        if (mKernelCountsMap.find(kernelName) == mKernelCountsMap.end())
          mKernelCountsMap[kernelName] = 1;
        else
          mKernelCountsMap[kernelName] += 1;
      }
    }
  }
}


