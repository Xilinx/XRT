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

// Copyright 2016 Xilinx, Inc. All rights reserved.
#ifndef __XILINX_RT_PROFILE_RULE_CHECKS_H
#define __XILINX_RT_PROFILE_RULE_CHECKS_H

#include <map>
#include <string>
#include <fstream>
#include <CL/opencl.h>

// Use this class to gather statistics for profile rule checks

namespace XCL {
  class WriterI;
  class RTProfile;

  // Profile rule checks
  class ProfileRuleChecks {
  public:
    ProfileRuleChecks() {};
    ~ProfileRuleChecks() {clear();};

  public:
    typedef std::map<std::string, std::string> ProfileRuleCheckMap;
    typedef std::map<std::string, uint32_t> ProfileRuleCheckMap2;

    enum e_rule_check {
      DEVICE_EXEC_TIME,
      CU_CALLS,
      MEMORY_BIT_WIDTH,
      MIGRATE_MEM,
      DDR_BANKS,
      PORT_BIT_WIDTH,
      KERNEL_COUNT
    };

  public:
    void getProfileRuleCheckSummary(RTProfile *profile);
    void writeProfileRuleCheckSummary(WriterI* writer, RTProfile *profile);
    static void getRuleCheckName(e_rule_check check, std::string& name);

  private:
    void clear();
    void getDeviceExecutionTimes(RTProfile *profile);
    void getUnusedComputeUnits(RTProfile *profile);
    void getKernelCounts(RTProfile *profile);

  private:
    ProfileRuleCheckMap  mDeviceExecTimesMap;
    ProfileRuleCheckMap  mComputeUnitCallsMap;
    ProfileRuleCheckMap2 mKernelCountsMap;
  };

};
#endif


