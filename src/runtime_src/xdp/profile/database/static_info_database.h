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
#include <list>
#include <memory> // for unique_ptr

#include "xdp/config.h"
#include "xdp/profile/device/device_intf.h"
#include "core/common/system.h"
#include "core/common/device.h"


namespace xdp {

  // Forward declarations
  class VPDatabase;
  class VPWriter;
  class DeviceIntf;

  struct Monitor {
    uint8_t     type;
    uint64_t    index;
    int32_t     cuIndex;
    int32_t     memIndex;
    std::string name;
    std::string args; // For OpenCL monitors associated with a port on a CU
    std::string port;
    uint64_t    portWidth; // For OpenCL monitors associated with a port
    bool        isRead;

    Monitor(uint8_t ty, uint64_t idx, const char* n, int32_t cuId = -1, int32_t memId = -1)
      : type(ty),
        index(idx),
        cuIndex(cuId),
        memIndex(memId),
        name(n),
        args(""),
        port(""),
        portWidth(0),
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

  /*
   * AIE Config Writer Classes
   */
  class aie_cfg_counter
  {
    public:
      uint32_t start_event = 0;
      uint32_t stop_event = 0;
      uint32_t reset_event = 0;
      uint32_t event_value = 0;
      uint32_t counter_value = 0;
  };

  class aie_cfg_base
  {
    public:
      uint32_t packet_type = 0;
      uint32_t packet_id = 0;
      uint32_t start_event = 28;
      uint32_t stop_event = 29;
      uint32_t traced_events[8] = {0};
      std::map<uint32_t, uint32_t> group_event_config = {};
      uint32_t combo_event_input[4] = {0};
      uint32_t combo_event_control[3] = {0};
      uint32_t broadcast_mask_south = 65535;
      uint32_t broadcast_mask_north = 65535;
      uint32_t broadcast_mask_west = 65535;
      uint32_t broadcast_mask_east = 65535;
      uint32_t internal_events_broadcast[16] = {0};
      std::vector<aie_cfg_counter> pc;

      aie_cfg_base(uint32_t count) : pc(count) {};
  };

class aie_cfg_core : public aie_cfg_base
{
  public:
    uint32_t trace_mode = 1;
    std::string port_trace = "null";
    aie_cfg_core() : aie_cfg_base(4)
    {
      group_event_config = {
        {2 ,  0},
        {15,  0},
        {22,  0},
        {32,  0},
        {46,  0},
        {47,  0},
        {73,  0},
        {106, 0},
        {123, 0}
      };
    };
};

class aie_cfg_memory : public aie_cfg_base
{
  public:
    aie_cfg_memory() : aie_cfg_base(2) {};
};

class aie_cfg_tile
{
  public:
    uint32_t column;
    uint32_t row;
    aie_cfg_core core_trace_config;
    aie_cfg_memory memory_trace_config;
    aie_cfg_tile(uint32_t c, uint32_t r) : column(c), row(r) {}
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

    // The slot ID inside the counter results structure for this CU
    int32_t amId;

    // All of the slot IDs inside the counter results structure for any
    //  monitors attached to this CU
    std::vector<uint32_t> aimIds;
    std::vector<uint32_t> asmIds;

    bool stall        = false;
    bool dataflow     = false;
    bool hasfa        = false;

    ComputeUnitInstance() = delete ;
  public:
    
    // Getters and setters
    inline const std::string& getName() { return name ; }
    inline const std::string& getKernelName() { return kernelName ; }

    inline int32_t getIndex() { return index ; }
    inline void setDim(int32_t x, int32_t y, int32_t z) { dim[0] = x; dim[1] = y; dim[2] = z; }
    XDP_EXPORT std::string getDim() ;

    XDP_EXPORT void addConnection(int32_t, int32_t);
    inline std::map<int32_t, std::vector<int32_t>>* getConnections()
    {  return &connections; }

    void    setAccelMon(int32_t id) { amId = id; }
    int32_t getAccelMon() { return amId; }
 
    void addAIM(uint32_t id) { aimIds.push_back(id); }
    void addASM(uint32_t id) { asmIds.push_back(id); }

    inline std::vector<uint32_t>* getAIMs() { return &aimIds; }
    inline std::vector<uint32_t>* getASMs() { return &asmIds; }

    void setStallEnabled(bool b) { stall = b; }
    bool stallEnabled() { return stall; }

    bool streamEnabled() { return (asmIds.empty() ? false : true); }

    void setDataflowEnabled(bool b) { dataflow = b; }
    bool dataflowEnabled() { return dataflow; }

    bool dataTransferEnabled() { return (aimIds.empty() ? false : true); }

    void setFaEnabled(bool b) { hasfa = b; }
    bool faEnabled() { return hasfa; }

    XDP_EXPORT ComputeUnitInstance(int32_t i, const std::string &n);
    XDP_EXPORT ~ComputeUnitInstance() ;
  } ;

  struct Memory {
    uint8_t     type;
    int32_t     index;
    uint64_t    baseAddress;
    uint64_t    size;
    std::string name;
    bool used ;

  Memory(uint8_t ty, int32_t idx, uint64_t baseAddr, uint64_t sz, const char* n, bool u)
      : type(ty),
        index(idx),
        baseAddress(baseAddr),
        size(sz),
        name(n),
        used(u)
    {}
  };

  struct XclbinInfo {
    // Choose reasonable defaults
    double maxReadBW = 0.0 ;
    double maxWriteBW = 0.0 ;
    double clockRateMHz = 300 ;
    bool usesTs2mm = false ;
    bool hasFloatingAIM = false;
    bool hasFloatingASM = false;
    DeviceIntf* deviceIntf = nullptr;

    xrt_core::uuid uuid ; 
    std::string name ;
    std::map<int32_t, ComputeUnitInstance*> cus ;
    std::map<int32_t, Memory*> memoryInfo ;

    /* Maps for AM, AIM, ASM Monitor (enabled for trace) with slotID as the key.
     * Contains only user space monitors, but no shell monitor (e.g. shell AIM/ASM)
     */
    std::map<uint64_t, Monitor*>  amMap;
    std::map<uint64_t, Monitor*>  aimMap;
    std::map<uint64_t, Monitor*>  asmMap;

    std::vector<Monitor*>  noTraceAMs;
    std::vector<Monitor*>  noTraceAIMs;
    std::vector<Monitor*>  noTraceASMs;

    // These contain all of the instances. This is necessary for counters.
    std::vector<Monitor*> aimList;
    std::vector<Monitor*> amList;
    std::vector<Monitor*> asmList;
    std::vector<Monitor*> nocList;

    uint32_t numTracePLIO = 0;

    ~XclbinInfo() {
      for (auto& i : cus) {
        delete i.second ;
      }
      cus.clear() ;
      for (auto& i : memoryInfo) {
        delete i.second ;
      }
      memoryInfo.clear() ;

      // Clear the map with monitors enabled for trace
      amMap.clear();
      aimMap.clear();
      asmMap.clear();

      // Clear the list with monitors not enabled trace
      noTraceAMs.clear();
      noTraceAIMs.clear();
      noTraceASMs.clear();

      // Delete the monitors using the all inclusive monitor list
      for(auto& i : amList) {
        delete i;
      }
      amList.clear();
      for(auto& i : aimList) {
        delete i;
      }
      aimList.clear();
      for(auto& i : asmList) {
        delete i;
      }
      asmList.clear();

      for(auto& i : nocList) {
        delete i;
      }
      nocList.clear();

      if (deviceIntf) delete deviceIntf ;
    }
  } ;

  struct DeviceInfo {
    // ****** Known information regardless of loaded XCLBIN ******
    uint64_t deviceId ;
    std::string deviceName ;
    std::string ctxInfo ;
    uint64_t kdmaCount     = 0 ;
    bool isEdgeDevice      = false ;
    bool isReady           = false;
    bool isAIEcounterRead  = false;
    bool isGMIORead        = false;

    // ****** Information specific all previously loaded XCLBINs ******
    std::vector<XclbinInfo*> loadedXclbins ;

    // ****** Information specific to the currently loaded XCLBIN ******

    std::vector<AIECounter*>     aieList;
    std::vector<TraceGMIO*>      gmioList;
    std::map<uint32_t, uint32_t> aieCountersMap;
    std::map<uint32_t, uint32_t> aieCoreEventsMap;
    std::map<uint32_t, uint32_t> aieMemoryEventsMap;
    std::vector<std::unique_ptr<aie_cfg_tile>> aieCfgList;

    XclbinInfo* currentXclbin() {
      if (loadedXclbins.size() <= 0)
        return nullptr ;
      return loadedXclbins.back() ;
    }

    void addXclbin(XclbinInfo* xclbin) {
      if (loadedXclbins.size() > 0) {
        if (loadedXclbins.back()->deviceIntf != nullptr) {
          delete loadedXclbins.back()->deviceIntf ;
          loadedXclbins.back()->deviceIntf = nullptr ;
        }
      }
      loadedXclbins.push_back(xclbin) ;
    }

    bool hasFloatingAIM(XclbinInfo* xclbin) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->hasFloatingAIM ;
      }
      return false ;
    }

    bool hasFloatingASM(XclbinInfo* xclbin) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->hasFloatingASM ;
      }
      return false ;
    }

    inline uint64_t getNumAM(XclbinInfo* xclbin) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->amMap.size() ;
      }
      return 0 ;
    }

    inline uint64_t getNumAIM(XclbinInfo* xclbin) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->aimMap.size() ;
      }
      return 0 ;
    }

    inline uint64_t getNumASM(XclbinInfo* xclbin) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->asmMap.size() ;
      }
      return 0 ;
    }

    inline uint64_t getNumNOC(XclbinInfo* xclbin) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->nocList.size() ;
      }
      return 0 ;
    }

    inline Monitor* getAMonitor(XclbinInfo* xclbin, uint64_t slotID) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->amMap[slotID] ;
      }
      return nullptr ;
    }

    inline Monitor* getAIMonitor(XclbinInfo* xclbin, uint64_t slotID) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->aimMap[slotID] ;
      }
      return nullptr ;
    }

    inline Monitor* getASMonitor(XclbinInfo* xclbin, uint64_t slotID) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->asmMap[slotID] ;
      }
      return nullptr ;
    }

    inline Monitor* getNOC(XclbinInfo* xclbin, uint64_t idx) {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return bin->nocList[idx] ;
      }
      return nullptr ;
    }

    // Includes only User Space AIM enabled for trace, but no shell AIM
    inline std::map<uint64_t, Monitor*>* getAIMonitors(XclbinInfo* xclbin)
    {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return &(bin->aimMap) ;
      }
      return nullptr ;
    }

    // Includes only User Space ASM enabled for trace, but no shell ASM
    inline std::map<uint64_t, Monitor*>* getASMonitors(XclbinInfo* xclbin)
    {
      for (auto bin : loadedXclbins) {
        if (bin == xclbin) return &(bin->asmMap) ;
      }
      return nullptr ;
    }

    inline xrt_core::uuid currentXclbinUUID() {
      if (loadedXclbins.size() <= 0)
        return xrt_core::uuid() ;
      return loadedXclbins.back()->uuid ; 
    }

    std::vector<XclbinInfo*> getLoadedXclbins() {
      return loadedXclbins ;
    }

    void cleanCurrentXclbinInfo() {
      for(auto i : aieList) {
        delete i;
      }
      aieList.clear();
      for(auto i : gmioList) {
        delete i;
      }
      gmioList.clear();
    }

    bool hasDMAMonitor() {
      if(!currentXclbin()) {
        return false;
      }
      for (auto aim : currentXclbin()->aimList) {
        if (aim->name.find("Host to Device") != std::string::npos)
          return true ;
      }
      return false ;
    }

    bool hasDMABypassMonitor() {
      if(!currentXclbin()) {
        return false;
      }
      for (auto aim : currentXclbin()->aimList) {
        if (aim->name.find("Peer to Peer") != std::string::npos)
          return true ;
      }
      return false ;
    }

    bool hasKDMAMonitor() {
      if(!currentXclbin()) {
        return false;
      }
      for (auto aim : currentXclbin()->aimList) {
        if (aim->name.find("Memory to Memory") != std::string::npos)
          return true ;
      }
      return false ;
    }

    ~DeviceInfo();

    void addTraceGMIO(uint32_t i, uint16_t col, uint16_t num, uint16_t stream,
          uint16_t len) ;
    void addAIECounter(uint32_t i, uint16_t col, uint16_t r, uint8_t num,
           uint8_t start, uint8_t end, uint8_t reset,
           double freq, const std::string& mod,
           const std::string& aieName) ;
    void addAIECounterResources(uint32_t numCounters, uint32_t numTiles) {
      aieCountersMap[numCounters] = numTiles;
    }
    void addAIECoreEventResources(uint32_t numEvents, uint32_t numTiles) {
      aieCoreEventsMap[numEvents] = numTiles;
    }
    void addAIEMemoryEventResources(uint32_t numEvents, uint32_t numTiles) {
      aieMemoryEventsMap[numEvents] = numTiles;
    }
    void addAIECfgTile(std::unique_ptr<aie_cfg_tile>& tile) {
      aieCfgList.push_back(std::move(tile));
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
    uint64_t applicationStartTime ;
    
    // The files that need to be included in the run summary for
    //  consumption by Vitis_Analyzer
    std::vector<std::pair<std::string, std::string> > openedFiles ;
    std::string systemDiagram ;

    // ***** OpenCL Information ******
    std::set<uint64_t> commandQueueAddresses ;
    std::set<std::string> enqueuedKernels ; 
    std::map<uint64_t, uint64_t> contextIdToNumDevices ;

    // For OpenCL software emulation, we need a tiny bit of device info
    std::string softwareEmulationDeviceName ; 
    std::map<std::string, uint64_t> softwareEmulationCUCounts ;
    std::map<std::string, bool> softwareEmulationMemUsage ;
    std::vector<std::string> softwareEmulationPortBitWidths ;

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

    bool resetDeviceInfo(uint64_t deviceId, const std::shared_ptr<xrt_core::device>& device);

    // Helper functions that fill in device information
    bool setXclbinName(XclbinInfo*, const std::shared_ptr<xrt_core::device>& device);
    bool initializeComputeUnits(XclbinInfo*, const std::shared_ptr<xrt_core::device>&);
    bool initializeProfileMonitors(DeviceInfo*, const std::shared_ptr<xrt_core::device>&);

  public:
    VPStaticDatabase(VPDatabase* d) ;
    ~VPStaticDatabase() ;

    XDP_EXPORT bool validXclbin(void* devHandle) ;
    inline double earliestSupportedToolVersion() { return 2019.2 ; }

    // Getters and setters
    inline int getPid() { return pid ; }
    inline uint64_t getApplicationStartTime() { return applicationStartTime ; }
    inline void setApplicationStartTime(uint64_t t)
      { applicationStartTime = t ; }
    inline std::vector<std::pair<std::string, std::string>>& getOpenedFiles() 
      { return openedFiles ; }
    inline std::string getSystemDiagram() { return systemDiagram ; }
    inline std::set<uint64_t>& getCommandQueueAddresses() 
      { return commandQueueAddresses ; }
    inline std::set<std::string>& getEnqueuedKernels()
      { return enqueuedKernels ; }
    inline std::string getSoftwareEmulationDeviceName()
      { return softwareEmulationDeviceName ; }
    inline void setSoftwareEmulationDeviceName(const std::string& name)
      { softwareEmulationDeviceName = name ; }
    inline std::map<std::string, uint64_t> getSoftwareEmulationCUCounts()
      { return softwareEmulationCUCounts ; }
    inline void addSoftwareEmulationCUInstance(const std::string& kernelName) {
      if (softwareEmulationCUCounts.find(kernelName) == softwareEmulationCUCounts.end())
        softwareEmulationCUCounts[kernelName] = 1 ;
      else
        softwareEmulationCUCounts[kernelName] += 1 ;
    }
    inline std::map<std::string, bool>& getSoftwareEmulationMemUsage()
      { return softwareEmulationMemUsage ;}
    inline void addSoftwareEmulationMemUsage(const std::string& mem, bool used)
      { softwareEmulationMemUsage[mem] = used ; }
    inline std::vector<std::string>& getSoftwareEmulationPortBitWidths()
      { return softwareEmulationPortBitWidths ; }
    inline void addSoftwareEmulationPortBitWidth(const std::string& s)
      { softwareEmulationPortBitWidths.push_back(s) ; }
    inline void setNumDevices(uint64_t contextId, uint64_t numDevices)
    {
      contextIdToNumDevices[contextId] = numDevices ;
    }
    inline uint64_t getNumDevices(uint64_t contextId)
    {
      if (contextIdToNumDevices.find(contextId) == contextIdToNumDevices.end())
        return 0 ;
      return contextIdToNumDevices[contextId] ;
    }

    inline DeviceInfo* getDeviceInfo(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId];
    }

    inline XclbinInfo* getCurrentlyLoadedXclbin(uint64_t deviceId)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr ;
      return deviceInfo[deviceId]->currentXclbin() ;
    }

    inline void deleteCurrentlyUsedDeviceInterface(uint64_t deviceId)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return ;
      XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
      if (!xclbin) return ;
      if (xclbin->deviceIntf) {
        delete xclbin->deviceIntf ;
        xclbin->deviceIntf = nullptr ;
      }
    }

    bool isDeviceReady(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false; 
      return deviceInfo[deviceId]->isReady;
    }

    bool isAIECounterRead(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false; 
      return deviceInfo[deviceId]->isAIEcounterRead;
    }

    void setIsAIECounterRead(uint64_t deviceId, bool val)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return; 
      deviceInfo[deviceId]->isAIEcounterRead = val;
    }

    void setIsGMIORead(uint64_t deviceId, bool val)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return; 
      deviceInfo[deviceId]->isGMIORead = val;
    }

    bool isGMIORead(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false; 
      return deviceInfo[deviceId]->isGMIORead;
    }

    double getClockRateMHz(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 300; 
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return 300 ;
      return deviceInfo[deviceId]->loadedXclbins.back()->clockRateMHz;
      //return deviceInfo[deviceId]->clockRateMHz;
    }

    void setDeviceName(uint64_t deviceId, const std::string& name)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return; 
      deviceInfo[deviceId]->deviceName = name;
    }
    std::string getDeviceName(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return std::string(""); 
      return deviceInfo[deviceId]->deviceName; 
    }

    void setDeviceIntf(uint64_t deviceId, DeviceIntf* devIntf)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return; 
      if (deviceInfo[deviceId]->currentXclbin() == nullptr)
        return ;
      deviceInfo[deviceId]->currentXclbin()->deviceIntf = devIntf ;
    }

    DeviceIntf* getDeviceIntf(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      if (deviceInfo[deviceId]->currentXclbin() == nullptr)
        return nullptr;
      return deviceInfo[deviceId]->currentXclbin()->deviceIntf; 
    }

    void setKDMACount(uint64_t deviceId, uint64_t num)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return;
      deviceInfo[deviceId]->kdmaCount = num;
    }
    uint64_t getKDMACount(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->kdmaCount; 
    }

    void setMaxReadBW(uint64_t deviceId, double bw)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end()) return ;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0) return ;
      deviceInfo[deviceId]->loadedXclbins.back()->maxReadBW = bw ;
      //deviceInfo[deviceId]->maxReadBW = bw ;
    }

    double getMaxReadBW(uint64_t deviceId)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end()) return 0 ;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0) return 0 ;
      return deviceInfo[deviceId]->loadedXclbins.back()->maxReadBW ;
      //return deviceInfo[deviceId]->maxReadBW ;
    }

    void setMaxWriteBW(uint64_t deviceId, double bw)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end()) return ;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0) return ;
      deviceInfo[deviceId]->loadedXclbins.back()->maxWriteBW = bw ;
      //deviceInfo[deviceId]->maxWriteBW = bw ;
    }

    double getMaxWriteBW(uint64_t deviceId)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end()) return 0 ;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0) return 0 ;
      return deviceInfo[deviceId]->loadedXclbins.back()->maxWriteBW ;
      //return deviceInfo[deviceId]->maxWriteBW ;
    }

    std::string getXclbinName(uint64_t deviceId) { 
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return std::string(""); 
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return std::string("") ;
      return deviceInfo[deviceId]->loadedXclbins.back()->name; 
      //return deviceInfo[deviceId]->loadedXclbin; 
    }

    std::vector<XclbinInfo*> getLoadedXclbins(uint64_t deviceId) {
      if (deviceInfo.find(deviceId) == deviceInfo.end()) {
        std::vector<XclbinInfo*> blank ;
        return blank ;
      }
      return deviceInfo[deviceId]->getLoadedXclbins() ;
    }

    ComputeUnitInstance* getCU(uint64_t deviceId, int32_t cuId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return nullptr ;
      return deviceInfo[deviceId]->loadedXclbins.back()->cus[cuId] ;
      //return deviceInfo[deviceId]->cus[cuId];
    }

    inline std::map<int32_t, ComputeUnitInstance*>* getCUs(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return nullptr ;
      return &(deviceInfo[deviceId]->loadedXclbins.back()->cus) ;
      //return &(deviceInfo[deviceId]->cus);
    }

    inline std::map<int32_t, Memory*>* getMemoryInfo(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return nullptr ;
      return &(deviceInfo[deviceId]->loadedXclbins.back()->memoryInfo) ;
      //return &(deviceInfo[deviceId]->memoryInfo);
    }

    Memory* getMemory(uint64_t deviceId, int32_t memId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return nullptr ;
      return deviceInfo[deviceId]->loadedXclbins.back()->memoryInfo[memId] ;
      //return deviceInfo[deviceId]->memoryInfo[memId];
    }

    // Includes only User Space AM enabled for trace
    inline uint64_t getNumAM(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->getNumAM(xclbin);
    }

    /* Includes only User Space AIM enabled for trace; but no shell AIM
     * Note : May not match xdp::DeviceIntf::getNumMonitors(XCL_PERF_MON_MEMORY)
     *        as that includes both user-space and shell AIM
     */
    inline uint64_t getNumAIM(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->getNumAIM(xclbin);
    }

    /* Includes only User Space ASM enabled for trace; but no shell ASM
     * Note : May not match xdp::DeviceIntf::getNumMonitors(XCL_PERF_MON_STR)
     *        as that includes both user-space and shell ASM
     */
    inline uint64_t getNumASM(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->getNumASM(xclbin);
    }

    inline uint64_t getNumNOC(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      return deviceInfo[deviceId]->getNumNOC(xclbin);
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

    inline Monitor* getAMonitor(uint64_t deviceId, XclbinInfo* xclbin, uint64_t slotID)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->getAMonitor(xclbin, slotID) ;
    }

    inline Monitor* getAIMonitor(uint64_t deviceId, XclbinInfo* xclbin, uint64_t slotID)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->getAIMonitor(xclbin, slotID);
    }

    inline Monitor* getASMonitor(uint64_t deviceId, XclbinInfo* xclbin, uint64_t slotID)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->getASMonitor(xclbin, slotID);
    }

    inline Monitor* getNOC(uint64_t deviceId, XclbinInfo* xclbin, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->getNOC(xclbin, idx);
    }

    inline AIECounter* getAIECounter(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;

      if(deviceInfo[deviceId]->aieList.size() <= idx)
        return nullptr;

      return deviceInfo[deviceId]->aieList[idx];
    }

    inline std::map<uint32_t, uint32_t>&
    getAIECounterResources(uint64_t deviceId)
    {
      return deviceInfo[deviceId]->aieCountersMap;
    }

    inline std::map<uint32_t, uint32_t>&
    getAIECoreEventResources(uint64_t deviceId)
    {
      return deviceInfo[deviceId]->aieCoreEventsMap;
    }

    inline std::map<uint32_t, uint32_t>&
    getAIEMemoryEventResources(uint64_t deviceId)
    {
      return deviceInfo[deviceId]->aieMemoryEventsMap;
    }

    inline std::vector<std::unique_ptr<aie_cfg_tile>>&
      getAIECfgTiles(uint64_t deviceId)
    {
      return deviceInfo[deviceId]->aieCfgList;
    }

    inline TraceGMIO* getTraceGMIO(uint64_t deviceId, uint64_t idx)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->gmioList[idx];
    }

    inline void addTraceGMIO(uint64_t deviceId, uint32_t i, uint16_t col,
           uint16_t num, uint16_t stream, uint16_t len)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
  return ;
      deviceInfo[deviceId]->addTraceGMIO(i, col, num, stream, len);
    }

    inline void addAIECounter(uint64_t deviceId, uint32_t i, uint16_t col,
            uint16_t r, uint8_t num, uint8_t start,
            uint8_t end, uint8_t reset, double freq,
            const std::string& mod,
            const std::string& aieName)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return ;
      deviceInfo[deviceId]->addAIECounter(i, col, r, num, start, end, reset,
                freq, mod, aieName) ;
    }

    inline void addAIECounterResources(uint64_t deviceId, uint32_t numCounters, uint32_t numTiles) {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return ;
      deviceInfo[deviceId]->addAIECounterResources(numCounters, numTiles) ;
    }
    
    inline void addAIECoreEventResources(uint64_t deviceId, uint32_t numEvents, uint32_t numTiles) {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return ;
      deviceInfo[deviceId]->addAIECoreEventResources(numEvents, numTiles) ;
    }
    
    inline void addAIEMemoryEventResources(uint64_t deviceId, uint32_t numEvents, uint32_t numTiles) {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return ;
      deviceInfo[deviceId]->addAIEMemoryEventResources(numEvents, numTiles) ;
    }

    inline void addAIECfgTile(uint64_t deviceId, std::unique_ptr<aie_cfg_tile>& tile)
    {
      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return ;
      deviceInfo[deviceId]->addAIECfgTile(tile) ;
    }

    // Includes only User Space AIM enabled for trace, but no shell AIM
    inline std::map<uint64_t, Monitor*>* getAIMonitors(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->getAIMonitors(xclbin);
    }

    // Includes only User Space ASM enabled for trace, but no shell ASM
    inline std::map<uint64_t, Monitor*>* getASMonitors(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return nullptr;
      return deviceInfo[deviceId]->getASMonitors(xclbin) ;
    }

    inline void getDataflowConfiguration(uint64_t deviceId, bool* config, size_t size)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return ;

      size_t count = 0;
      /* User space AM in sorted order of their slotIds.
       * Matches with sorted list of AM in xdp::DeviceIntf
       */   
      for(auto mon : deviceInfo[deviceId]->loadedXclbins.back()->amList) {
        if(count >= size)
          return;
        auto cu = deviceInfo[deviceId]->loadedXclbins.back()->cus[mon->cuIndex];
        config[count] = cu->dataflowEnabled();
        ++count;
      }
    }

    // For profile summary information, we have to aggregate information
    //  from all devices.
    inline uint64_t getNumDevices() { return deviceInfo.size() ; }
    XDP_EXPORT std::vector<std::string> getDeviceNames() ;
    XDP_EXPORT std::vector<DeviceInfo*> getDeviceInfos() ;
    XDP_EXPORT bool hasStallInfo() ;

    inline void getFaConfiguration(uint64_t deviceId, bool* config, size_t size)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return;
      if (deviceInfo[deviceId]->loadedXclbins.size() <= 0)
        return ;

      size_t count = 0;
      /* User space AM in sorted order of their slotIds.
       * Matches with sorted list of AM in xdp::DeviceIntf
       */   
      for(auto mon : deviceInfo[deviceId]->loadedXclbins.back()->amList) {
        if(count >= size)
          return;
        auto cu = deviceInfo[deviceId]->loadedXclbins.back()->cus[mon->cuIndex];
        config[count] = cu->faEnabled();
        ++count;
      }
    }

    inline std::string getCtxInfo(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return "";
      return deviceInfo[deviceId]->ctxInfo;
    }

    inline bool hasFloatingAIM(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false;
      return deviceInfo[deviceId]->hasFloatingAIM(xclbin);
    }

    inline bool hasFloatingASM(uint64_t deviceId, XclbinInfo* xclbin)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return false;
      return deviceInfo[deviceId]->hasFloatingASM(xclbin);
    }

    inline uint64_t getNumTracePLIO(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;
      if(deviceInfo[deviceId]->loadedXclbins.empty())
        return 0;
      return deviceInfo[deviceId]->loadedXclbins.back()->numTracePLIO;
    }

    inline uint64_t getNumAIETraceStream(uint64_t deviceId)
    {
      if(deviceInfo.find(deviceId) == deviceInfo.end())
        return 0;

      uint64_t numAIETraceStream = getNumTracePLIO(deviceId);
      if(numAIETraceStream)
        return numAIETraceStream;

      return deviceInfo[deviceId]->gmioList.size();
    }

    // Reseting device information whenever a new xclbin is added
    XDP_EXPORT void updateDevice(uint64_t deviceId, void* devHandle) ;

    // Functions that add information to the database
    XDP_EXPORT void addCommandQueueAddress(uint64_t a) ;
    XDP_EXPORT void addKDMACount(void* dev, uint16_t numKDMAs) ;

    XDP_EXPORT void addOpenedFile(const std::string& name, 
          const std::string& type) ;
    XDP_EXPORT void addEnqueuedKernel(const std::string& identifier) ;
  } ;

}

#endif
