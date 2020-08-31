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
#include "core/common/system.h"

namespace xdp {

  // Forward declarations
  class VPDatabase ;
  class VPWriter ;

  struct Monitor {
    uint8_t     type;
    uint64_t    index;
    int32_t     cuIndex;
    int32_t     memIndex;
    std::string name;
    bool        isRead;

    Monitor(uint8_t ty, uint64_t idx, const char* n, int32_t cuId = -1, int32_t memId = -1)
      : type(ty),
        index(idx),
        cuIndex(cuId),
        memIndex(memId),
        name(n),
        isRead(false)
    {}
  };

  struct AIECounter {
    uint32_t id;
    uint16_t column;
    uint16_t row;
    uint8_t counterNumber;
    uint8_t startEvent;
    uint8_t endEvent;
    uint8_t resetEvent;
    double clockFreqMhz;
    std::string module;
    std::string name;

    AIECounter(uint32_t i, uint16_t col, uint16_t r, uint8_t num, 
               uint8_t start, uint8_t end, uint8_t reset,
               double freq, std::string mod, std::string aieName)
      : id(i),
        column(col),
        row(r),
        counterNumber(num),
        startEvent(start),
        endEvent(end),
        resetEvent(reset),
        clockFreqMhz(freq),
        module(mod),
        name(aieName)
    {}
  };

  struct TraceGMIO {
    uint32_t id;
    uint16_t shimColumn;
    uint16_t channelNumber;
    uint16_t streamId;
    uint16_t burstLength;

    TraceGMIO(uint32_t i, uint16_t col, uint16_t num, 
              uint16_t stream, uint16_t len)
      : id(i),
        shimColumn(col),
        channelNumber(num),
        streamId(stream),
        burstLength(len)
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
    int32_t dim[3] ;

    // A mapping of arguments to memory resources
    std::map<int32_t, std::vector<int32_t>> connections ;

    std::vector<Monitor*> monitors;

    bool stall        = false;
    bool stream       = false;
    bool dataflow     = false;
    bool dataTransfer = false;

    ComputeUnitInstance() = delete ;
  public:
    
    // Getters and setters
    inline const std::string& getName() { return name ; }
    inline const std::string& getKernelName() { return kernelName ; }

    inline int32_t getIndex() { return index ; }

    inline void setDim(int32_t x, int32_t y, int32_t z) { dim[0] = x; dim[1] = y; dim[2] = z; }
    XDP_EXPORT std::string getDim() ;

    XDP_EXPORT void addConnection(int32_t, int32_t);
    std::map<int32_t, std::vector<int32_t>>* getConnections()
    {  return &connections; }

    void addMonitor(Monitor* m) { monitors.push_back(m); }

    void setStallEnabled(bool b) { stall = b; }
    bool stallEnabled() { return stall; }

    void setStreamEnabled(bool b) { stream = b; }
    bool streamEnabled() { return stream; }

    void setDataflowEnabled(bool b) { dataflow = b; }
    bool dataflowEnabled() { return dataflow; }

    void setDataTransferEnabled(bool b) { dataTransfer = b; }
    bool dataTransferEnabled() { return dataTransfer; }

    XDP_EXPORT ComputeUnitInstance(int32_t i, const std::string &n);
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
    uint64_t    size;
    std::string name;

    Memory(uint8_t ty, int32_t idx, uint64_t baseAddr, uint64_t sz, const char* n)
      : type(ty),
        index(idx),
        baseAddress(baseAddr),
        size(sz),
        name(n)
    {}
  };

  struct DeviceInfo {
    bool isReady;
    double clockRateMHz;
    struct PlatformInfo platformInfo;
    std::string loadedXclbin;
    std::map<int32_t, ComputeUnitInstance*> cus;
    //uuid        loadedXclbinUUID;
    std::map<int32_t, Memory*> memoryInfo;
    std::vector<Monitor*>      aimList;
    std::vector<Monitor*>      amList;
    std::vector<Monitor*>      asmList;
    std::vector<Monitor*>      nocList;
    std::vector<AIECounter*>   aieList;
    std::vector<TraceGMIO*>    gmioList;

    bool hasFloatingAIM = false;
    bool hasFloatingASM = false;

    ~DeviceInfo()
    {
      for(auto i : cus) {
        delete i.second;
      }
      cus.clear();
      for(auto i : memoryInfo) {
        delete i.second;
      }
      memoryInfo.clear();
      for(auto i : aimList) {
        delete i;
      }
      aimList.clear();
      for(auto i : amList) {
        delete i;
      }
      amList.clear();
      for(auto i : asmList) {
        delete i;
      }
      asmList.clear();
      for(auto i : nocList) {
        delete i;
      }
      nocList.clear();
      for(auto i : aieList) {
        delete i;
      }
      aieList.clear();
      for(auto i : gmioList) {
        delete i;
      }
      gmioList.clear();
    }
  };

  class VPStaticDatabase
  {
  private:
    // Parent pointer to database so we can issue broadcasts
    VPDatabase* db ;
    // The static database handles the single instance of the run summary
    VPWriter* runSummary ;

  private:
    // ********* Information specific to each host execution **********
    int pid ;
    
    // The files that need to be included in the run summary for
    //  consumption by Vitis_Analyzer
    std::vector<std::pair<std::string, std::string> > openedFiles ;

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
    //bool setXclbinUUID(DeviceInfo*, const std::shared_ptr<xrt_core::device>& device);
    bool setXclbinName(DeviceInfo*, const std::shared_ptr<xrt_core::device>& device);
    bool initializeComputeUnits(DeviceInfo*, const std::shared_ptr<xrt_core::device>&);
    bool initializeProfileMonitors(DeviceInfo*, const std::shared_ptr<xrt_core::device>&);
    bool initializeAIECounters(DeviceInfo*, const std::shared_ptr<xrt_core::device>&);

  public:
    VPStaticDatabase(VPDatabase* d) ;
    ~VPStaticDatabase() ;

    // Getters and setters
    inline int getPid() { return pid ; }
    inline std::vector<std::pair<std::string, std::string>>& getOpenedFiles() 
      { return openedFiles ; }
    inline std::set<uint64_t>& getCommandQueueAddresses() 
      { return commandQueueAddresses ; }

    inline DeviceInfo* getDeviceInfo(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId];
    }

    bool isDeviceReady(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false; 
      return deviceInfo[deviceId]->isReady;
    }

    double getClockRateMHz(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 300; 
      return deviceInfo[deviceId]->clockRateMHz;
    }

    void setDeviceName(uint64_t deviceId, std::string name)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return; 
      deviceInfo[deviceId]->platformInfo.deviceName = name;
    }
    std::string getDeviceName(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return std::string(""); 
      return deviceInfo[deviceId]->platformInfo.deviceName; 
    }

    void setKDMACount(uint64_t deviceId, uint64_t num)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return;
      deviceInfo[deviceId]->platformInfo.kdmaCount = num;
    }
    uint64_t getKDMACount(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->platformInfo.kdmaCount; 
    }
#if 0
    uuid getXclbinUUID(uint64_t deviceId) { 
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->loadedXclbinUUID; 
    }
#endif

    std::string getXclbinName(uint64_t deviceId) { 
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return std::string(""); 
      return deviceInfo[deviceId]->loadedXclbin; 
    }

    ComputeUnitInstance* getCU(uint64_t deviceId, int32_t cuId)
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

    Memory* getMemory(uint64_t deviceId, int32_t memId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->memoryInfo[memId];
    }

    inline uint64_t getNumAIM(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->aimList.size();
    }

    inline uint64_t getNumAM(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->amList.size();
    }

    inline uint64_t getNumASM(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->asmList.size();
    }

    inline uint64_t getNumNOC(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->nocList.size();
    }

    inline uint64_t getNumAIECounter(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->aieList.size();
    }

    inline uint64_t getNumTraceGMIO(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->gmioList.size();
    }

    inline Monitor* getAIMonitor(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->aimList[idx];
    }

    inline std::vector<Monitor*>* getAIMonitors(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return &(deviceInfo[deviceId]->aimList);
    }

    inline Monitor* getAMonitor(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->amList[idx];
    }

    inline Monitor* getASMonitor(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->asmList[idx];
    }

    inline Monitor* getNOC(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->nocList[idx];
    }

    inline AIECounter* getAIECounter(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->aieList[idx];
    }

    inline TraceGMIO* getTraceGMIO(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->gmioList[idx];
    }
    
    inline std::vector<Monitor*>* getASMonitors(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return &(deviceInfo[deviceId]->asmList);
    }

    inline void getDataflowConfiguration(uint64_t deviceId, bool* config, size_t size)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return;

      size_t count = 0;
      for(auto mon : deviceInfo[deviceId]->amList) {
        if(count >= size)
          return;
        auto cu = deviceInfo[deviceId]->cus[mon->cuIndex];
        config[count] = cu->dataflowEnabled();
        ++count;
      }
    }

    inline bool hasFloatingAIM(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false;
      return deviceInfo[deviceId]->hasFloatingAIM;
    }

    inline bool hasFloatingASM(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false;
      return deviceInfo[deviceId]->hasFloatingASM;
    }

    // Reseting device information whenever a new xclbin is added
    XDP_EXPORT void updateDevice(uint64_t deviceId, void* devHandle) ;

    // Functions that add information to the database
    XDP_EXPORT void addCommandQueueAddress(uint64_t a) ;
    XDP_EXPORT void addKDMACount(void* dev, uint16_t numKDMAs) ;

    XDP_EXPORT void addOpenedFile(const std::string& name, 
				  const std::string& type) ;
  } ;

}

#endif
