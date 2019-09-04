/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

#ifndef _XDP_HAL_PROFILER_H
#define _XDP_HAL_PROFILER_H


#include "xdp/profile/device/device_intf.h"
#include "core/include/experimental/xrt-next.h"

namespace xdp {

    
class HALProfiler
{
  std::map<xclDeviceHandle, DeviceIntf*> devices;
  std::map<std::string, xclCounterResults> mFinalCounterResultsMap;
  std::map<std::string, xclCounterResults> mRolloverCounterResultsMap;
  std::map<std::string, xclCounterResults> mRolloverCountsMap;

// flags  : profile modes 

private:
  HALProfiler() {}

  void calculateAIMRolloverResult(std::string key, unsigned int numAIM, xclCounterResults& counterResult, bool firstReadAfterProgram);
  void calculateAMRolloverResult(std::string key, unsigned int numAM, xclCounterResults& counterResults, bool firstReadAfterProgram);
  void recordAMResult(ProfileResults* results, DeviceIntf* currDevice, std::string key);
  void recordAIMResult(ProfileResults* results, DeviceIntf* currDevice, std::string key);
  void recordASMResult(ProfileResults* results, DeviceIntf* currDevice, std::string key);


public:
  static HALProfiler* Instance();

  ~HALProfiler();

  void startProfiling(xclDeviceHandle);
  void endProfiling();

  void createProfileResults(xclDeviceHandle, void*);
  void getProfileResults(xclDeviceHandle, void*);
  void destroyProfileResults(xclDeviceHandle, void*);

  void startCounters();
  void stopCounters();
  void readCounters();

  void startTrace();
  void stopTrace();
  void readTrace();
  
};

}

#endif
