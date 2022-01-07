/**
 * Copyright (C) 2021 Xilinx, Inc
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

#define XDP_SOURCE

#include "xdp/profile/database/static_info/device_info.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/device/device_intf.h"

namespace xdp {

  DeviceInfo::~DeviceInfo()
  {
    for (auto i : loadedXclbins) {
      delete i ;
    }
    loadedXclbins.clear() ;
  }

  std::string DeviceInfo::getUniqueDeviceName() const
  {
    return deviceName + "-" + std::to_string(deviceId) ;
  }

  xrt_core::uuid DeviceInfo::currentXclbinUUID()
  {
    if (loadedXclbins.size() <= 0)
      return xrt_core::uuid() ;
    return loadedXclbins.back()->uuid ;
  }

  XclbinInfo* DeviceInfo::currentXclbin()
  {
    if (loadedXclbins.size() <= 0)
      return nullptr ;
    return loadedXclbins.back() ;
  }

  void DeviceInfo::addXclbin(XclbinInfo* xclbin)
  {
    // When loading a new xclbin, we need to destroy any existing
    //  device interface.
    if (loadedXclbins.size() > 0) {
      if (loadedXclbins.back()->deviceIntf != nullptr) {
        delete loadedXclbins.back()->deviceIntf ;
        loadedXclbins.back()->deviceIntf = nullptr ;
      }
    }
    loadedXclbins.push_back(xclbin) ;
  }

  bool DeviceInfo::hasFloatingAIMWithTrace(XclbinInfo* xclbin) const
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return bin->pl.hasFloatingAIMWithTrace ;
    }
    return false ;
  }

  bool DeviceInfo::hasFloatingASMWithTrace(XclbinInfo* xclbin) const
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return bin->pl.hasFloatingASMWithTrace ;
    }
    return false ;
  }

  uint64_t DeviceInfo::getNumAM(XclbinInfo* xclbin) const
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return bin->pl.ams.size() ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumUserAMWithTrace(XclbinInfo* xclbin) const
  {
    uint64_t num = 0 ;
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto am : bin->pl.ams) {
          if (am->traceEnabled)
            ++num ;
        }
      }
    }
    return num ;
  }

  uint64_t DeviceInfo::getNumAIM(XclbinInfo* xclbin) const
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return bin->pl.aims.size() ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumUserAIM(XclbinInfo* xclbin) const
  {
    uint64_t num = 0 ;
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto aim : bin->pl.aims) {
          if (!aim->isShellMonitor())
            ++num ;
        }
      }
    }
    return num ;
  }

  uint64_t DeviceInfo::getNumUserAIMWithTrace(XclbinInfo* xclbin) const
  {
    uint64_t num = 0 ;
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto aim : bin->pl.aims) {
          if (aim->traceEnabled && !aim->isShellMonitor())
            ++num ;
        }
      }
    }
    return num ;
  }

  uint64_t DeviceInfo::getNumASM(XclbinInfo* xclbin) const
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return bin->pl.asms.size() ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumUserASM(XclbinInfo* xclbin) const
  {
    uint64_t num = 0 ;
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto mon : bin->pl.asms) {
          if (!mon->isShellMonitor())
            ++num ;
        }
      }
    }
    return num ;
  }

  uint64_t DeviceInfo::getNumUserASMWithTrace(XclbinInfo* xclbin) const
  {
    uint64_t num = 0 ;
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto mon : bin->pl.asms) {
          if (mon->traceEnabled && !mon->isShellMonitor())
            ++num ;
        }
      }
    }
    return num ;
  }

  uint64_t DeviceInfo::getNumNOC(XclbinInfo* xclbin) const
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return bin->aie.nocList.size() ;
    }
    return 0 ;
  }

  Monitor* DeviceInfo::getAMonitor(XclbinInfo* xclbin, uint64_t slotId)
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto am : bin->pl.ams) {
          if (am->slotIndex == slotId)
            return am ;
        }
      }
    }
    return nullptr ;
  }

  Monitor* DeviceInfo::getAIMonitor(XclbinInfo* xclbin, uint64_t slotId)
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto aim : bin->pl.aims) {
          if (aim->slotIndex == slotId)
            return aim ;
        }
      }
    }
    return nullptr ;
  }

  Monitor* DeviceInfo::getASMonitor(XclbinInfo* xclbin, uint64_t slotId)
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto streamMonitor : bin->pl.asms) {
          if (streamMonitor->slotIndex == slotId)
            return streamMonitor ;
        }
      }
    }
    return nullptr ;
  }

  NoCNode* DeviceInfo::getNOC(XclbinInfo* xclbin, uint64_t idx)
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        if (bin->aie.nocList.size() <= idx)
          return nullptr;
        return bin->aie.nocList[idx] ;
      }
    }
    return nullptr ;
  }

  std::vector<Monitor*>* DeviceInfo::getAIMonitors(XclbinInfo* xclbin)
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return &(bin->pl.aims) ;
    }
    return nullptr ;
  }

  std::vector<Monitor*>* DeviceInfo::getASMonitors(XclbinInfo* xclbin)
  {
    for (auto bin : loadedXclbins) {
      if (bin == xclbin)
        return &(bin->pl.asms) ;
    }
    return nullptr ;
  }

  std::vector<Monitor*> DeviceInfo::getUserAIMsWithTrace(XclbinInfo* xclbin)
  {
    std::vector<Monitor*> constructed ;
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto aim : bin->pl.aims) {
          if (aim->traceEnabled && !aim->isShellMonitor())
            constructed.push_back(aim) ;
        }
      }
    }
    return constructed ;
  }

  std::vector<Monitor*> DeviceInfo::getUserASMsWithTrace(XclbinInfo* xclbin)
  {
    std::vector<Monitor*> constructed ;
    for (auto bin : loadedXclbins) {
      if (bin == xclbin) {
        for (auto mon : bin->pl.asms) {
          if (mon->traceEnabled && !mon->isShellMonitor())
            constructed.push_back(mon) ;
        }
      }
    }
    return constructed ;
  }

  void DeviceInfo::addTraceGMIO(uint32_t id, uint16_t col, uint16_t num,
                                uint16_t stream, uint16_t len)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.gmioList.push_back(new TraceGMIO(id, col, num, stream, len)) ;
  }

  void DeviceInfo::addAIECounter(uint32_t i, uint16_t col, uint16_t r,
                                 uint8_t num, uint16_t start, uint16_t end,
                                 uint8_t reset, uint32_t load, double freq,
                                 const std::string& mod,
				 const std::string& aieName)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.aieList.push_back(new AIECounter(i, col, r, num, start, end,
                                                 reset, load, freq, mod, aieName)) ;
  }

  void DeviceInfo::addAIECounterResources(uint32_t numCounters,
                                          uint32_t numTiles,
                                          uint8_t moduleType)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    switch (moduleType) {
    case 0:
      xclbin->aie.aieCoreCountersMap[numCounters] = numTiles ;
      break ;
    case 1:
      xclbin->aie.aieMemoryCountersMap[numCounters] = numTiles ;
      break ;
    default:
      xclbin->aie.aieShimCountersMap[numCounters] = numTiles ;
      break ;
    }
  }

  void DeviceInfo::addAIECoreEventResources(uint32_t numEvents,
                                            uint32_t numTiles)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.aieCoreEventsMap[numEvents] = numTiles ;
  }

  void DeviceInfo::addAIEMemoryEventResources(uint32_t numEvents,
                                              uint32_t numTiles)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.aieMemoryEventsMap[numEvents] = numTiles ;
  }

  void DeviceInfo::addAIEShimEventResources(uint32_t numEvents,
                                            uint32_t numTiles)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.aieShimEventsMap[numEvents] = numTiles ;
  }

  void DeviceInfo::addAIECfgTile(std::unique_ptr<aie_cfg_tile>& tile)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.aieCfgList.push_back(std::move(tile)) ;
  }

  void DeviceInfo::cleanCurrentXclbinInfo()
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return ;

    for (auto i : xclbin->aie.aieList) {
      delete i ;
    }
    xclbin->aie.aieList.clear() ;
    for (auto i : xclbin->aie.gmioList) {
      delete i ;
    }
    xclbin->aie.gmioList.clear() ;
  }

  bool DeviceInfo::hasAIMNamed(const std::string& name)
  {
    XclbinInfo* xclbin = currentXclbin() ;
    if (!xclbin)
      return false ;

    for (auto aim : xclbin->pl.aims) {
      if (aim->name.find(name) != std::string::npos)
        return true ;
    }
    return false ;
  }

  bool DeviceInfo::hasDMAMonitor()
  {
    return hasAIMNamed("Host to Device") ;
  }

  bool DeviceInfo::hasDMABypassMonitor()
  {
    return hasAIMNamed("Peer to Peer") ;
  }

  bool DeviceInfo::hasKDMAMonitor()
  {
    return hasAIMNamed("Memory to Memory") ;
  }

} // end namespace xdp
