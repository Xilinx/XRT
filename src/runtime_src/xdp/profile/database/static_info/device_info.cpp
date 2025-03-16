/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc - All rights reserved.
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

#include <memory>
#include "xdp/profile/database/static_info/device_info.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "core/common/message.h"

namespace xdp {

  DeviceInfo::~DeviceInfo()
  {
    loadedConfigInfos.clear() ;
  }

  std::string DeviceInfo::getUniqueDeviceName() const
  {
    return deviceName + "-" + std::to_string(deviceId) ;
  }

  xrt_core::uuid DeviceInfo::currentXclbinUUID()
  {
    if (loadedConfigInfos.size() <= 0)
      return xrt_core::uuid() ;
    return loadedConfigInfos.back()->getConfigUuid();
  }

  XclbinInfo* DeviceInfo::createXclbinFromLastConfig(XclbinInfoType xclbinQueryType)
  {
    XclbinInfo* requiredXclbinInfo = nullptr;
    if (loadedConfigInfos.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", "Loaded config on device is empty.");
      return requiredXclbinInfo;
    }

    bool xclbinAvailable = false;
    auto lastConfigType = loadedConfigInfos.back()->type;
    if (lastConfigType == CONFIG_AIE_PL || lastConfigType == CONFIG_AIE_PL_FORMED)
      xclbinAvailable = true;

    if (!xclbinAvailable) {
      if (loadedConfigInfos.back()->containsXclbinType(xclbinQueryType))
        xclbinAvailable = true;
    }

    if (xclbinAvailable) {
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", "Missing xclbin is available in config.");
      ConfigInfo* lastCfg = loadedConfigInfos.back().get();
      for (auto &xclbin : lastCfg->currentXclbins)
      {
        if (xclbin->type == xclbinQueryType || xclbin->type == XCLBIN_AIE_PL) {
          // Create a copy of required missing xclbinInfo.
          requiredXclbinInfo = new XclbinInfo(xclbinQueryType);
          if (xclbinQueryType == XCLBIN_AIE_ONLY)
          {
            // Perform deep copy of missing AIE xclbin
            requiredXclbinInfo->aie = xclbin->aie;
            requiredXclbinInfo->pl.valid = false ;
          }
          else
          {
            // Perform deep copy of missing PL xclbin
            requiredXclbinInfo->pl = xclbin->pl;
            requiredXclbinInfo->aie.valid = false ;
          }
          requiredXclbinInfo->uuid = xclbin->uuid ;
          requiredXclbinInfo->name = xclbin->name ;
          break;  // Need only one such missing xclbinInfo from last config.
        }
      }
    }
    return requiredXclbinInfo;
  }

  void DeviceInfo::createConfig(XclbinInfo* xclbin)
  {
    // Create a new config
    std::unique_ptr<ConfigInfo> config = std::make_unique<ConfigInfo>();
    config->addXclbin(xclbin);

    auto currentXclbinType = xclbin->type;

    // Check if this itself is a complete xclbin (AIE+PL).
    if (currentXclbinType == XCLBIN_AIE_PL)
    {
      loadedConfigInfos.push_back(std::move(config));
      return;
    }

    // If it is not a complete xclbin.
    //  Check what is missing & request that missing XclbinInfo.?
    //    a. AIEInfo or PLInfo 
    XclbinInfo *missingXclbin = nullptr;
    if (currentXclbinType == XCLBIN_AIE_ONLY)
    {
      xclbin->pl.valid = false ;
      missingXclbin = createXclbinFromLastConfig(XCLBIN_PL_ONLY);
    }
    else
    {
      xclbin->aie.valid = false ;
      missingXclbin = createXclbinFromLastConfig(XCLBIN_AIE_ONLY);
    }

    // If missing part of XclbinInfo is available. 
    if (missingXclbin)
    {
      config->currentXclbins.back()->aie.numTracePLIO = loadedConfigInfos.size() == 0 ? 0 : loadedConfigInfos.back()->currentXclbins.back()->aie.numTracePLIO;
      config->addXclbin(missingXclbin);
      config->type = CONFIG_AIE_PL_FORMED;
    }
    else
    {
      // If missing part of XclbinInfo is not available.
      // This is same xclbin type load as previous xclbin.
      config->type = (currentXclbinType == XCLBIN_AIE_ONLY) ? CONFIG_AIE_ONLY : CONFIG_PL_ONLY ;
    }

    loadedConfigInfos.push_back(std::move(config));
  }

  bool DeviceInfo::hasFloatingAIMWithTrace(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->hasFloatingAIMWithTrace(xclbin);
    }
    return false ;
  }

  bool DeviceInfo::hasFloatingASMWithTrace(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->hasFloatingASMWithTrace(xclbin);
    }
    return false ;
  }

  uint64_t DeviceInfo::getNumAM(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumAM(xclbin);
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumUserAMWithTrace(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumUserAMWithTrace(xclbin) ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumAIM(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumAIM(xclbin) ;
    }

    return 0 ;
  }

  uint64_t DeviceInfo::getNumUserAIM(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumUserAIM(xclbin) ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumUserAIMWithTrace(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumUserAIMWithTrace(xclbin) ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumASM(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumASM(xclbin) ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumUserASM(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumUserASM(xclbin) ;
    }
    return 0;
  }

  uint64_t DeviceInfo::getNumUserASMWithTrace(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumUserASMWithTrace(xclbin) ;
    }
    return 0 ;
  }

  uint64_t DeviceInfo::getNumNOC(XclbinInfo* xclbin) const
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNumNOC(xclbin) ;
    }
    return 0 ;
  }

  Monitor* DeviceInfo::getAMonitor(XclbinInfo* xclbin, uint64_t slotId)
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getAMonitor(xclbin, slotId) ;
    }
    return nullptr ;
  }

  Monitor* DeviceInfo::getAIMonitor(XclbinInfo* xclbin, uint64_t slotId)
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getAIMonitor(xclbin, slotId) ;
    }
    return nullptr ;
  }

  Monitor* DeviceInfo::getASMonitor(XclbinInfo* xclbin, uint64_t slotId)
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getASMonitor(xclbin, slotId) ;
    }
    return nullptr ;
  }

  NoCNode* DeviceInfo::getNOC(XclbinInfo* xclbin, uint64_t idx)
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getNOC(xclbin, idx) ;
    }
    return nullptr ;
  }

  std::vector<Monitor*>* DeviceInfo::getAIMonitors(XclbinInfo* xclbin)
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getAIMonitors(xclbin) ;
    }
    return nullptr ;
  }

  std::vector<Monitor*>* DeviceInfo::getASMonitors(XclbinInfo* xclbin)
  {
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getASMonitors(xclbin) ;
    }
    return nullptr ;
  }

  std::vector<Monitor*> DeviceInfo::getUserAIMsWithTrace(XclbinInfo* xclbin)
  {
    std::vector<Monitor*> constructed ;
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getUserAIMsWithTrace(xclbin) ;
    }
    return constructed ;
  }

  std::vector<Monitor*> DeviceInfo::getUserASMsWithTrace(XclbinInfo* xclbin)
  {
    std::vector<Monitor*> constructed ;
    for (const auto& cfg : getLoadedConfigs()) {
      if (cfg->hasXclbin(xclbin))
        return cfg->getUserASMsWithTrace(xclbin) ;
    }
    return constructed ;
  }

  void DeviceInfo::addTraceGMIO(uint32_t id, uint8_t col, uint8_t num,
                                uint8_t stream, uint8_t len)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addTraceGMIO(id, col, num, stream, len) ;
  }

  void DeviceInfo::addAIECounter(uint32_t i, uint8_t col, uint8_t row,
                                 uint8_t num, uint16_t start, uint16_t end,
                                 uint8_t reset, uint64_t load, double freq,
                                 const std::string& mod,
                                 const std::string& aieName, uint8_t streamId)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addAIECounter(i, col, row, num, start, end,
                          reset, load, freq, mod, aieName, streamId) ;
  }

  void DeviceInfo::addAIECounterResources(uint32_t numCounters,
                                          uint32_t numTiles,
                                          uint8_t moduleType)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addAIECounterResources(numCounters, numTiles, moduleType) ;
  }

  void DeviceInfo::addAIECoreEventResources(uint32_t numEvents,
                                            uint32_t numTiles)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addAIECoreEventResources(numEvents, numTiles) ;
  }

  void DeviceInfo::addAIEMemoryEventResources(uint32_t numEvents,
                                              uint32_t numTiles)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addAIEMemoryEventResources(numEvents, numTiles) ;
  }

  void DeviceInfo::addAIEShimEventResources(uint32_t numEvents,
                                            uint32_t numTiles)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addAIEShimEventResources(numEvents, numTiles) ;
  }

  void DeviceInfo::addAIEMemTileEventResources(uint32_t numEvents,
                                               uint32_t numTiles)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addAIEMemTileEventResources(numEvents, numTiles) ;
  }

  void DeviceInfo::addAIECfgTile(std::unique_ptr<aie_cfg_tile>& tile)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->addAIECfgTile(std::move(tile)) ; 
  }

  ConfigInfo* DeviceInfo::currentConfig() const
  {
    if (getLoadedConfigs().empty())
      return nullptr ;

    return getLoadedConfigs().back().get() ;
  }
  
  void DeviceInfo::cleanCurrentConfig(XclbinInfoType type)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return ;

    config->cleanCurrentXclbinInfos(type) ;
  }

  double DeviceInfo::getMaxClockRatePLMHz()
  {
    if (deviceName.find("aws") != std::string::npos)
      return 250.0;
    return 300.0;
  }

  bool DeviceInfo::hasAIMNamed(const std::string& name)
  {
    ConfigInfo* config = currentConfig() ;
    if (!config || config->currentXclbins.empty())
      return false;
    
    return config->hasAIMNamed(name) ; 
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
