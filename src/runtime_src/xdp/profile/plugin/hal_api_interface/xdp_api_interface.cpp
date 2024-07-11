/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifdef _WIN32
#pragma warning (disable : 4996 4244)
/* 4996 : Disable warning for use of strcpy */
/* 4244 : Disable warning for conversion from "uint64_t" to "unsigned int" */
#endif

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>

#include "core/include/xdp/common.h"

#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/plugin/hal_api_interface/xdp_api_interface.h"

namespace xdp {

  bool HALAPIInterface::live = false;

  HALAPIInterface::HALAPIInterface() 
  {
    HALAPIInterface::live = true;
  }

  HALAPIInterface::~HALAPIInterface()
  {
    for(auto &itr : devices) {
      delete itr.second;
      itr.second = nullptr;
    }
    devices.clear();

    HALAPIInterface::live = false;
  }

  void HALAPIInterface::startProfiling(xclDeviceHandle handle)
  {
    // create the devices
    // find device for handle ; if not found then create and add device
    // for now directly create the device

    PLDeviceIntf* dev = new PLDeviceIntf();

    auto ret = devices.insert(std::pair<xclDeviceHandle, PLDeviceIntf*>(handle, dev));
    if(false == ret.second) {
      // HAL handle already exists. New xclbin is being loaded on the device.
      // So, reset and clear old DeviceIntf and add the new device
      PLDeviceIntf* oldDeviceIntf = ret.first->second;
      delete oldDeviceIntf;
      devices.erase(handle);
      
      // add new entry for the handle and DeviceIntf
      devices[handle] = dev;
    }
    
    dev->setDevice(std::make_unique<xdp::HalDevice>(handle));
    dev->readDebugIPlayout();
    
    dev->startCounters();
  }

  void HALAPIInterface::startCounters()
  {
    for(const auto& itr : devices) {
      itr.second->startCounters();
    }
  }

  void HALAPIInterface::readCounters()
  {
    xdp::CounterResults counterResults;
    for(const auto& itr : devices) {
      itr.second->readCounters(counterResults);
    }
  }

  void HALAPIInterface::createProfileResults(xclDeviceHandle deviceHandle, 
                                             void* ret)
  {
    ProfileResults** retResults = static_cast<ProfileResults**>(ret);
    
    // create profile result
    ProfileResults* results = (ProfileResults*)calloc(1, sizeof(ProfileResults));
    *retResults = results;
    
    // Initialise profile monitor numbers in ProfileResult and allocate memory
    // Use 1 device now
    PLDeviceIntf* currDevice = nullptr;
    
    try {
      currDevice = devices[deviceHandle];
    } catch (std::exception &) {
      // device not found
      // For now, just return
      return;
    }
    
    // readDebugIPlayout called from startProfiling : check other cases

    std::string deviceName = util::getDeviceName(deviceHandle);
    if (deviceName == "")
    {
      // If we cannot get device information, return an empty profile result
      return ;
    }
    
    results->deviceName = (char*)malloc(deviceName.length()+1);
    strcpy(results->deviceName, deviceName.c_str());
    
    results->numAIM = currDevice->getNumMonitors(xdp::MonitorType::memory);
    results->numAM  = currDevice->getNumMonitors(xdp::MonitorType::accel);
    results->numASM = currDevice->getNumMonitors(xdp::MonitorType::str);
    
    if(results->numAIM) {
      results->kernelTransferData = (KernelTransferData*)calloc(results->numAIM, sizeof(KernelTransferData));
      
      for(unsigned int i=0; i < results->numAIM ; ++i) {
        std::string monName = currDevice->getMonitorName(xdp::MonitorType::memory, i);
        results->kernelTransferData[i].cuPortName = (char*)malloc(monName.length()+1);
        strcpy(results->kernelTransferData[i].cuPortName, monName.c_str());

        // argname
        // memoryname
      }
    }
    
    if(results->numAM) {
      results->cuExecData = (CuExecData*)calloc(results->numAM, sizeof(CuExecData));
      
      for(unsigned int i=0; i < results->numAM ; ++i) {
        std::string monName = currDevice->getMonitorName(xdp::MonitorType::accel, i);
        results->cuExecData[i].cuName = (char*)malloc((monName.length()+1)*sizeof(char));
        strcpy(results->cuExecData[i].cuName, monName.c_str());
        // kernel name
      }
    }      
    
    
    if(results->numASM) {
      results->streamData = (StreamTransferData*)calloc(results->numASM, sizeof(StreamTransferData));
      
      for(unsigned int i=0; i < results->numASM ; ++i) {
        std::string monName = currDevice->getMonitorName(xdp::MonitorType::str, i);
        // Stream monitors have the name structured as "Master-Slave"
        std::size_t sepPos  = monName.find("-");
        if(sepPos == std::string::npos)
          continue;

        std::string masterPort = monName.substr(0, sepPos);
        std::string slavePort  = monName.substr(sepPos + 1);

        results->streamData[i].masterPortName = (char*)malloc((masterPort.length()+1));
        strcpy(results->streamData[i].masterPortName, masterPort.c_str());

        results->streamData[i].slavePortName = (char*)malloc((slavePort.length()+1));
        strcpy(results->streamData[i].slavePortName, slavePort.c_str());
      }
    }
  }
 
  void HALAPIInterface::recordAMResult(ProfileResults* results, PLDeviceIntf* /*currDevice*/, const std::string& key)
  {
    xdp::CounterResults& counterResults = mFinalCounterResultsMap[key];
    
    for(unsigned int i = 0; i < results->numAM ; ++i) {
      
      results->cuExecData[i].cuExecCount = counterResults.CuExecCount[i];
      results->cuExecData[i].cuExecCycles = counterResults.CuExecCycles[i];
      results->cuExecData[i].cuBusyCycles = counterResults.CuBusyCycles[i];
      
      results->cuExecData[i].cuMaxExecCycles = counterResults.CuMaxExecCycles[i];
      results->cuExecData[i].cuMinExecCycles = counterResults.CuMinExecCycles[i];
      results->cuExecData[i].cuMaxParallelIter = counterResults.CuMaxParallelIter[i];
      results->cuExecData[i].cuStallExtCycles = counterResults.CuStallExtCycles[i];
      results->cuExecData[i].cuStallIntCycles = counterResults.CuStallIntCycles[i];
      results->cuExecData[i].cuStallStrCycles = counterResults.CuStallStrCycles[i];
    }
  }
  
  void HALAPIInterface::recordAIMResult(ProfileResults* results, PLDeviceIntf* currDevice, const std::string& key)
  {
    xdp::CounterResults& counterResults = mFinalCounterResultsMap[key];
   
    for(unsigned int i = 0; i < results->numAIM ; ++i) {
      if(currDevice->isHostAIM(i)) {
        continue;
      }
      
      results->kernelTransferData[i].totalReadBytes = counterResults.ReadBytes[i];
      results->kernelTransferData[i].totalReadTranx = counterResults.ReadTranx[i];
      results->kernelTransferData[i].totalReadLatency = counterResults.ReadLatency[i];
      results->kernelTransferData[i].totalReadBusyCycles = counterResults.ReadBusyCycles[i];
      // min max readLatency
      
      results->kernelTransferData[i].totalWriteBytes = counterResults.WriteBytes[i];
      results->kernelTransferData[i].totalWriteTranx = counterResults.WriteTranx[i];
      results->kernelTransferData[i].totalWriteLatency = counterResults.WriteLatency[i];
      results->kernelTransferData[i].totalWriteBusyCycles = counterResults.WriteBusyCycles[i];
      // min max readLatency
    }
  }
  
  void HALAPIInterface::recordASMResult(ProfileResults* results, PLDeviceIntf* /*currDevice*/, const std::string& key)
  {
    xdp::CounterResults& counterResults = mFinalCounterResultsMap[key];
    
    for(unsigned int i = 0; i < results->numASM ; ++i) {
      results->streamData[i].strmNumTranx = counterResults.StrNumTranx[i];
      results->streamData[i].strmBusyCycles = counterResults.StrBusyCycles[i];
      results->streamData[i].strmDataBytes = counterResults.StrDataBytes[i];
      results->streamData[i].strmStallCycles = counterResults.StrStallCycles[i];
      results->streamData[i].strmStarveCycles = counterResults.StrStarveCycles[i];
      
    }
  }
  
  void HALAPIInterface::getProfileResults(xclDeviceHandle deviceHandle, void* res)
  {
    // Step 1: read counters from device
    // Step 2: log the data into counter results data-structure
    // Step 3: populate ProfileResults
    
    // check one device for now
    PLDeviceIntf* currDevice = nullptr;
    
    try {
      currDevice = devices[deviceHandle];
    } catch (std::exception &) {
      // device not found
      // For now, just return
      return;
    }
    
    // Step 1: read counters from device
    
    xdp::CounterResults counterResults;
    currDevice->readCounters(counterResults);  // read from device
    
    ProfileResults* results = static_cast<ProfileResults*>(res);
    // Use 1 device now
    
    // Record the counter data 
    //  auto timeSinceEpoch = (std::chrono::steady_clock::now()).time_since_epoch();
    //  auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch);
    //  uint64_t timeNsec = value.count();
    
    // Create unique name for device since currently all devices are called fpga0
    std::string deviceName(results->deviceName);
    std::string binaryName = "fpga0";
    //  uint32_t program_id = 0;
    
    std::string key = deviceName + "|" + binaryName;
    
    // Step 2: log the data into counter results data-structure
    
    // Log current counter result
    mFinalCounterResultsMap[key] = counterResults;
    
    // record is per device
    
    // log AM into result
    recordAMResult(results, currDevice, key);
    recordAIMResult(results, currDevice, key);
    recordASMResult(results, currDevice, key);
  }
  
  void HALAPIInterface::destroyProfileResults(xclDeviceHandle, void* ret)
  {
    ProfileResults* results = static_cast<ProfileResults*>(ret);
    
    free(results->deviceName);
    results->deviceName = NULL;
    
    // clear AIM data
    for(unsigned int i = 0; i < results->numAIM ; ++i) {
      free(results->kernelTransferData[i].cuPortName);
      free(results->kernelTransferData[i].argName);
      free(results->kernelTransferData[i].memoryName);
      
      results->kernelTransferData[i].cuPortName = NULL;
      results->kernelTransferData[i].argName = NULL;
      results->kernelTransferData[i].memoryName = NULL;
    }
    free(results->kernelTransferData);
    results->kernelTransferData = NULL;
    
    
    // clear AM data
    for(unsigned int i = 0; i < results->numAM ; ++i) {
      free(results->cuExecData[i].cuName);
      free(results->cuExecData[i].kernelName);
      
      results->cuExecData[i].cuName = NULL;
      results->cuExecData[i].kernelName = NULL;
    }
    free(results->cuExecData);
    results->cuExecData = NULL;
    
    // clear ASM data
    for(unsigned int i = 0; i < results->numASM ; ++i) {
      free(results->streamData[i].masterPortName);
      free(results->streamData[i].slavePortName);
      
      results->streamData[i].masterPortName = NULL;
      results->streamData[i].slavePortName = NULL;
    }
    free(results->streamData);
    results->streamData = NULL;
    
    free(results);
  }
}
