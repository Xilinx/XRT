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

#include <cstring>
#include <chrono>
#include "hal_profiler.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
// xclShim methods header file

//#include "core/include/profile_results.h"

namespace xdp {

HALProfiler* HALProfiler::Instance()
{
  std::cout << " In HALProfiler::Instance " << std::endl;
  // check validity
  static HALProfiler instance;
  return &instance;
}

HALProfiler::~HALProfiler()
{
  std::cout << " In HALProfiler::HALProfile dest " << std::endl;
  // check if already dead ?
  endProfiling();

  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    delete (*itr);
    (*itr) = nullptr;
  }
}

void HALProfiler::startProfiling(xclDeviceHandle handle)
{
  std::cout << " In HALProfiler::startProfiling" << std::endl;

  // create the devices
  // find device for handle ; if not found then create and add device
  // for now directly create the device
  DeviceIntf* dev = new DeviceIntf();
  deviceList.push_back(dev);

  dev->setDevice(new xdp::HalDevice(handle));
  dev->readDebugIPlayout();

  // check the flags 
  startCounters();

#if 0
  // Not supported now
  startTrace();
#endif
}

void HALProfiler::endProfiling()
{
  std::cout << " In HALProfiler::endProfiling" << std::endl;
  // check the flags 
  stopCounters();
#if 0
  stopTrace();

  xclCounterResults counterResults;
  readCounters(counterResults);
  readTrace();
#endif
}

void HALProfiler::startCounters()
{
  std::cout << " In HALProfiler::startCounters" << std::endl;
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->startCounters((xclPerfMonType)0);
  }
}

void HALProfiler::stopCounters()
{
  std::cout << " In HALProfiler::stopCounters" << std::endl;
  for(std::vector<DeviceIntf*>::iterator itr = deviceList.begin() ; itr != deviceList.end() ; ++itr) {
    (*itr)->stopCounters((xclPerfMonType)0);
  }
}

void HALProfiler::readCounters(xclCounterResults& counterResults)
{
  std::cout << " In HALProfiler::readCounters" << std::endl;
//  xclCounterResults counterResults;
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
#if 0
void HALProfiler::createProfileResults(xclDeviceHandle, void* ret)
{
  ProfileResults** retResults = static_cast<ProfileResults**>(ret);

  // create profile result
  ProfileResults* results = (ProfileResults*)malloc(sizeof(ProfileResults));
  *retResults = results;

  // Initialise profile monitor numbers in ProfileResult and allocate memory
  // Use 1 device now
  DeviceIntf* currDevice = deviceList[0];
// readDebugIPlayout called from startProfiling : check other scenaria

  results->numAIM = currDevice->getNumMonitors(XCL_PERF_MON_MEMORY);
  results->numAM  = currDevice->getNumMonitors(XCL_PERF_MON_ACCEL);
  results->numASM = currDevice->getNumMonitors(XCL_PERF_MON_STR);

  if(results->numAIM)
    results->kernelTransferData = (KernelTransferData*)malloc(results->numAIM * sizeof(KernelTransferData));

  if(results->numAM)
    results->cuExecData = (CuExecData*)malloc(results->numAM * sizeof(CuExecData));

  if(results->numASM)
    results->streamData = (StreamTransferData*)malloc(results->numASM * sizeof(StreamTransferData));

  // populate the names
}
#endif


void HALProfiler::createProfileResults(xclDeviceHandle, void* ret)
{
  ProfileResults** retResults = static_cast<ProfileResults**>(ret);

  // create profile result
  ProfileResults* results = (ProfileResults*)calloc(1, sizeof(ProfileResults));
  *retResults = results;

  // Initialise profile monitor numbers in ProfileResult and allocate memory
  // Use 1 device now
  DeviceIntf* currDevice = deviceList[0];
// readDebugIPlayout called from startProfiling : check other scenaria

  results->numAIM = currDevice->getNumMonitors(XCL_PERF_MON_MEMORY);
  results->numAM  = currDevice->getNumMonitors(XCL_PERF_MON_ACCEL);
  results->numASM = currDevice->getNumMonitors(XCL_PERF_MON_STR);

  if(results->numAIM)
    results->kernelTransferData = (KernelTransferData*)calloc(results->numAIM, sizeof(KernelTransferData));

  if(results->numAM)
    results->cuExecData = (CuExecData*)calloc(results->numAM, sizeof(CuExecData));

  if(results->numASM)
    results->streamData = (StreamTransferData*)calloc(results->numASM, sizeof(StreamTransferData));

  // populate the names
}


void HALProfiler::calculateAIMRolloverResult(std::string key, unsigned int numAIM, xclCounterResults& counterResult, bool firstReadAfterProgram)
{
      // Traverse all monitor slots (host and all CU ports)
//      bool deviceDataExists = (mDeviceBinaryDataSlotsMap.find(key) == mDeviceBinaryDataSlotsMap.end()) ? false : true;
  xclCounterResults& loggedResult = mFinalCounterResultsMap[key];
  xclCounterResults& rollOverCount = mRolloverCountsMap[key];
  xclCounterResults& rollOverCounterResult = mRolloverCounterResultsMap[key];

  for(unsigned int i=0; i < numAIM ; ++i) {
#if 0
        mPluginHandle->getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, s, slotName);
        if (!deviceDataExists) {
          mDeviceBinaryDataSlotsMap[key].push_back(slotName);
          auto p = mPluginHandle->getProfileSlotProperties(XCL_PERF_MON_MEMORY, deviceName, s);
          mDataSlotsPropertiesMap[key].push_back(p);
        }
#endif
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

void HALProfiler::calculateAMRolloverResult(std::string key, unsigned int numAM, xclCounterResults& counterResult, bool firstReadAfterProgram)
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

void HALProfiler::recordAMResult(ProfileResults* results, DeviceIntf* currDevice, std::string key)
{
//    bool deviceDataExists = (mDeviceBinaryCuSlotsMap.find(key) == mDeviceBinaryCuSlotsMap.end()) ? false : true;


  xclCounterResults& counterResults = mFinalCounterResultsMap[key];
  xclCounterResults& rollOverCount = mRolloverCountsMap[key];
  xclCounterResults& rollOverCounterResult = mRolloverCounterResultsMap[key];

  for(unsigned int i = 0; i < results->numAM ; ++i) {

// if counterResults.CuMaxParallelIter[i] > 0)
    std::string monName = currDevice->getMonitorName(XCL_PERF_MON_ACCEL, i); // cu name ?
// get profile kernel name getProfileKernelName
    auto len = monName.length();
    results->cuExecData[i].cuName = (char*)malloc((len+1)*sizeof(char));
    memcpy(results->cuExecData[i].cuName, monName.c_str(), len);
    results->cuExecData[i].cuName[len] = '\0';

    // kernel name
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

void HALProfiler::recordAIMResult(ProfileResults* results, DeviceIntf* currDevice, std::string key)
{
  xclCounterResults& counterResults = mFinalCounterResultsMap[key];
  xclCounterResults& rollOverCount = mRolloverCountsMap[key];
//  xclCounterResults& rollOverCounterResult = mRolloverCounterResultsMap[key];
  for(unsigned int i = 0; i < results->numAIM ; ++i) {
    // skip XAIM_HOST_PROPERTY_MASK and no monitor name

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
  } // end for
}

void HALProfiler::recordASMResult(ProfileResults* results, DeviceIntf* currDevice, std::string key)
{
  xclCounterResults& counterResults = mFinalCounterResultsMap[key];
// populate the names etc. 
  for(unsigned int i = 0; i < results->numASM ; ++i) {
    // skip XAIM_HOST_PROPERTY_MASK and no monitor name

    results->streamData[i].strmNumTranx = counterResults.StrNumTranx[i];
    results->streamData[i].strmBusyCycles = counterResults.StrBusyCycles[i];
    results->streamData[i].strmDataBytes = counterResults.StrDataBytes[i];
    results->streamData[i].strmStallCycles = counterResults.StrStallCycles[i];
    results->streamData[i].strmStarveCycles = counterResults.StrStarveCycles[i];

  } // end for
}

  // Step 1: read counters from device
  // Step 2: Initialise profile monitor numbers in ProfileResult and allocate memory
  // Step 3: log the data into counter and rollover results data-structure
  // Step 4: populate ProfileResults
void HALProfiler::getProfileResults(xclDeviceHandle, void* res)
{
  // Step 1: read counters from device
  // Step 2: Initialise profile monitor numbers in ProfileResult and allocate memory
  // Step 3: log the data into counter and rollover results data-structure
  // Step 4: populate ProfileResults

  std::cout << " In HALProfiler::getProfileResults" << std::endl;

  // check one device for now

  // Step 1: read counters from device

  xclCounterResults counterResults;
  readCounters(counterResults); // read from device, done

  ProfileResults* results = static_cast<ProfileResults*>(res);
  // Use 1 device now
  DeviceIntf* currDevice = deviceList[0];

#if 0
  // Step 2: Initialise profile monitor numbers in ProfileResult and allocate memory
  // Use 1 device now
  DeviceIntf* currDevice = deviceList[0];
  ProfileResults* results = static_cast<ProfileResults*>(res);

  results->numAIM = currDevice->getNumMonitors(XCL_PERF_MON_MEMORY);
  results->numAM  = currDevice->getNumMonitors(XCL_PERF_MON_ACCEL);
  results->numASM = currDevice->getNumMonitors(XCL_PERF_MON_STR);

  results->kernelTransferData = (KernelTransferData*)malloc(results->numAIM * sizeof(KernelTransferData));
  results->cuExecData = (CuExecData*)malloc(results->numAM * sizeof(CuExecData));
  results->streamData = (StreamTransferData*)malloc(results->numASM * sizeof(StreamTransferData));
#endif   
  // Record the counter data 
//  auto timeSinceEpoch = (std::chrono::steady_clock::now()).time_since_epoch();
//  auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch);
//  uint64_t timeNsec = value.count();

  // Create unique name for device since currently all devices are called fpga0
  std::string deviceName = "unique_device";
  std::string binaryName = "fpga0";
//  uint32_t program_id = 0;

  std::string key = deviceName + "|" + binaryName;

  // Step 3: log the data into counter and rollover results data-structure

  // If not already defined, zero out rollover values for this device
  if (mFinalCounterResultsMap.find(key) == mFinalCounterResultsMap.end()) {
    mFinalCounterResultsMap[key] = counterResults;

    xclCounterResults rolloverResults;
    memset(&rolloverResults, 0, sizeof(xclCounterResults));
    //rolloverResults.NumSlots = counterResults.NumSlots;
    mRolloverCounterResultsMap[key] = rolloverResults;
    mRolloverCountsMap[key] = rolloverResults;
  } else {

    calculateAIMRolloverResult(key, results->numAIM, counterResults, true /*firstReadAfterProgram*/);
    calculateAMRolloverResult (key, results->numAM,  counterResults, true /*firstReadAfterProgram*/);

    // for stream counters only properties need to be populated : check

    // Log current counter result
    mFinalCounterResultsMap[key] = counterResults;
  }

// record is per device

   // log AM into result
   recordAMResult(results, currDevice, key);
   recordAIMResult(results, currDevice, key);
// record total AIM data over all the slots ? : i guess no


   recordASMResult(results, currDevice, key);
}

void HALProfiler::destroyProfileResults(xclDeviceHandle, void* ret)
{
  ProfileResults* results = static_cast<ProfileResults*>(ret);

  // clear AIM data
  for(unsigned int i = 0; i < results->numAIM ; ++i) {
    free(results->kernelTransferData[i].deviceName);
    free(results->kernelTransferData[i].cuPortName);
    free(results->kernelTransferData[i].argName);
    free(results->kernelTransferData[i].memoryName);

    results->kernelTransferData[i].deviceName = '\0';
    results->kernelTransferData[i].cuPortName = '\0';
    results->kernelTransferData[i].argName = '\0';
    results->kernelTransferData[i].memoryName = '\0';
  }
  free(results->kernelTransferData);
  results->kernelTransferData = '\0';


  // clear AM data
  for(unsigned int i = 0; i < results->numAM ; ++i) {
    free(results->cuExecData[i].cuName);
    free(results->cuExecData[i].kernelName);

    results->cuExecData[i].cuName = '\0';
    results->cuExecData[i].kernelName = '\0';
  }
  free(results->cuExecData);
  results->cuExecData = '\0';

  // clear ASM data
  for(unsigned int i = 0; i < results->numASM ; ++i) {
    free(results->streamData[i].deviceName);
    free(results->streamData[i].masterPortName);
    free(results->streamData[i].slavePortName);

    results->streamData[i].deviceName = '\0';
    results->streamData[i].masterPortName = '\0';
    results->streamData[i].slavePortName = '\0';
  }
  free(results->streamData);
  results->streamData = '\0';
}

}
