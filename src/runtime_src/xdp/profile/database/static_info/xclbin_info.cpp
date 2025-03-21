/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "core/common/message.h"

namespace xdp {

  PLInfo& PLInfo::operator=(const PLInfo& src)
  {
    // Check for self assignment
    if (this == &src)
      return *this;

    // Release existing PLInfo resources
    releaseResources() ;

    this->hostMaxReadBW    = src.hostMaxReadBW ;
    this->hostMaxWriteBW   = src.hostMaxWriteBW ;
    this->kernelMaxReadBW  = src.kernelMaxReadBW ;
    this->kernelMaxWriteBW = src.kernelMaxWriteBW ;
    this->clockRatePLMHz = src.clockRatePLMHz ;

    this->usesTs2mm = src.usesTs2mm ;
    this->usesFifo = src.usesFifo ;
    this->hasFloatingAIMWithTrace = src.hasFloatingAIMWithTrace ;
    this->hasFloatingASMWithTrace = src.hasFloatingASMWithTrace ;
    this->hasMemoryAIM = src.hasMemoryAIM ;

    for (auto &cu : src.cus)
      this->cus[cu.first] = new ComputeUnitInstance(*cu.second) ;

    for (auto &mi : src.memoryInfo)
      this->memoryInfo[mi.first] = new Memory(*mi.second) ;
    
    this->ams.reserve(src.ams.size()) ;
    for (auto& am : src.ams)
      this->ams.push_back(new Monitor(*am)) ;

    this->aims.reserve(src.aims.size()) ;
    for (auto& aim : src.aims)
      this->aims.push_back(new Monitor(*aim)) ;

    this->asms.reserve(src.asms.size()) ;
    for (auto& asmPtr : src.asms)
      this->asms.push_back(new Monitor(*asmPtr)) ;

    return *this ;
  }

  PLInfo::~PLInfo()
  {
    releaseResources();
  }

  void PLInfo::releaseResources()
  {
    for (auto& i : cus)
      delete i.second ;
    cus.clear();

    for (auto& i : memoryInfo)
      delete i.second ;
    memoryInfo.clear();

    for (auto i : ams)
      delete i ;
    ams.clear();

    for (auto i : aims)
      delete i ;
    aims.clear();

    for (auto i : asms)
      delete i ;
    asms.clear();
  }

  std::vector<ComputeUnitInstance*>
  PLInfo::collectCUs(const std::string& kernelName)
  {
    std::vector<ComputeUnitInstance*> collected;

    for (auto& iter : cus) {
      auto instance = iter.second;
      if (instance->getKernelName() == kernelName)
        collected.push_back(instance);
    }
    return collected;
  }

  void PLInfo::addComputeUnitPorts(const std::string& kernelName,
                                   const std::string& portName,
                                   int32_t portWidth)
  {
    for (const auto& iter : cus) {
      auto cu = iter.second;
      if (cu->getKernelName() == kernelName)
        cu->addPort(portName, portWidth);
    }
  }

  void PLInfo::addArgToPort(const std::string& kernelName,
                            const std::string& argName,
                            const std::string& portName)
  {
    for (const auto& iter : cus) {
      auto cu = iter.second;
      if (cu->getKernelName() == kernelName)
        cu->addArgToPort(argName, portName);
    }
  }

  void PLInfo::connectArgToMemory(const std::string& cuName,
                                  const std::string& portName,
                                  const std::string& argName,
                                  int32_t memId)
  {
    if (memoryInfo.find(memId) == memoryInfo.end())
      return;

    Memory* mem = memoryInfo[memId];
    for (const auto& iter : cus) {
      auto cu = iter.second;
      if (cu->getName() == cuName)
        cu->connectArgToMemory(portName, argName, mem);
    }
  }

  AIEInfo& AIEInfo::operator=(const AIEInfo& src)
  {
    // Check for self assignment
    if (this == &src)
      return *this ;

    // Release existing PLInfo resources
    releaseResources() ;

    this->clockRateAIEMHz = src.clockRateAIEMHz ;
    this->numTracePLIO = src.numTracePLIO ;
    this->isGMIORead = src.isGMIORead ;
    this->isAIEcounterRead = src.isAIEcounterRead ;

    for (auto &aie : src.aieList)
      this->aieList.push_back(new AIECounter(*aie)) ;

    for (auto &gmio : src.gmioList)
      this->gmioList.push_back(new TraceGMIO(*gmio)) ;

    this->aieCoreCountersMap    = src.aieCoreCountersMap ;
    this->aieShimCountersMap    = src.aieShimCountersMap ;
    this->aieMemTileCountersMap = src.aieMemTileCountersMap ;
    this->aieCoreEventsMap      = src.aieCoreEventsMap ;
    this->aieMemoryEventsMap    = src.aieMemoryEventsMap ;
    this->aieShimEventsMap      = src.aieShimEventsMap ;
    this->aieMemTileEventsMap   = src.aieMemTileEventsMap ;

    for (auto &tile : src.aieCfgList)
      this->aieCfgList.push_back(std::make_unique<aie_cfg_tile>(*tile)) ;

    for (auto &noc : src.nocList)
      this->nocList.push_back(new NoCNode(*noc)) ;

    return *this ;
  }

  AIEInfo::~AIEInfo()
  {
    releaseResources();
  }

  void AIEInfo::releaseResources()
  {
    for (auto i : aieList)
      delete i ;
    aieList.clear();

    for (auto i: gmioList)
      delete i;
    gmioList.clear();

    for (auto i : nocList)
      delete i;

    // release aie_cfg_tile unique pointers
    aieCfgList.clear() ;
  }

  XclbinInfo::XclbinInfo(XclbinInfoType xclbinType) : type(xclbinType)
  {
      if (xclbinType == XclbinInfoType::XCLBIN_PL_ONLY) {
        pl.valid  = true;
        aie.valid = false;
      } else if (xclbinType == XclbinInfoType::XCLBIN_AIE_ONLY) {
        pl.valid  = false;
        aie.valid = true;
      }
  }

  ConfigInfo::ConfigInfo(XclbinInfo* xclbin) : type(CONFIG_AIE_PL)
  {
    currentXclbins.push_back(xclbin);
  }

  ConfigInfo::~ConfigInfo()
  {
    for (auto xclbin : currentXclbins)
      delete xclbin;
    currentXclbins.clear();

    if (plDeviceIntf) {
      delete plDeviceIntf;
      plDeviceIntf = nullptr;
    }
  }

  xrt_core::uuid ConfigInfo::getConfigUuid()
  {
    if (currentXclbins.size()==1)
      return currentXclbins.back()->uuid;

    std::string mix_uuid_str;
    for (auto xclbin : currentXclbins)
      mix_uuid_str += xclbin->uuid.to_string();
    
    return xrt_core::uuid(mix_uuid_str);
  }

  void ConfigInfo::addXclbin(XclbinInfo* xclbin)
  {
    currentXclbins.push_back(xclbin);
  }

  bool ConfigInfo::containsXclbin(xrt_core::uuid& uuid)
  {
    for (auto xclbin : currentXclbins)
    {
      if (xclbin->uuid == uuid)
        return true;
    }
    
    return false;
  }

  bool ConfigInfo::containsXclbinType(XclbinInfoType& xclbinQueryType)
  {
    for (auto xclbin : currentXclbins)
    {
      if (xclbin->type == xclbinQueryType)
        return true;
    }

    return false;
  }

  XclbinInfo* ConfigInfo::getPlXclbin()
  {
    for (auto xclbin : currentXclbins)
    {
      if (xclbin->pl.valid)
        return xclbin;
    }
    return nullptr;
  }

  XclbinInfo* ConfigInfo::getAieXclbin()
  {
    for (auto xclbin : currentXclbins)
    {
      if (xclbin->aie.valid)
        return xclbin;
    }
    return nullptr;
  }

  std::string ConfigInfo::getXclbinNames()
  {
    std::string name = "";
    if (!currentXclbins.empty()) {
      name += currentXclbins.front()->name;
      for (size_t i=1; i<currentXclbins.size(); i++)
        name += ", " + currentXclbins[i]->name;
    }
    return name ;
  }

  bool ConfigInfo::isAiePlusPl()
  {
    if (type == CONFIG_AIE_PL || type == CONFIG_AIE_PL_FORMED)
      return true;
    return false; 
  }
  
  bool ConfigInfo::isAieOnly()
  {
    return type == CONFIG_AIE_ONLY;
  }

  bool ConfigInfo::isPlOnly()
  {
    return type == CONFIG_PL_ONLY;
  }

  bool ConfigInfo::hasXclbin(XclbinInfo* xclbin)
  {
    for (auto bin : currentXclbins) {
      if (bin == xclbin)
        return true;
    }
    return false;
  }

    bool ConfigInfo::hasFloatingAIMWithTrace(XclbinInfo* xclbin)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid)
          return bin->pl.hasFloatingAIMWithTrace ;
      }

      return false ;
    }

    bool ConfigInfo::hasFloatingASMWithTrace(XclbinInfo* xclbin)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid)
          return bin->pl.hasFloatingASMWithTrace ;
      }
      
      return false ;
    }

    uint64_t ConfigInfo::getNumAM(XclbinInfo* xclbin)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid)
          return bin->pl.ams.size() ;
      }

      return 0;
    }

    uint64_t ConfigInfo::getNumUserAMWithTrace(XclbinInfo* xclbin)
    {
      uint64_t num = 0;
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto am : bin->pl.ams) {
            if (am->traceEnabled)
              ++num ;
          }
        }
      }
      return num ;
    }

    uint64_t ConfigInfo::getNumAIM(XclbinInfo* xclbin)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin)
          return bin->pl.aims.size() ;
      }

      return 0 ;
    }

    uint64_t ConfigInfo::getNumUserAIM(XclbinInfo* xclbin)
    {
      uint64_t num = 0;
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto aim : bin->pl.aims) {
            if (!aim->isShellMonitor())
              ++num ;
          }
        }
      }
      return num ;
    }

    uint64_t ConfigInfo::getNumUserAIMWithTrace(XclbinInfo* xclbin) const
    {
      uint64_t num = 0;
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto aim : bin->pl.aims) {
            if (aim->traceEnabled && !aim->isShellMonitor())
              ++num ;
          }
        }
      }

      return num ;
    }

    uint64_t ConfigInfo::getNumASM(XclbinInfo* xclbin) const
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin)
          return bin->pl.asms.size() ;
      }
      return 0 ;
    }

    uint64_t ConfigInfo::getNumUserASM(XclbinInfo* xclbin) const
    {
      uint64_t num = 0;
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto mon : bin->pl.asms) {
            if (!mon->isShellMonitor())
              ++num;
          }
        }
      }
      return num ;
    }

    uint64_t ConfigInfo::getNumUserASMWithTrace(XclbinInfo* xclbin)
    {
      uint64_t num = 0;
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto mon : bin->pl.asms) {
            if (mon->traceEnabled && !mon->isShellMonitor())
              ++num;
          }
        }
      }
      return num ;
    }

    uint64_t ConfigInfo::getNumNOC(XclbinInfo* xclbin)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin)
          return bin->aie.nocList.size() ;
      }
      return 0 ;
    }

    Monitor* ConfigInfo::getAMonitor(XclbinInfo* xclbin, uint64_t slotId)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto am : bin->pl.ams) {
            if (am->slotIndex == slotId)
              return am ;
          }
        }
      }
      return nullptr ;
    }

    Monitor* ConfigInfo::getAIMonitor(XclbinInfo* xclbin, uint64_t slotId)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto aim : bin->pl.aims) {
            if (aim->slotIndex == slotId)
              return aim ;
          }
        }
      }
      return nullptr ;
    }

    Monitor* ConfigInfo::getASMonitor(XclbinInfo* xclbin, uint64_t slotId)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto streamMonitor : bin->pl.asms) {
            if (streamMonitor->slotIndex == slotId)
              return streamMonitor ;
          }
        }
      }
      return nullptr ;
    }

    NoCNode* ConfigInfo::getNOC(XclbinInfo* xclbin, uint64_t idx)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->aie.valid) {
          if (bin->aie.nocList.size() <= idx)
            return nullptr;
          return bin->aie.nocList[idx] ;
        }
      }
      return nullptr ;
    }

    std::vector<Monitor*>* ConfigInfo::getAIMonitors(XclbinInfo* xclbin)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin)
          return &(bin->pl.aims) ;
      }
      return nullptr ;
    }

    std::vector<Monitor*>* ConfigInfo::getASMonitors(XclbinInfo* xclbin)
    {
      for (auto bin : currentXclbins) {
        if (bin == xclbin)
          return &(bin->pl.asms) ;
      }
      return nullptr ;
    }

    std::vector<Monitor*> ConfigInfo::getUserAIMsWithTrace(XclbinInfo* xclbin)
    {
      std::vector<Monitor*> constructed ;
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto aim : bin->pl.aims) {
            if (aim->traceEnabled && !aim->isShellMonitor())
              constructed.push_back(aim) ;
          }
        }
      }
      return constructed ;
    }

    std::vector<Monitor*> ConfigInfo::getUserASMsWithTrace(XclbinInfo* xclbin)
    {
      std::vector<Monitor*> constructed ;
      for (auto bin : currentXclbins) {
        if (bin == xclbin && bin->pl.valid) {
          for (auto mon : bin->pl.asms) {
            if (mon->traceEnabled && !mon->isShellMonitor())
              constructed.push_back(mon) ;
          }
        }
      }
      return constructed ;
    }

    void ConfigInfo::addTraceGMIO(uint32_t id, uint8_t col, uint8_t num,
                                uint8_t stream, uint8_t len)
    {
      for (auto xclbin : currentXclbins)
      {
        if (xclbin->aie.valid)
        {
          xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Added GMIO trace of ID "+ std::to_string(id) + ".");
          xclbin->aie.gmioList.push_back(new TraceGMIO(id, col, num, stream, len)) ;
          return ;
        }
      }
    }

    void ConfigInfo::addAIECounter(uint32_t i, uint8_t col, uint8_t r,
                                   uint8_t num, uint16_t start, uint16_t end,
                                   uint8_t reset, uint64_t load, double freq,
                                   const std::string& mod,
                                   const std::string& aieName, uint8_t streamId)
    {
      for (auto xclbin : currentXclbins)
      {
        if (xclbin->aie.valid)
        {
          xclbin->aie.aieList.push_back(new AIECounter(i, col, r, num, start, end,
                                                       reset, load, freq, mod,
                                                       aieName,streamId)) ;
          return ;
        }
      }
    }

    void ConfigInfo::addAIECounterResources(uint32_t numCounters,
                                            uint32_t numTiles,
                                            uint8_t moduleType)
    {
      XclbinInfo* xclbin = nullptr ;
      for (auto bin : currentXclbins)
      {
        if (bin->aie.valid)
        {
          xclbin = bin;
          break ;
        }
      }

      if (!xclbin)
        return;

      switch (moduleType)
      {
        case module_type::core:
          xclbin->aie.aieCoreCountersMap[numCounters] = numTiles ;
          break ;
        case module_type::dma:
          xclbin->aie.aieMemoryCountersMap[numCounters] = numTiles ;
          break ;
        case module_type::shim:
          xclbin->aie.aieShimCountersMap[numCounters] = numTiles ;
          break ;
        default:
          xclbin->aie.aieMemTileCountersMap[numCounters] = numTiles ;
          break ;
      }
    }

    void ConfigInfo::addAIECoreEventResources(uint32_t numEvents,
                                              uint32_t numTiles)
    {
      for (auto xclbin : currentXclbins)
      {
        if (xclbin->aie.valid)
        {
          xclbin->aie.aieCoreEventsMap[numEvents] = numTiles ;
          break ;
        }
      }
    }

    void ConfigInfo::addAIEMemoryEventResources(uint32_t numEvents,
                                                uint32_t numTiles)
    {
      for (auto xclbin : currentXclbins)
      {
        if (xclbin->aie.valid)
        {
          xclbin->aie.aieMemoryEventsMap[numEvents] = numTiles ;
          break ;
        }
      }
    }

    void ConfigInfo::addAIEShimEventResources(uint32_t numEvents,
                                              uint32_t numTiles)
    {
      for (auto xclbin : currentXclbins)
      {
        if (xclbin->aie.valid)
        {
          xclbin->aie.aieShimEventsMap[numEvents] = numTiles ;
          break ;
        }
      }
    }

    void ConfigInfo::addAIEMemTileEventResources(uint32_t numEvents,
                                                 uint32_t numTiles)
    {
      for (auto xclbin : currentXclbins)
      {
        if (xclbin->aie.valid)
        {
          xclbin->aie.aieMemTileEventsMap[numEvents] = numTiles ;
          break ;
        }
      }

    }

    void ConfigInfo::addAIECfgTile(std::unique_ptr<aie_cfg_tile>&& tile)
    {
      for (auto xclbin : currentXclbins)
      {
        if (xclbin->aie.valid)
        {
          xclbin->aie.aieCfgList.push_back(std::move(tile)) ;
          break ;
        }
      }
    }

    void ConfigInfo::cleanCurrentXclbinInfos(XclbinInfoType xclbinType)
    {
      if (xclbinType == XCLBIN_AIE_ONLY)   {
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
                "Skipping the current config cleanup for new aie-only xclbin.");
        return;
      }

      for (auto xclbin : currentXclbins) {
        // Clean up AIE xclbin
        if (xclbin->aie.valid) {
          
          for (auto i : xclbin->aie.aieList)
            delete i ;
          xclbin->aie.aieList.clear() ;
          
          for (auto i : xclbin->aie.gmioList)
            delete i ;
          xclbin->aie.gmioList.clear() ;
          
          xclbin->aie.valid = false;
        }
      }
    }

    bool ConfigInfo::hasAIMNamed(const std::string& name)
    {
      for (auto xclbin : currentXclbins) {
        for (auto aim : xclbin->pl.aims) {
          if (aim->name.find(name) != std::string::npos)
            return true ;
        }
      }
      return false ;
    }
} // end namespace xdp
