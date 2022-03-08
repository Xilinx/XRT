/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#ifndef XDP_API_INTERFACE_DOT_H
#define XDP_API_INTERFACE_DOT_H

#include <string>
#include <map>
#include <vector>

#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/database/database.h"
#include "core/include/experimental/xrt-next.h"

namespace xdp {

  class HALAPIInterface
  {
  private:
    std::map<xclDeviceHandle, DeviceIntf*> devices;
    std::map<std::string, xclCounterResults> mFinalCounterResultsMap;

    static bool live;

  private:
    void recordAMResult(ProfileResults* results, 
			DeviceIntf* currDevice, 
			const std::string& key);
    void recordAIMResult(ProfileResults* results, 
			 DeviceIntf* currDevice, 
			 const std::string& key);
    void recordASMResult(ProfileResults* results, 
			 DeviceIntf* currDevice, 
			 const std::string& key);

  public:
     HALAPIInterface() ;
     ~HALAPIInterface();

     void startProfiling(xclDeviceHandle);

     void createProfileResults(xclDeviceHandle, void*);
     void getProfileResults(xclDeviceHandle, void*);
     void destroyProfileResults(xclDeviceHandle, void*);
     
     void startCounters();
     void readCounters();

     static bool alive() { return HALAPIInterface::live; }
  } ;

}

#endif

