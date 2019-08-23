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

#include "hal_profiler.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
// xclShim methods header file

namespace xdp {

HALProfiler* HALProfiler::Instance()
{
  // check validity
  static HALProfiler instance;
  return &instance;
}

HALProfiler::~HALProfiler()
{
  // check if already dead ?
  endProfiling();

  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    delete (*itr);
    (*itr) = nullptr;
  }
}

void HALProfiler::startProfiling(xclDeviceHandle handle)
{
  // create the devices
  // find device for handle ; if not found then create and add device
  // for now directly create the device
  DeviceIntf* dev = new DeviceIntf();
  deviceList.push_back(dev);

  dev->setDevice(new xdp::HalDevice(handle));
  dev->readDebugIPlayout();

  // check the flags 
  startCounters();
  startTrace();
}

void HALProfiler::endProfiling()
{
  // check the flags 
  stopCounters();
  stopTrace();

  readCounters();
  readTrace();
}

void HALProfiler::startCounters()
{
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->startCounters((xclPerfMonType)0);
  }
}

void HALProfiler::stopCounters()
{
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->stopCounters((xclPerfMonType)0);
  }
}

void HALProfiler::readCounters()
{
  xclCounterResults counterResults;
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->readCounters((xclPerfMonType)0, counterResults);
  }
}

void HALProfiler::startTrace()
{
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->startTrace((xclPerfMonType)0, 0);
  }
}

void HALProfiler::stopTrace()
{
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->stopTrace((xclPerfMonType)0);
  }
}

void HALProfiler::readTrace()
{
  xclTraceResultsVector traceVector;
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->readTrace((xclPerfMonType)0, traceVector);
  }
}

}
