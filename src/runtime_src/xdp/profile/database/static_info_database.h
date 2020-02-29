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

#ifndef STATIC_INFO_DATABASE_DOT_H
#define STATIC_INFO_DATABASE_DOT_H

#include <map>
#include <set>
#include <mutex>
#include <vector>
#include <string>

#include "xdp/config.h"

namespace xdp {

  struct Monitor {
    uint8_t     type;
    uint64_t    index;
    int32_t     cuIndex;
    int32_t     memIndex;
    std::string name;

    Monitor(uint8_t ty, uint64_t idx, const char* n, int32_t cuId = -1, int32_t memId = -1)
      : type(ty),
        index(idx),
        cuIndex(cuId),
        memIndex(memId),
        name(n)
    {}
  };

  class ComputeUnitInstance
  {
  private:
    // The connections require the original index in the ip_layout
    int32_t index ; 

    std::string name ;
    std::string kernelName ;

    // In OpenCL, each compute unit is set up with a static workgroup size
    int dim[3] ;

    // A mapping of arguments to memory resources
    std::map<int32_t, std::vector<int32_t>> connections ;

    std::vector<Monitor*> monitors;

    bool stall = false;
    bool dataTransfer = false;

    ComputeUnitInstance() = delete ;
  public:
    
    // Getters and setters
    inline const std::string& getName() { return name ; }
    inline const std::string& getKernelName() { return kernelName ; }
    inline int32_t getIndex() { return index ; }
    XDP_EXPORT std::string getDim() ;
    XDP_EXPORT void addConnection(int32_t, int32_t);
    std::map<int32_t, std::vector<int32_t>>* getConnections()
    {  return &connections; }

    void addMonitor(Monitor* m) { monitors.push_back(m); }

    void setStallEnabled(bool b) { stall = b; }
    bool stallEnabled() { return stall; }

    void setDataTransferEnabled(bool b) { dataTransfer = b; }
    bool dataTransferEnabled() { return dataTransfer; }

    bool hasStream() { return false; }

    XDP_EXPORT ComputeUnitInstance(int32_t i, const char* n);
    XDP_EXPORT ~ComputeUnitInstance() ;
  } ;

  struct PlatformInfo {
    uint64_t    kdmaCount;
    std::string deviceName;
  };

  struct Memory {
    uint8_t     type;
    int32_t     index;
    uint64_t    baseAddress;
    std::string name;

    Memory(uint8_t ty, int32_t idx, uint64_t baseAddr, const char* n)
      : type(ty),
        index(idx),
        baseAddress(baseAddr),
        name(n)
    {}
  };

  struct DeviceInfo {
    struct PlatformInfo platformInfo;
    std::string loadedXclbin;
    std::map<int32_t, ComputeUnitInstance*> cus;
    std::map<int32_t, Memory*>   memoryInfo;
    std::vector<Monitor*> aimList;
    std::vector<Monitor*> amList;
    std::vector<Monitor*> asmList;
    std::map<uint64_t, Monitor*>   monitorInfo;
  };

  class VPStaticDatabase
  {
  private:
    // ********* Information specific to each host execution **********
    int pid ;

    // ***** OpenCL Information ******
    std::set<uint64_t> commandQueueAddresses ;

    /* Device Specific Information mapped to the Unique Device Id
     * Device Information contains :
     * 1. Platform information :
     *    a. KDMA count
     *    b. Device Name
     * 2. Loaded xclbin 
     *    a. Name of loaded xclbin
     *    b. Map of Compute Units
     *    c. Map of connected Memory
     */
    std::map<uint64_t, DeviceInfo*> deviceInfo;

    // Static info can be accessed via any host thread
    std::mutex dbLock ;

    void resetDeviceInfo(uint64_t deviceId) ;

    // Helper functions that fill in device information
    bool setXclbinUUID(DeviceInfo*, const void* binary);
    bool initializeComputeUnits(DeviceInfo*, const void* binary);
    bool initializeProfileMonitorConnections(DeviceInfo*, const void* binary);
#if 0
    bool initializeMemory(DeviceInfo* devInfo, const void* binary) ;
    bool initializeComputeUnits(DeviceInfo* devInfo, const void* binary) ;
    bool initializeConnections(DeviceInfo* devInfo, const void* binary) ;
#endif

  public:
    VPStaticDatabase() ;
    ~VPStaticDatabase() ;

    // Getters and setters
    inline int getPid() { return pid ; }
    inline std::set<uint64_t>& getCommandQueueAddresses() 
      { return commandQueueAddresses ; }

    inline void setDeviceName(uint64_t deviceId, std::string name) { if(deviceInfo.find(deviceId) == deviceInfo.end()) return; deviceInfo[deviceId]->platformInfo.deviceName = name; }
    inline std::string getDeviceName(uint64_t deviceId) { if(deviceInfo.find(deviceId) == deviceInfo.end()) return std::string(""); return deviceInfo[deviceId]->platformInfo.deviceName; }

    inline void setKDMACount(uint64_t deviceId, uint64_t num) { if(deviceInfo.find(deviceId) == deviceInfo.end()) return; deviceInfo[deviceId]->platformInfo.kdmaCount = num; }
    inline uint16_t getKDMACount(uint64_t deviceId) { if(deviceInfo.find(deviceId) == deviceInfo.end()) return 0; return deviceInfo[deviceId]->platformInfo.kdmaCount; }

    inline std::string getXclbinUUID(uint64_t deviceId) { 
        if(deviceInfo.find(deviceId) == deviceInfo.end()) return std::string(""); 
        return deviceInfo[deviceId]->loadedXclbin; 
    }

    inline ComputeUnitInstance* getCU(uint64_t deviceId, int32_t cuId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->cus[cuId];
    }

    inline std::map<int32_t, ComputeUnitInstance*>* getCUs(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return &(deviceInfo[deviceId]->cus);
    }

    inline std::map<int32_t, Memory*>* getMemoryInfo(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return &(deviceInfo[deviceId]->memoryInfo);
    }
#if 0
    inline std::map<uint64_t, Monitor*>* getMonitorInfo(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return &(deviceInfo[deviceId]->monitorInfo);
    }
#endif

    inline Monitor* getAIMonitor(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->aimList[idx];
    }
    inline Monitor* getAMonitor(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->amList[idx];
    }

    // Reseting device information whenever a new xclbin is added
    //XDP_EXPORT void updateDevice(void* dev, const void* binary) ;
    XDP_EXPORT void updateDevice(uint64_t deviceId, const void* binary) ;

    // Functions that add information to the database
    XDP_EXPORT void addCommandQueueAddress(uint64_t a) ;
  } ;

}

#endif
