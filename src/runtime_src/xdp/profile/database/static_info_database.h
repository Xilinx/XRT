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
    const std::string& getName() { return name ; }
    std::string getDim() ;

    ComputeUnitInstance(const char* n, int i) ;
    ~ComputeUnitInstance() ;
  } ;

  class VPStaticDatabase
  {
  private:
    // ********* Information specific to each host execution **********
    int pid ;

    // ***** OpenCL Information ******
    std::set<uint64_t> commandQueueAddresses ;

    // ***** HAL Information ******

    // ********* Information specific to each platform **********

    // Device handle to number of KDMA
    std::map<void*, uint16_t> kdmaCount ;
    // Device handle to device name
    std::map<void*, std::string> deviceNames ;

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

    void resetDeviceInfo(void* dev) ;

    // Helper functions that fill in device information
    bool initializeMemory(void* dev, const void* binary) ;
    bool initializeComputeUnits(void* dev, const void* binary) ;
    bool initializeConnections(void* dev, const void* binary) ;

  public:
    VPStaticDatabase() ;
    ~VPStaticDatabase() ;

    // Getters and setters
    inline int getPid() { return pid ; }
    inline std::set<uint64_t>& getCommandQueueAddresses() 
      { return commandQueueAddresses ; }
    inline std::string getDeviceName(void* dev) { return deviceNames[dev] ; }
    inline std::string getXclbinName(void* dev) { return loadedXclbins[dev] ; } 
    inline uint16_t getKDMACount(void* dev)     { return kdmaCount[dev] ; }
    inline std::vector<ComputeUnitInstance>& getCUs(void* dev)
      { return cus[dev] ; } 

    // Reseting device information whenever a new xclbin is added
    void updateDevice(void* dev, const void* binary) ;

    // Functions that add information to the database
    void addCommandQueueAddress(uint64_t a) ;
    void addKDMACount(void* dev, uint16_t numKDMAs) ;
  } ;

}

#endif
