/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include "xdp/profile/plugin/hal_api_interface/xdp_api_interface.h"

#include <cassert>
#include <iostream>
#include <cstring>
#include <chrono>
#include "xdp/profile/device/hal_device/xdp_hal_device.h"

namespace xdp {

  HALAPIInterface::HALAPIInterface() 
  {
  }

  HALAPIInterface::~HALAPIInterface()
  {
    endProfiling();
    
    for(auto &itr : devices) {
      delete itr.second;
      itr.second = nullptr;
    }
    devices.clear();  
  }

  void HALAPIInterface::startProfiling(xclDeviceHandle handle)
  {
    // create the devices
    // find device for handle ; if not found then create and add device
    // for now directly create the device

    DeviceIntf* dev = new DeviceIntf();

    auto ret = devices.insert(std::pair<xclDeviceHandle, DeviceIntf*>(handle, dev));
    if(false == ret.second) {
      // HAL handle already exists. New xclbin is being loaded on the device.
      // So, reset and clear old DeviceIntf and add the new device
      DeviceIntf* oldDeviceIntf = ret.first->second;
      delete oldDeviceIntf;
      devices.erase(handle);
      
      // add new entry for the handle and DeviceIntf
      devices[handle] = dev;
    }
    
    dev->setDevice(new xdp::HalDevice(handle));
    dev->readDebugIPlayout();
    
    dev->startCounters();
}

  void HALAPIInterface::endProfiling()
  {
    stopCounters();
  }

  void HALAPIInterface::startCounters()
  {
    for(auto itr : devices) {
      itr.second->startCounters();
    }
  }

  void HALAPIInterface::stopCounters()
  {
    for(auto itr : devices) {
      itr.second->stopCounters();
    }
  }

  void HALAPIInterface::readCounters()
  {
    xclCounterResults counterResults;
    for(auto itr : devices) {
      itr.second->readCounters(counterResults);
    }
  }

  void HALAPIInterface::startTrace()
  {
    for(auto itr : devices) {
      itr.second->startTrace(0);
    }
  }

  void HALAPIInterface::stopTrace()
  {
    for(auto itr : devices) {
      itr.second->stopTrace();
    }
  }

  void HALAPIInterface::readTrace()
  {
    xclTraceResultsVector traceVector;
    for(auto itr : devices) {
      itr.second->readTrace(traceVector);
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
    DeviceIntf* currDevice = nullptr;
    
    try {
      currDevice = devices[deviceHandle];
    } catch (std::exception &) {
      // device not found
      // For now, just return
      return;
    }
    
    // readDebugIPlayout called from startProfiling : check other cases

    xclDeviceInfo2 devInfo;
    if (xclGetDeviceInfo2(deviceHandle, &devInfo) != 0)
    {
      // If we cannot get device information, return an empty profile result
      return ;
    }
    
    auto deviceNameSz = strlen(devInfo.mName);
    results->deviceName = (char*)malloc(deviceNameSz+1);
    memcpy(results->deviceName, devInfo.mName, deviceNameSz);
    results->deviceName[deviceNameSz] = '\0';
    
    results->numAIM = currDevice->getNumMonitors(XCL_PERF_MON_MEMORY);
    results->numAM  = currDevice->getNumMonitors(XCL_PERF_MON_ACCEL);
    results->numASM = currDevice->getNumMonitors(XCL_PERF_MON_STR);
    
    if(results->numAIM) {
      results->kernelTransferData = (KernelTransferData*)calloc(results->numAIM, sizeof(KernelTransferData));
      
      for(unsigned int i=0; i < results->numAIM ; ++i) {
	std::string monName = currDevice->getMonitorName(XCL_PERF_MON_MEMORY, i);
	results->kernelTransferData[i].cuPortName = (char*)malloc(monName.length()+1);
	strcpy(results->kernelTransferData[i].cuPortName, monName.c_str());
	
	// argname
	// memoryname
      }
    }
    
    if(results->numAM) {
      results->cuExecData = (CuExecData*)calloc(results->numAM, sizeof(CuExecData));
      
      for(unsigned int i=0; i < results->numAM ; ++i) {
	std::string monName = currDevice->getMonitorName(XCL_PERF_MON_ACCEL, i);
	results->cuExecData[i].cuName = (char*)malloc((monName.length()+1)*sizeof(char));
	strcpy(results->cuExecData[i].cuName, monName.c_str());
	// kernel name
      }
    }      
    
    
    if(results->numASM) {
      results->streamData = (StreamTransferData*)calloc(results->numASM, sizeof(StreamTransferData));
      
      for(unsigned int i=0; i < results->numASM ; ++i) {
	std::string monName = currDevice->getMonitorName(XCL_PERF_MON_STR, i);
	std::size_t sepPos  = monName.find(IP_LAYOUT_SEP);
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
  
  void HALAPIInterface::calculateAIMRolloverResult(const std::string& key, unsigned int numAIM, xclCounterResults& counterResult, bool firstReadAfterProgram)
  {
    xclCounterResults& loggedResult = mFinalCounterResultsMap[key];
    xclCounterResults& rollOverCount = mRolloverCountsMap[key];
    xclCounterResults& rollOverCounterResult = mRolloverCounterResultsMap[key];
    
    for(unsigned int i=0; i < numAIM ; ++i) {
      // Check for rollover of byte counters; if detected, add 2^32
      // Otherwise, if first read after program with binary, then capture bytes from previous xclbin
      if (!firstReadAfterProgram) {
	// Update "RollOverCount"
	if(counterResult.WriteBytes[i]      < loggedResult.WriteBytes[i])      rollOverCount.WriteBytes[i]++;
	if(counterResult.ReadBytes[i]       < loggedResult.ReadBytes[i])       rollOverCount.ReadBytes[i]++;
	if(counterResult.WriteTranx[i]      < loggedResult.WriteTranx[i])      rollOverCount.WriteTranx[i]++;
	if(counterResult.ReadTranx[i]       < loggedResult.ReadTranx[i])       rollOverCount.ReadTranx[i]++;
	if(counterResult.WriteLatency[i]    < loggedResult.WriteLatency[i])    rollOverCount.WriteLatency[i]++;
	if(counterResult.ReadLatency[i]     < loggedResult.ReadLatency[i])     rollOverCount.ReadLatency[i]++;
	if(counterResult.ReadBusyCycles[i]  < loggedResult.ReadBusyCycles[i])  rollOverCount.ReadBusyCycles[i]++;
	if(counterResult.WriteBusyCycles[i] < loggedResult.WriteBusyCycles[i]) rollOverCount.WriteBusyCycles[i]++;
      } else {
	// Update "RollOverCounterResults" with logged data
	rollOverCounterResult.WriteBytes[i]      += loggedResult.WriteBytes[i];
	rollOverCounterResult.ReadBytes[i]       += loggedResult.ReadBytes[i];
	rollOverCounterResult.WriteTranx[i]      += loggedResult.WriteTranx[i];
	rollOverCounterResult.ReadTranx[i]       += loggedResult.ReadTranx[i];
	rollOverCounterResult.WriteLatency[i]    += loggedResult.WriteLatency[i];
	rollOverCounterResult.ReadLatency[i]     += loggedResult.ReadLatency[i];
	rollOverCounterResult.ReadBusyCycles[i]  += loggedResult.ReadBusyCycles[i];
	rollOverCounterResult.WriteBusyCycles[i] += loggedResult.WriteBusyCycles[i];
      }
    }
  }
  
  void HALAPIInterface::calculateAMRolloverResult(const std::string& key, unsigned int numAM, xclCounterResults& counterResult, bool firstReadAfterProgram)
  {
    xclCounterResults& loggedResult = mFinalCounterResultsMap[key];
    xclCounterResults& rollOverCount = mRolloverCountsMap[key];
    xclCounterResults& rollOverCounterResult = mRolloverCounterResultsMap[key];
    
    for(unsigned int i = 0; i < numAM ; ++i) {
      // Update "RollOverCount" 
      if (!firstReadAfterProgram) {
	if(counterResult.CuExecCycles[i]     < loggedResult.CuExecCycles[i])     rollOverCount.CuExecCycles[i]++;
	if(counterResult.CuBusyCycles[i]     < loggedResult.CuBusyCycles[i])     rollOverCount.CuBusyCycles[i]++;
	if(counterResult.CuStallExtCycles[i] < loggedResult.CuStallExtCycles[i]) rollOverCount.CuStallExtCycles[i]++;
	if(counterResult.CuStallIntCycles[i] < loggedResult.CuStallIntCycles[i]) rollOverCount.CuStallIntCycles[i]++;
	if(counterResult.CuStallStrCycles[i] < loggedResult.CuStallStrCycles[i]) rollOverCount.CuStallStrCycles[i]++;
      } else {
	// Update "RollOverCounterResults" with logged data
	rollOverCounterResult.CuExecCount[i]      += loggedResult.CuExecCount[i];
	rollOverCounterResult.CuExecCycles[i]     += loggedResult.CuExecCycles[i];
	rollOverCounterResult.CuBusyCycles[i]     += loggedResult.CuBusyCycles[i];
	rollOverCounterResult.CuStallExtCycles[i] += loggedResult.CuStallExtCycles[i];
	rollOverCounterResult.CuStallIntCycles[i] += loggedResult.CuStallIntCycles[i];
	rollOverCounterResult.CuStallStrCycles[i] += loggedResult.CuStallStrCycles[i];
      }
      
    }
  }
  
  void HALAPIInterface::recordAMResult(ProfileResults* results, DeviceIntf* /*currDevice*/, const std::string& key)
  {
    //    bool deviceDataExists = (mDeviceBinaryCuSlotsMap.find(key) == mDeviceBinaryCuSlotsMap.end()) ? false : true;
    
    
    xclCounterResults& counterResults = mFinalCounterResultsMap[key];
    xclCounterResults& rollOverCount = mRolloverCountsMap[key];
    xclCounterResults& rollOverCounterResult = mRolloverCounterResultsMap[key];
    
    for(unsigned int i = 0; i < results->numAM ; ++i) {
      
      // if counterResults.CuMaxParallelIter[i] > 0)
      
      results->cuExecData[i].cuExecCount = counterResults.CuExecCount[i] + rollOverCounterResult.CuExecCount[i];
      results->cuExecData[i].cuExecCycles = counterResults.CuExecCycles[i] + rollOverCounterResult.CuExecCycles[i]
	+ (rollOverCount.CuExecCycles[i] * 4294967296UL);
      results->cuExecData[i].cuBusyCycles = counterResults.CuBusyCycles[i] + rollOverCounterResult.CuBusyCycles[i]
	+ (rollOverCount.CuBusyCycles[i] * 4294967296UL);
      
      results->cuExecData[i].cuMaxExecCycles = counterResults.CuMaxExecCycles[i];
      results->cuExecData[i].cuMinExecCycles = counterResults.CuMinExecCycles[i];
      results->cuExecData[i].cuMaxParallelIter = counterResults.CuMaxParallelIter[i];
      results->cuExecData[i].cuStallExtCycles = counterResults.CuStallExtCycles[i];
      results->cuExecData[i].cuStallIntCycles = counterResults.CuStallIntCycles[i];
      results->cuExecData[i].cuStallStrCycles = counterResults.CuStallStrCycles[i];
    }
  }
  
  void HALAPIInterface::recordAIMResult(ProfileResults* results, DeviceIntf* currDevice, const std::string& key)
  {
    xclCounterResults& counterResults = mFinalCounterResultsMap[key];
    xclCounterResults& rollOverCount  = mRolloverCountsMap[key];
    
    for(unsigned int i = 0; i < results->numAIM ; ++i) {
      if(currDevice->isHostAIM(i)) {
	continue;
      }
      
      results->kernelTransferData[i].totalReadBytes = counterResults.ReadBytes[i] + (rollOverCount.ReadBytes[i] * 4294967296UL);
      results->kernelTransferData[i].totalReadTranx = counterResults.ReadTranx[i] + (rollOverCount.ReadTranx[i] * 4294967296UL);
      results->kernelTransferData[i].totalReadLatency = counterResults.ReadLatency[i] + (rollOverCount.ReadLatency[i] * 4294967296UL);
      results->kernelTransferData[i].totalReadBusyCycles = counterResults.ReadBusyCycles[i] + (rollOverCount.ReadBusyCycles[i] * 4294967296UL);
      // min max readLatency
      
      results->kernelTransferData[i].totalWriteBytes = counterResults.WriteBytes[i] + (rollOverCount.WriteBytes[i] * 4294967296UL);
      results->kernelTransferData[i].totalWriteTranx = counterResults.WriteTranx[i] + (rollOverCount.WriteTranx[i] * 4294967296UL);
      results->kernelTransferData[i].totalWriteLatency = counterResults.WriteLatency[i] + (rollOverCount.WriteLatency[i] * 4294967296UL);
      results->kernelTransferData[i].totalWriteBusyCycles = counterResults.WriteBusyCycles[i] + (rollOverCount.WriteBusyCycles[i] * 4294967296UL);
      // min max readLatency
    }
  }
  
  void HALAPIInterface::recordASMResult(ProfileResults* results, DeviceIntf* /*currDevice*/, const std::string& key)
  {
    xclCounterResults& counterResults = mFinalCounterResultsMap[key];
    
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
    // Step 2: log the data into counter and rollover results data-structure
    // Step 3: populate ProfileResults
    
    // check one device for now
    DeviceIntf* currDevice = nullptr;
    
    try {
      currDevice = devices[deviceHandle];
    } catch (std::exception &) {
      // device not found
      // For now, just return
      return;
    }
    
    // Step 1: read counters from device
    
    xclCounterResults counterResults;
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
    
    // Step 2: log the data into counter and rollover results data-structure
    
    // If not already defined, zero out rollover values for this device
    if (mFinalCounterResultsMap.find(key) == mFinalCounterResultsMap.end()) {
      mFinalCounterResultsMap[key] = counterResults;
      
      xclCounterResults rolloverResults;
      memset(&rolloverResults, 0, sizeof(xclCounterResults));
      mRolloverCounterResultsMap[key] = rolloverResults;
      mRolloverCountsMap[key] = rolloverResults;
    } else {
      
      calculateAIMRolloverResult(key, results->numAIM, counterResults, true /*firstReadAfterProgram*/);
      calculateAMRolloverResult (key, results->numAM,  counterResults, true /*firstReadAfterProgram*/);
      
      // Streaming IP Counters are 64 bit and unlikely to roll over
      
      // Log current counter result
      mFinalCounterResultsMap[key] = counterResults;
    }
    
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
