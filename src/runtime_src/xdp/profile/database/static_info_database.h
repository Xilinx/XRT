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

  class ComputeUnitInstance
  {
  private:
    std::string name ;

    // In OpenCL, each compute unit is set up with a static workgroup size
    int dim[3] ;

    // The connections require the original index in the ip_layout
    int index ; 

    // A mapping of arguments to memory resources
    std::map<std::string, std::vector<int>> connections ;

    ComputeUnitInstance() = delete ;
  public:
    
    // Getters and setters
    inline const std::string& getName() { return name ; }
    XDP_EXPORT std::string getDim() ;

    XDP_EXPORT ComputeUnitInstance(const char* n, int i) ;
    XDP_EXPORT ~ComputeUnitInstance() ;
  } ;

  struct PlatformInfo {
    uint64_t    kdmaCount;
    std::string deviceName;
  };

  struct DeviceInfo {
    struct PlatformInfo platformInfo;
    std::string loadedXclbin;
    std::vector<ComputeUnitInstance> cus;
    std::vector<std::pair<uint64_t, std::string>> ddrBanks;
    std::vector<std::pair<uint64_t, std::string>> hbmBanks;
    std::vector<std::pair<uint64_t, std::string>> plramBanks;    
  };

  class VPStaticDatabase
  {
  private:
    // ********* Information specific to each host execution **********
    int pid ;

    // ***** OpenCL Information ******
    std::set<uint64_t> commandQueueAddresses ;



    // Device Info
    std::map<uint64_t, DeviceInfo*> deviceInfo;

    std::map<void*, uint64_t> devInfo;

    std::map<std::string /*sysfsPath*/, uint64_t /*devId*/> uniqueDevices;

    std::vector<struct DeviceInfo*> dev;

    // ***** HAL Information ******

    // ********* Information specific to each platform **********


    // ********* Information specific to each xclbin **********
    // Device handle to name of the xclbin loaded on that device
    std::map<void*, std::string> loadedXclbins ;

    // Device handle to a list of compute units
    std::map<void*, std::vector<ComputeUnitInstance>> cus ;
    // Device handle to addresses of DDR banks
    std::map<void*, std::vector<std::pair<uint64_t, std::string>>> ddrBanks ;
    // Device handle to addresses of HBM banks
    std::map<void*, std::vector<std::pair<uint64_t, std::string>>> hbmBanks ;
    // Device handle to addresses of PLRAM banks
    std::map<void*, std::vector<std::pair<uint64_t, std::string>>> plramBanks ;

    // Static info can be accessed via any host thread
    std::mutex dbLock ;

    void resetDeviceInfo(uint64_t deviceId) ;
#if 0
    // Helper functions that fill in device information
    bool initializeMemory(void* dev, const void* binary) ;
    bool initializeComputeUnits(void* dev, const void* binary) ;
    bool initializeConnections(void* dev, const void* binary) ;
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

    inline void setXclbinName(uint64_t deviceId, std::string name) { if(deviceInfo.find(deviceId) == deviceInfo.end()) return; deviceInfo[deviceId]->loadedXclbin = name; }
    inline std::string getXclbinName(uint64_t deviceId) { if(deviceInfo.find(deviceId) == deviceInfo.end()) return std::string(""); return deviceInfo[deviceId]->loadedXclbin; }


    inline std::vector<ComputeUnitInstance>& getCUs(void* dev)
    { return cus[dev] ; } 

    // Reseting device information whenever a new xclbin is added
    //XDP_EXPORT void updateDevice(void* dev, const void* binary) ;
    XDP_EXPORT void updateDevice(uint64_t deviceId, const void* binary) ;

    // Functions that add information to the database
    XDP_EXPORT void addCommandQueueAddress(uint64_t a) ;
    XDP_EXPORT void addKDMACount(void* dev, uint16_t numKDMAs) ;
  } ;

}

#endif
