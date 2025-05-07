/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#pragma warning (disable : 4996 4267 4244 4200)
/* 4267 : Disable warning for conversion of size_t to int32_t */
/* 4244 : Disable warning for conversion of uint64_t to uint32_t */
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/api/xclbin_int.h"
#include "core/include/xrt/detail/xclbin.h"

#define XDP_CORE_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/device_info.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"

#include "core/common/query_requests.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/common/api/hw_context_int.h"

constexpr unsigned int XAM_STALL_PROPERTY_MASK  = 0x4;
constexpr unsigned int XMON_TRACE_PROPERTY_MASK = 0x1;

namespace xdp {

  VPStaticDatabase::VPStaticDatabase(VPDatabase* d)
    : db(d)
    , runSummary(nullptr)
    , systemDiagram("")
    , softwareEmulationDeviceName("default_sw_emu_device")
    , aieDevInst(nullptr)
    , aieDevice(nullptr)
    , deallocateAieDevice(nullptr)
  {
#ifdef _WIN32
    pid = _getpid() ;
#else
    pid = static_cast<int>(getpid()) ;
#endif
  }

  VPStaticDatabase::~VPStaticDatabase()
  {
    if (runSummary != nullptr)
      runSummary->write(false);

    // AIE specific functions
    if (aieDevice != nullptr && deallocateAieDevice != nullptr)
      deallocateAieDevice(aieDevice);
  }

  // ***********************************************************************
  // ***** Functions related to information on the running application *****
  int VPStaticDatabase::getPid() const
  {
    return pid ;
  }

  uint64_t VPStaticDatabase::getApplicationStartTime() const
  {
    return applicationStartTime ;
  }

  void VPStaticDatabase::setApplicationStartTime(uint64_t t)
  {
    std::lock_guard<std::mutex> lock(summaryLock) ;
    applicationStartTime = t ;
  }

  bool VPStaticDatabase::getAieApplication() const
  {
    return aieApplication;
  }

  void VPStaticDatabase::setAieApplication()
  {
    std::lock_guard<std::mutex> lock(summaryLock);
    aieApplication = true;
  }

  // ***************************************************
  // ***** Functions related to OpenCL information *****

  std::set<uint64_t>& VPStaticDatabase::getCommandQueueAddresses()
  {
    return commandQueueAddresses ;
  }

  std::set<std::string>& VPStaticDatabase::getEnqueuedKernels()
  {
    return enqueuedKernels ;
  }

  void VPStaticDatabase::addEnqueuedKernel(const std::string& identifier)
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    enqueuedKernels.emplace(identifier) ;
  }

  void VPStaticDatabase::setNumDevices(uint64_t contextId, uint64_t numDevices)
  {
    contextIdToNumDevices[contextId] = numDevices ;
  }

  uint64_t VPStaticDatabase::getNumDevices(uint64_t contextId)
  {
    if (contextIdToNumDevices.find(contextId) == contextIdToNumDevices.end())
      return 0 ;
    return contextIdToNumDevices[contextId] ;
  }

  std::string VPStaticDatabase::getSoftwareEmulationDeviceName()
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    return softwareEmulationDeviceName ;
  }

  void VPStaticDatabase::setSoftwareEmulationDeviceName(const std::string& name)
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    softwareEmulationDeviceName = name ;
  }

  std::map<std::string, uint64_t>
  VPStaticDatabase::getSoftwareEmulationCUCounts()
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    return softwareEmulationCUCounts ;
  }

  void
  VPStaticDatabase::addSoftwareEmulationCUInstance(const std::string& kName)
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    if (softwareEmulationCUCounts.find(kName) ==
        softwareEmulationCUCounts.end())
      softwareEmulationCUCounts[kName] = 1 ;
    else
      softwareEmulationCUCounts[kName] += 1 ;
  }

  std::map<std::string, bool>&
  VPStaticDatabase::getSoftwareEmulationMemUsage()
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    return softwareEmulationMemUsage ;
  }

  void
  VPStaticDatabase::
  addSoftwareEmulationMemUsage(const std::string& mem, bool used)
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    softwareEmulationMemUsage[mem] = used ;
  }

  std::vector<std::string>&
  VPStaticDatabase::getSoftwareEmulationPortBitWidths()
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    return softwareEmulationPortBitWidths ;
  }

  void VPStaticDatabase::addSoftwareEmulationPortBitWidth(const std::string& s)
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;
    softwareEmulationPortBitWidths.push_back(s) ;
  }

  // ************************************************
  // ***** Functions related to the run summary *****
  std::vector<std::pair<std::string, std::string>>&
  VPStaticDatabase::getOpenedFiles()
  {
    std::lock_guard<std::mutex> lock(summaryLock) ;
    return openedFiles ;
  }

  void VPStaticDatabase::addOpenedFile(const std::string& name,
                                       const std::string& type)
  {
    {
      // Protect changes to openedFiles and creation of the run summary.
      //  The write function, however, needs to query the opened files so
      //  place the lock inside its own scope.
      std::lock_guard<std::mutex> lock(summaryLock) ;

      openedFiles.push_back(std::make_pair(name, type)) ;

      if (runSummary == nullptr)
        runSummary = std::make_unique<VPRunSummaryWriter>("xrt.run_summary", db);
    }
    runSummary->write(false) ;
  }

  std::string VPStaticDatabase::getSystemDiagram()
  {
    std::lock_guard<std::mutex> lock(summaryLock) ;
    return systemDiagram ;
  }

  // ***************************************************************
  // ***** Functions related to information on all the devices *****
  uint64_t VPStaticDatabase::getNumDevices()
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    return deviceInfo.size() ;
  }

  DeviceInfo* VPStaticDatabase::getDeviceInfo(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId].get();
  }

  std::vector<std::string> VPStaticDatabase::getDeviceNames()
  {
    std::vector<std::string> uniqueNames ;
    std::lock_guard<std::mutex> lock(deviceLock) ;

    for (const auto& device : deviceInfo) {
      uniqueNames.push_back(device.second->getUniqueDeviceName()) ;
    }
    return uniqueNames ;
  }

  std::vector<DeviceInfo*> VPStaticDatabase::getDeviceInfos()
  {
    std::vector<DeviceInfo*> infos;
    std::lock_guard<std::mutex> lock(deviceLock) ;

    for (const auto& device : deviceInfo) {
      infos.push_back(device.second.get());
    }
    return infos ;
  }

  bool VPStaticDatabase::hasStallInfo()
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    for (const auto& device : deviceInfo) {
      for (const auto& config : device.second->getLoadedConfigs()) {

        XclbinInfo* xclbin = config->getPlXclbin();
        if (!xclbin)
          continue;
        for (const auto& cu : xclbin->pl.cus) {
          if (cu.second->getStallEnabled())
            return true ;
        }
      }
    }
    return false ;
  }

  ConfigInfo* VPStaticDatabase::getCurrentlyLoadedConfig(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->currentConfig() ;
  }

  bool VPStaticDatabase::isDeviceReady(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return false ;
    return deviceInfo[deviceId]->isReady ;
  }

  double VPStaticDatabase::getClockRateMHz(uint64_t deviceId, bool PL)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    // If we don't have any information on the specific ID, return
    //  defaults.  300 MHz for PL clock rate and 1 GHz for AIE clock rate.
    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return PL ? 300.0 : 1000.0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return PL ? 300.0 : 1000.0 ;

    if (PL) {
      XclbinInfo* xclbin = config->getPlXclbin();
      if (!xclbin)
        return 300.0 ;
      return xclbin->pl.clockRatePLMHz ;
    }
    else {
      XclbinInfo* xclbin = config->getAieXclbin();
      if (!xclbin)
        return 1000.0 ;
      return xclbin->aie.clockRateAIEMHz ;
    }
  }

  double VPStaticDatabase::getPLMaxClockRateMHz(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    // If we don't have any information on the specific ID, return
    //  defaults.  300 MHz for PL clock rate.
    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 300.0;
    
    ConfigInfo* config = deviceInfo[deviceId]->currentConfig();
    if (!config)
      return 300.0;
  
    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return 300.0;

    //We will consider the clock rate of the Compute Unit with the highest Clock Frequency
    double plClockFreq = 0;
      for (const auto& cu : xclbin->pl.cus) {
        plClockFreq = std::max(plClockFreq, cu.second->getClockFrequency());
      }
    return plClockFreq>0 ? plClockFreq : 300.0;

  }

  void VPStaticDatabase::setDeviceName(uint64_t deviceId, const std::string& name)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    deviceInfo[deviceId]->deviceName = name ;
  }

  std::string VPStaticDatabase::getDeviceName(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return "" ;
    return deviceInfo[deviceId]->deviceName ;
  }

  PLDeviceIntf* VPStaticDatabase::getDeviceIntf(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    return config->plDeviceIntf ;
  }

  // Should only be called from Alveo hardware emulation
  // because the device interface must be destroyed while the
  // simulation is still open and we cannot wait until the end of execution.
  void VPStaticDatabase::removeDeviceIntf(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock);

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return;
    ConfigInfo* config = deviceInfo[deviceId]->currentConfig();
    if (!config)
      return;

    delete config->plDeviceIntf;
    config->plDeviceIntf = nullptr;
  }

  // This function will create a PL Device Interface if an xdp::Device is
  // passed in, and then associate it with the current xclbin loaded onto
  // the device corresponding to deviceId.
  void VPStaticDatabase::createPLDeviceIntf(uint64_t deviceId, std::unique_ptr<xdp::Device> dev, XclbinInfoType newXclbinType)
  {
    std::lock_guard<std::mutex> lock(deviceLock);

    if (dev == nullptr)
      return;
    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return;
    ConfigInfo* config = deviceInfo[deviceId]->currentConfig();
    if (!config)
      return;

    // Check if new xclbin has new PL metadata
    if (newXclbinType == XCLBIN_AIE_PL || newXclbinType == XCLBIN_PL_ONLY)
    {
      if (config->plDeviceIntf != nullptr)
          delete config->plDeviceIntf; // It shouldn't be...

      config->plDeviceIntf = new PLDeviceIntf();
      config->plDeviceIntf->setDevice(std::move(dev));
      try {
        config->plDeviceIntf->readDebugIPlayout();
      }
      catch (std::exception& /* e */) {
        // If reading the debug ip layout fails, we shouldn't have
        // any device interface at all
        delete config->plDeviceIntf;
        config->plDeviceIntf = nullptr;
      }
    }
    else if (newXclbinType == XCLBIN_AIE_ONLY)
    {
      // By the time of PLDeviceIntf creation, corresponding config is already stored in loaded configs.
      // currently loaded config  = totalConfigs-1
      // previously loaded config = totalConfigs-2
      // Hence, required missing PLDeviceIntf is fetched from (totalConfigs-2) index.
      int totalConfigs = deviceInfo[deviceId]->loadedConfigInfos.size();
      if (totalConfigs > 1)
      {
        config->plDeviceIntf = deviceInfo[deviceId]->loadedConfigInfos[totalConfigs-2]->plDeviceIntf;
        deviceInfo[deviceId]->loadedConfigInfos[totalConfigs-2]->plDeviceIntf = nullptr;
      }else {
        // No previous PL device interface available
      }
    }
  }

  uint64_t VPStaticDatabase::getKDMACount(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->kdmaCount ;
  }

  void VPStaticDatabase::setHostMaxReadBW(uint64_t deviceId, double bw)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return;

    xclbin->pl.hostMaxReadBW = bw ;
  }

  double VPStaticDatabase::getHostMaxReadBW(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0.0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return 0.0 ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return 0.0;

    return xclbin->pl.hostMaxReadBW ;
  }

  void VPStaticDatabase::setHostMaxWriteBW(uint64_t deviceId, double bw)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return ;

    xclbin->pl.hostMaxWriteBW = bw ;
  }

  double VPStaticDatabase::getHostMaxWriteBW(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0.0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return 0.0 ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return 0.0;

    return xclbin->pl.hostMaxWriteBW ;
  }

  void VPStaticDatabase::setKernelMaxReadBW(uint64_t deviceId, double bw)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return;

    xclbin->pl.kernelMaxReadBW = bw ;
  }

  double VPStaticDatabase::getKernelMaxReadBW(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0.0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return 0.0 ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return 0.0;

    return xclbin->pl.kernelMaxReadBW ;
  }

  void VPStaticDatabase::setKernelMaxWriteBW(uint64_t deviceId, double bw)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return;

    xclbin->pl.kernelMaxWriteBW = bw ;
  }

  double VPStaticDatabase::getKernelMaxWriteBW(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0.0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return 0.0 ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return 0.0;

    return xclbin->pl.kernelMaxWriteBW ;
  }

  std::string VPStaticDatabase::getXclbinName(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return "" ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return "" ;

    return config->getXclbinNames();
  }

  const std::vector<std::unique_ptr<ConfigInfo>>& VPStaticDatabase::getLoadedConfigs(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end()) {
      static const std::vector<std::unique_ptr<xdp::ConfigInfo>> emptyVector;
      return emptyVector;
    }
    return deviceInfo[deviceId]->getLoadedConfigs() ;
  }


  ComputeUnitInstance* VPStaticDatabase::getCU(uint64_t deviceId, int32_t cuId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return nullptr;

    return xclbin->pl.cus[cuId] ;
  }

  Memory* VPStaticDatabase::getMemory(uint64_t deviceId, int32_t memId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin)
      return nullptr;

    if (xclbin->pl.memoryInfo.find(memId) == xclbin->pl.memoryInfo.end())
      return nullptr ;

    return xclbin->pl.memoryInfo[memId] ;
  }

  void VPStaticDatabase::getDataflowConfiguration(uint64_t deviceId,
                                                  bool* config,
                                                  size_t size)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    ConfigInfo* currentConfig = deviceInfo[deviceId]->currentConfig() ;
    if (!currentConfig)
      return ;

    XclbinInfo* xclbin = currentConfig->getPlXclbin();
    if (!xclbin)
      return;

    // User space AM in sorted order of their slotIds.  Matches with
    //  sorted list of AM in xdp::DeviceIntf
    size_t count = 0 ;
    for (auto mon : xclbin->pl.ams) {
      if (count >= size)
        return ;
      auto cu = xclbin->pl.cus[mon->cuIndex] ;
      config[count] = cu->getDataflowEnabled() ;
      ++count ;
    }
  }

  void VPStaticDatabase::getFaConfiguration(uint64_t deviceId, bool* config,
                                            size_t size)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    ConfigInfo* currentConfig = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return ;

    // User space AM in sorted order of their slotIds.  Matches with
    //  sorted list of AM in xdp::PLDeviceIntf
    XclbinInfo* xclbin = currentConfig->getPlXclbin();
    if (!xclbin)
      return;

    size_t count = 0 ;
    for (auto mon : xclbin->pl.ams) {
      if (count >= size)
        return ;
      auto cu = xclbin->pl.cus[mon->cuIndex] ;
      config[count] = cu->getHasFA() ;
      ++count ;
    }
  }

  std::string VPStaticDatabase::getCtxInfo(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return "" ;
    return deviceInfo[deviceId]->ctxInfo ;
  }

  // *********************************************************
  // ***** Functions related to AIE specific information *****
  uint8_t VPStaticDatabase::getAIEGeneration(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 1 ;

    return deviceInfo[deviceId]->getAIEGeneration() ;
  }

  bool VPStaticDatabase::isAIECounterRead(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return false ;

    for (const auto& config : deviceInfo[deviceId]->getLoadedConfigs()) {
      XclbinInfo* xclbin = config->getAieXclbin();
      if (!xclbin)
        continue ;
      if (xclbin->aie.isAIEcounterRead)
        return true ;
    }
    return false ;
  }

  void VPStaticDatabase::setIsAIECounterRead(uint64_t deviceId, bool val)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return ;

    xclbin->aie.isAIEcounterRead = val ;
  }

  void VPStaticDatabase::setIsGMIORead(uint64_t deviceId, bool val)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return ;

    xclbin->aie.isGMIORead = val ;
  }

  bool VPStaticDatabase::isGMIORead(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return false ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return false ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return false;

    return xclbin->aie.isGMIORead ;
  }

  uint64_t VPStaticDatabase::getNumAIECounter(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return 0 ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return 0;

    return xclbin->aie.aieList.size() ;
  }

  uint64_t VPStaticDatabase::getNumTraceGMIO(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return 0 ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return 0;

    return xclbin->aie.gmioList.size() ;
  }

  AIECounter* VPStaticDatabase::getAIECounter(uint64_t deviceId, uint64_t idx)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    if (xclbin->aie.aieList.size()>0)
        return xclbin->aie.aieList[idx] ;
    return nullptr;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIECoreCounterResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieCoreCountersMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEMemoryCounterResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieMemoryCountersMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEShimCounterResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieShimCountersMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEMemTileCounterResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieMemTileCountersMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIECoreEventResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieCoreEventsMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEMemoryEventResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieMemoryEventsMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEShimEventResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieShimEventsMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEMemTileEventResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieMemTileEventsMap) ;
  }

  std::vector<std::unique_ptr<aie_cfg_tile>>*
  VPStaticDatabase::getAIECfgTiles(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    return &(xclbin->aie.aieCfgList) ;
  }

  TraceGMIO* VPStaticDatabase::getTraceGMIO(uint64_t deviceId, uint64_t idx)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return nullptr;

    if (idx < xclbin->aie.gmioList.size())
        return xclbin->aie.gmioList[idx] ;
    return nullptr;
  }

  void VPStaticDatabase::addTraceGMIO(uint64_t deviceId, uint32_t i,
                                      uint8_t col, uint8_t num,
                                      uint8_t stream, uint8_t len)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addTraceGMIO(i, col, num, stream, len) ;
  }

  void VPStaticDatabase::addAIECounter(uint64_t deviceId, uint32_t i,
                                       uint8_t col, uint8_t row, uint8_t num,
                                       uint16_t start, uint16_t end,
                                       uint8_t reset, uint64_t load,
                                       double freq, const std::string& mod,
                                       const std::string& aieName, uint8_t streamId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIECounter(i, col, row, num, start, end, reset,
                                        load, freq, mod, aieName, streamId) ;
  }

  void VPStaticDatabase::addAIECounterResources(uint64_t deviceId,
                                                uint32_t numCounters,
                                                uint32_t numTiles,
                                                uint8_t moduleType)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIECounterResources(numCounters, numTiles, moduleType);
  }

  void VPStaticDatabase::addAIECoreEventResources(uint64_t deviceId,
                                                  uint32_t numEvents,
                                                  uint32_t numTiles)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIECoreEventResources(numEvents, numTiles) ;
  }

  void VPStaticDatabase::addAIEMemoryEventResources(uint64_t deviceId,
                                                    uint32_t numEvents,
                                                    uint32_t numTiles)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIEMemoryEventResources(numEvents, numTiles) ;
  }

  void VPStaticDatabase::addAIEShimEventResources(uint64_t deviceId,
                                                  uint32_t numEvents,
                                                  uint32_t numTiles)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIEShimEventResources(numEvents, numTiles) ;
  }

  void VPStaticDatabase::addAIEMemTileEventResources(uint64_t deviceId,
                                                     uint32_t numEvents,
                                                     uint32_t numTiles)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIEMemTileEventResources(numEvents, numTiles) ;
  }

  void VPStaticDatabase::addAIECfgTile(uint64_t deviceId,
                                       std::unique_ptr<aie_cfg_tile>& tile)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIECfgTile(tile) ;
  }

  uint64_t VPStaticDatabase::getNumTracePLIO(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return 0 ;

    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return 0;
    
    return xclbin->aie.numTracePLIO ;
  }

  uint64_t VPStaticDatabase::getNumAIETraceStream(uint64_t deviceId)
  {
    uint64_t numAIETraceStream = getNumTracePLIO(deviceId) ;
    if (numAIETraceStream)
      return numAIETraceStream ;
    {
      // NumTracePLIO also locks the database, so put this lock in its own
      //  scope after numTracePLIO has returned.
      std::lock_guard<std::mutex> lock(deviceLock) ;

      if (deviceInfo.find(deviceId) == deviceInfo.end())
        return 0 ;

      ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
      if (!config)
        return 0 ;

      XclbinInfo* xclbin = config->getAieXclbin();
      if (!xclbin)
        return 0;

      auto rc = xclbin->aie.gmioList.size() ;
      return rc;
    }
  }

  void* VPStaticDatabase::getAieDevInst(std::function<void* (void*)> fetch,
                                        void* devHandle)
  {
    std::lock_guard<std::mutex> lock(aieLock) ;
    if (aieDevInst)
      return aieDevInst ;

    aieDevInst = fetch(devHandle) ;
    return aieDevInst ;
  }

  void* VPStaticDatabase::getAieDevice(std::function<void* (void*)> allocate,
                                       std::function<void (void*)> deallocate,
                                       void* devHandle)
  {
    std::lock_guard<std::mutex> lock(aieLock) ;
    if (aieDevice)
      return aieDevice;
    if (!aieDevInst)
      return nullptr ;

    deallocateAieDevice = deallocate ;
    aieDevice = allocate(devHandle) ;
    return aieDevice ;
  }

  // ************************************************************************
  // ***** Functions for information from a specific xclbin on a device *****
  uint64_t VPStaticDatabase::getNumAM(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumAM(xclbin) ;
  }

  uint64_t VPStaticDatabase::getNumUserAMWithTrace(uint64_t deviceId,
                                                   XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumUserAMWithTrace(xclbin) ;
  }

  // Get the total number of AIMs in the design.  This includes shell monitors
  //  and all user space monitors.
  uint64_t VPStaticDatabase::getNumAIM(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumAIM(xclbin) ;
  }

  // Get the number of AIMs in the user space, including monitors configured
  //  for counters only and counters + trace.  Exclude shell monitors.
  uint64_t
  VPStaticDatabase::getNumUserAIM(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumUserAIM(xclbin) ;
  }

  // Get the number of AIMs only in the user space configured with trace.
  //  Exclude shell monitors, memory monitors, and any other monitors configured
  //  just with counters.
  uint64_t
  VPStaticDatabase::getNumUserAIMWithTrace(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumUserAIMWithTrace(xclbin) ;
  }

  // Get the total number of ASMs in the design.  This includes shell monitors
  //  and all user space monitors.
  uint64_t VPStaticDatabase::getNumASM(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumASM(xclbin) ;
  }

  // Get the number of ASMs in the user space, including monitors configured
  //  for counters only and counters + trace.  Exclude shell monitors.
  uint64_t
  VPStaticDatabase::getNumUserASM(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumUserASM(xclbin) ;
  }

  // Get the number of ASMs only in the user space configured with trace.
  //  Exclude shell monitors and any other monitors configured
  //  just with counters.
  uint64_t
  VPStaticDatabase::getNumUserASMWithTrace(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumUserASMWithTrace(xclbin) ;
  }

  uint64_t VPStaticDatabase::getNumNOC(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->getNumNOC(xclbin) ;
  }

  std::vector<Monitor*>*
  VPStaticDatabase::getAIMonitors(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->getAIMonitors(xclbin) ;
  }

  std::vector<Monitor*>
  VPStaticDatabase::getUserAIMsWithTrace(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end()) {
      std::vector<Monitor*> constructed ;
      return constructed ;
    }
    return deviceInfo[deviceId]->getUserAIMsWithTrace(xclbin) ;
  }

  std::vector<Monitor*>*
  VPStaticDatabase::getASMonitors(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->getASMonitors(xclbin) ;
  }

  std::vector<Monitor*>
  VPStaticDatabase::getUserASMsWithTrace(uint64_t deviceId, XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end()) {
      std::vector<Monitor*> constructed ;
      return constructed ;
    }
    return deviceInfo[deviceId]->getUserASMsWithTrace(xclbin) ;
  }

  bool VPStaticDatabase::hasFloatingAIMWithTrace(uint64_t deviceId,
                                                 XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return false ;
    return deviceInfo[deviceId]->hasFloatingAIMWithTrace(xclbin) ;
  }

  bool VPStaticDatabase::hasFloatingASMWithTrace(uint64_t deviceId,
                                                 XclbinInfo* xclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return false ;
    return deviceInfo[deviceId]->hasFloatingASMWithTrace(xclbin) ;
  }

  // ********************************************************************
  // ***** Functions for single monitors from an xclbin on a device *****
  Monitor*
  VPStaticDatabase::
  getAMonitor(uint64_t deviceId, XclbinInfo* xclbin, uint64_t slotId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->getAMonitor(xclbin, slotId) ;
  }

  Monitor*
  VPStaticDatabase::
  getAIMonitor(uint64_t deviceId, XclbinInfo* xclbin, uint64_t slotId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->getAIMonitor(xclbin, slotId) ;
  }

  Monitor*
  VPStaticDatabase::
  getASMonitor(uint64_t deviceId, XclbinInfo* xclbin, uint64_t slotId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->getASMonitor(xclbin, slotId) ;
  }

  NoCNode*
  VPStaticDatabase::getNOC(uint64_t deviceId, XclbinInfo* xclbin, uint64_t idx)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->getNOC(xclbin, idx) ;
  }

  // ************************************************************************

  bool VPStaticDatabase::validXclbin(void* devHandle)
  {
    std::shared_ptr<xrt_core::device> device =
      xrt_core::get_userpf_device(devHandle);

    // If this xclbin was built with tools before the 2019.2 release, we
    //  do not support device profiling.  The XRT version of 2019.2 was 2.5.459

    // TODO : Check only latest xclbin returned via device query if it is valid
    auto xclbin = device->get_xclbin(device->get_xclbin_uuid());
    const axlf* binary = xclbin.get_axlf();

    if (binary == nullptr)
      return false;

    auto major = static_cast<unsigned int>(binary->m_header.m_versionMajor);
    auto minor = static_cast<unsigned int>(binary->m_header.m_versionMinor);

    if (major < earliestSupportedXRTVersionMajor())
      return false;
    if (minor < earliestSupportedXRTVersionMinor())
      return false;

    return true;
  }

  // This function is called whenever a device is loaded with an
  //  xclbin.  It has to clear out any previous device information and
  //  reload our information.
  void
  VPStaticDatabase::updateDeviceFromHandle(uint64_t deviceId,
                                           std::unique_ptr<xdp::Device> xdpDevice,
                                           void* devHandle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(devHandle);

    if (nullptr == device)
      return;

    xrt::uuid new_xclbin_uuid;

    if (getFlowMode() == HW_EMU && !isEdge() && !isClient()) {
      // This has to be Alveo hardware emulation, which doesn't support
      // the xclbin_slots query.
      new_xclbin_uuid = device->get_xclbin_uuid();
    }
    else {
      std::vector<xrt_core::query::xclbin_slots::slot_info> xclbin_slot_info;
      try {
        xclbin_slot_info = xrt_core::device_query<xrt_core::query::xclbin_slots>(device.get());
      }
      catch (const std::exception& e) {
        std::stringstream msg;
        msg << "Exception occured while retrieving loaded xclbin info: " << e.what();
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      }

      if (xclbin_slot_info.empty())
        return;
      new_xclbin_uuid = xrt::uuid(xclbin_slot_info.back().uuid);
    }

    /* If multiple plugins are enabled for the current run, the first plugin has already updated device information
     * in the static data base. So, no need to read the xclbin information again.
     */
    if (!resetDeviceInfo(deviceId, xdpDevice.get(), new_xclbin_uuid))
      return;

    xrt::xclbin xrtXclbin = device->get_xclbin(new_xclbin_uuid);
    DeviceInfo* devInfo   = updateDevice(deviceId, xrtXclbin, std::move(xdpDevice), false);
    if (device->is_nodma())
      devInfo->isNoDMADevice = true;
  }

  void
  VPStaticDatabase::
  updateDeviceFromCoreDevice(uint64_t deviceId,
                             std::shared_ptr<xrt_core::device> device,
                             bool readAIEMetadata,
                             std::unique_ptr<xdp::Device> xdpDevice)
  {
    xrt::uuid new_xclbin_uuid;
    //TODO:: Getting xclbin_uuid should be unified for both Client and VE2.
    if(isClient()) {
      new_xclbin_uuid = device->get_xclbin_uuid();
    }
    else {
      std::vector<xrt_core::query::xclbin_slots::slot_info> xclbin_slot_info;
      try {
        xclbin_slot_info = xrt_core::device_query<xrt_core::query::xclbin_slots>(device.get());
      }
      catch (const std::exception& e) {
        std::stringstream msg;
        msg << "Exception occured while retrieving loaded xclbin info: " << e.what();
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      }

      if (xclbin_slot_info.empty())
        return;
      new_xclbin_uuid = xrt::uuid(xclbin_slot_info.back().uuid);
    }

    /* If multiple plugins are enabled for the current run, the first plugin has already updated device information
     * in the static data base. So, no need to read the xclbin information again.
     */
    if (!resetDeviceInfo(deviceId, xdpDevice.get(), new_xclbin_uuid))
      return;
    xrt::xclbin xrtXclbin = device->get_xclbin(new_xclbin_uuid);
    updateDevice(deviceId, xrtXclbin, std::move(xdpDevice), isClient(), readAIEMetadata);
  }

  // Return true if we should reset the device information.
  // Return false if we should not reset device information
  bool VPStaticDatabase::resetDeviceInfo(uint64_t deviceId, xdp::Device* xdpDevice, xrt_core::uuid new_xclbin_uuid)
  {
    std::lock_guard<std::mutex> lock(deviceLock);

    auto itr = deviceInfo.find(deviceId);
    if(itr != deviceInfo.end()) {
      DeviceInfo *devInfo = itr->second.get();
      ConfigInfo* config = devInfo->currentConfig() ;

      if (config->containsXclbin(new_xclbin_uuid)) {
        // Even if we're attempting to load the same xclbin, if we need to
        // add a PL Device Interface, then we should reset the device info
        if (config->plDeviceIntf == nullptr && xdpDevice != nullptr)
          return true;

        return false;
      }
    }
    return true;
  }

  void VPStaticDatabase::setXclbinName(XclbinInfo* currentXclbin,
                                       const char* systemMetadataSection,
                                       size_t systemMetadataSz)
  {
    if (currentXclbin == nullptr)
      return;

    const char* defaultName = "default.xclbin";

    if (systemMetadataSection == nullptr || systemMetadataSz <= 0) {
      // If there is no SYSTEM_METADATA section, use a default name
      currentXclbin->name = defaultName;
      return;
    }

    try {
      std::stringstream ss;
      ss.write(systemMetadataSection, systemMetadataSz);

      // Create a property tree based off of the JSON
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      currentXclbin->name = pt.get<std::string>("system_diagram_metadata.xclbin.generated_by.xclbin_name", "");
      if(!currentXclbin->name.empty()) {
        currentXclbin->name += ".xclbin";
      }
    } catch(...) {
      currentXclbin->name = defaultName;
    }
  }

  void VPStaticDatabase::updateSystemDiagram(const char* systemMetadataSection,
                                             size_t systemMetadataSz)
  {
    if (systemMetadataSection == nullptr || systemMetadataSz <= 0)
      return;

    // For now, also update the System metadata for the run summary.
    //  TODO: Expand this so that multiple devices and multiple xclbins
    //  don't overwrite the single system diagram information
    std::ostringstream buf ;
    for (size_t index = 0 ; index < systemMetadataSz ; ++index) {
      buf << std::hex << std::setw(2) << std::setfill('0')
          << static_cast<unsigned int>(systemMetadataSection[index]);
    }

    {
      std::lock_guard<std::mutex> lock(summaryLock) ;
      systemDiagram = buf.str() ;
    }
  }

  void VPStaticDatabase::addPortInfo(XclbinInfo* currentXclbin,
                                     const char* systemMetadataSection,
                                     size_t systemMetadataSz)
  {
    if (currentXclbin == nullptr || systemMetadataSection == nullptr ||
        systemMetadataSz <= 0)
      return;

    // Parse the SYSTEM_METADATA section using boost property trees, which
    // could throw exceptions in multiple ways.
    try {
      std::stringstream ss;
      ss.write(systemMetadataSection, systemMetadataSz);

      // Create a property tree based off of the JSON
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      auto top = pt.get_child("system_diagram_metadata");

      // Parse the xsa section for memory topology information
      auto xsa = top.get_child("xsa");
      auto device_topology = xsa.get_child("device_topology");

      // Parse the xclbin section for compute unit port information
      auto xclbin = top.get_child("xclbin");
      auto user_regions = xclbin.get_child("user_regions");

      // Temp data structures to hold mappings of each CU's argument to memory
      typedef std::pair<std::string, std::string> fullName;
      std::map<fullName, int32_t> argumentToMemoryIndex;
      std::map<int, std::string> computeUnitIdToName;

      // Keep track of all the compute unit names associated with the id
      // number so we can make the connection later.
      for (auto& region : user_regions) {
        for (auto& compute_unit : region.second.get_child("compute_units")) {
          auto id = compute_unit.second.get<std::string>("id");
          auto cuName = compute_unit.second.get<std::string>("cu_name");
          auto idAsInt = std::stoi(id);
          computeUnitIdToName[idAsInt] = cuName;
        }
      }

      // We also need to know which argument goes to which memory
      for (auto& region : user_regions) {
        for (auto& connection : region.second.get_child("connectivity")) {
          auto node1 = connection.second.get_child("node1");
          auto node2 = connection.second.get_child("node2");

          auto arg = node1.get<std::string>("arg_name");
          auto cuId = node1.get<std::string>("id");
          auto id = node2.get<std::string>("id");
          std::string cuName = "";

          if (cuId != "") {
            int cuIdAsInt = std::stoi(cuId);
            cuName = computeUnitIdToName[cuIdAsInt];
          }

          if (id != "" && arg != "")
            argumentToMemoryIndex[{cuName, arg}] = std::stoi(id);
        }
      }

      auto tolower = [](char c) { return static_cast<char>(std::tolower(c));};

      // Now go through each of the kernels to determine the port information
      for (auto& region : user_regions) {
        for (auto& kernel : region.second.get_child("kernels")) {
          auto kernelName = kernel.second.get<std::string>("name", "");
          for (auto& port : kernel.second.get_child("ports")) {
            auto portName = port.second.get<std::string>("name");
            auto portType = port.second.get<std::string>("port_type");
            if (portName == "S_AXI_CONTROL" || portType == "stream")
              continue;

            auto portWidth = port.second.get<std::string>("data_width");
            std::transform(portName.begin(), portName.end(), portName.begin(),
                           tolower);

            currentXclbin->pl.addComputeUnitPorts(kernelName,
                                                  portName,
                                                  std::stoi(portWidth));
          }
          for (auto& arg : kernel.second.get_child("arguments")) {
            auto portName = arg.second.get<std::string>("port");
            auto portType = arg.second.get<std::string>("type");
            if (portName == "S_AXI_CONTROL" ||
                portType.find("stream") != std::string::npos)
              continue;
            std::transform(portName.begin(), portName.end(), portName.begin(),
                           tolower);
            auto argName = arg.second.get<std::string>("name");

            // All of the compute units have the same mapping of arguments
            // to ports.
            currentXclbin->pl.addArgToPort(kernelName, argName, portName);

            // Go through all of the compute units for this kernel
            auto cus = currentXclbin->pl.collectCUs(kernelName);
            for (auto cu : cus) {
	      std::string cuName = cu->getName();
              if (argumentToMemoryIndex.find({cuName, argName}) == argumentToMemoryIndex.end())
                continue; // Skip streams not connected to memory
              auto memId = argumentToMemoryIndex[{cuName, argName}];

              currentXclbin->pl.connectArgToMemory(cuName, portName,
                                                   argName, memId);
            }
          }
        }
      }

    } catch(...) {
      // If we catch an exception, leave the rest of the port info as is.
    }
  }

  std::unique_ptr<IpMetadata> VPStaticDatabase::populateIpMetadata(
                                uint64_t deviceId, 
                                const std::shared_ptr<xrt_core::device>& device)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return nullptr;

    XclbinInfo *xclbin = config->getPlXclbin();
    if (!xclbin)
      return nullptr;

    auto data = device->get_axlf_section(IP_METADATA);
    if (!data.first || !data.second)
      return nullptr;

    std::stringstream ss;
    ss.write(data.first,data.second);
    boost::property_tree::ptree pt;
    try {
      boost::property_tree::read_json(ss,pt);
      return std::make_unique<IpMetadata>(pt);
    } catch(...) {
      return nullptr;
    }
    
  }

  void VPStaticDatabase::createComputeUnits(XclbinInfo* currentXclbin,
                                            const ip_layout* ipLayoutSection,
                                            const char* systemMetadataSection,
                                            size_t systemMetadataSz)
  {
    if (currentXclbin == nullptr || ipLayoutSection == nullptr)
      return;

    //---------------------------------------------------------------------
    bool clockFlag = true;
    boost::property_tree::ptree pt;
    boost::property_tree::ptree user_regions;

    if (systemMetadataSection == nullptr || systemMetadataSz <= 0) {
      clockFlag = false;
    }

    else {

    // Parse the SYSTEM_METADATA section using boost property trees, which
    // could throw exceptions in multiple ways.
    try{
        std::stringstream ss;
        ss.write(systemMetadataSection, systemMetadataSz);

        // Create a property tree based off of the JSON
        boost::property_tree::read_json(ss, pt);

        auto top = pt.get_child("system_diagram_metadata");

        // Parse the xclbin section for compute unit port information
        auto xclbin = top.get_child("xclbin");
        user_regions = xclbin.get_child("user_regions");
    } catch(...) {
      // TODO: catch section
      clockFlag = false;
    }
    }
    //---------------------------------------------------------------------

    ComputeUnitInstance* cu = nullptr;
    for(int32_t i = 0; i < ipLayoutSection->m_count; ++i) {
      const struct ip_data* ipData = &(ipLayoutSection->m_ip_data[i]);
      if(ipData->m_type != IP_KERNEL)
        continue;

      std::string cuName(reinterpret_cast<const char*>(ipData->m_name));
      if(std::string::npos != cuName.find(":dm_")) {
        /* Assumption : If the IP_KERNEL CU name is of the format "<kernel_name>:dm_*", then it is a
         *              data mover and it should not be identified as a "CU" in profiling
         */
        continue;
      }
      cu = new ComputeUnitInstance(i, cuName);
      currentXclbin->pl.cus[i] = cu ;
      if((ipData->properties >> IP_CONTROL_SHIFT) & AP_CTRL_CHAIN) {
        cu->setDataflowEnabled(true);
      } else
      if((ipData->properties >> IP_CONTROL_SHIFT) & FAST_ADAPTER) {
        cu->setFaEnabled(true);
      }

      //---------------------------------------------------------------------
      //asigning clock frequency to each compute unit
      double clckfreq = 300.0;
      if (clockFlag) {

        try {
          // Keep track of all the compute unit names associated with the id
          // number so we can make the connection later.
          for (auto& region : user_regions) {
            for (auto& compute_unit : region.second.get_child("compute_units")) {
              auto cuNameSysMD = compute_unit.second.get<std::string>("cu_name");
              if ( 0 == cu->getName().compare(cuNameSysMD)) {
              for (auto& clck :compute_unit.second.get_child("clocks")) {
                // If the clock port name is ap clck, that is the frequency we associate
                // with the compute unit
                auto clockPortName = clck.second.get<std::string>("port_name");
                if (0 == clockPortName.compare("ap_clk")) {
                  auto tmp_var = clck.second.get<std::string>("requested_frequency");
                  clckfreq = std::stod(tmp_var);
                  break;
                }
              }
              break;
            }
          }
        }

      }catch(...) {
          clckfreq = 300.0;
      }
    }
    cu->setClockFrequency(clckfreq);
    //---------------------------------------------------------------------
    }
  }

  void VPStaticDatabase::createMemories(XclbinInfo* currentXclbin,
                                        const mem_topology* memTopologySection)
  {
    if (currentXclbin == nullptr || memTopologySection == nullptr)
      return;

    for(int32_t i = 0; i < memTopologySection->m_count; ++i) {
      const struct mem_data* memData = &(memTopologySection->m_mem_data[i]);
      currentXclbin->pl.memoryInfo[i] =
        new Memory(memData->m_type, i, memData->m_base_address, memData->m_size,
                   reinterpret_cast<const char*>(memData->m_tag),
                   memData->m_used);
    }
  }

  void VPStaticDatabase::createConnections(XclbinInfo* currentXclbin,
                                           const ip_layout* ipLayoutSection,
                                           const mem_topology* memTopologySection,
                                           const connectivity* connectivitySection)
  {
    if (currentXclbin == nullptr
        || ipLayoutSection == nullptr
        || memTopologySection == nullptr
        || connectivitySection == nullptr)
      return;

    // Now make the connections
    ComputeUnitInstance* cu = nullptr;
    for(int32_t i = 0; i < connectivitySection->m_count; ++i) {
      const struct connection* connctn = &(connectivitySection->m_connection[i]);

      if(currentXclbin->pl.cus.find(connctn->m_ip_layout_index) == currentXclbin->pl.cus.end()) {
        const struct ip_data* ipData = &(ipLayoutSection->m_ip_data[connctn->m_ip_layout_index]);
        if(ipData->m_type != IP_KERNEL) {
          // error ?
          continue;
        }
        std::string cuName(reinterpret_cast<const char*>(ipData->m_name));
        if(std::string::npos != cuName.find(":dm_")) {
          /* Assumption : If the IP_KERNEL CU name is of the format "<kernel_name>:dm_*", then it is a
           *              data mover and it should not be identified as a "CU" in profiling
           */
          continue;
        }
        cu = new ComputeUnitInstance(connctn->m_ip_layout_index, cuName);
        currentXclbin->pl.cus[connctn->m_ip_layout_index] = cu;
        if((ipData->properties >> IP_CONTROL_SHIFT) & AP_CTRL_CHAIN) {
          cu->setDataflowEnabled(true);
        } else
        if((ipData->properties >> IP_CONTROL_SHIFT) & FAST_ADAPTER) {
          cu->setFaEnabled(true);
        }
      } else {
        cu = currentXclbin->pl.cus[connctn->m_ip_layout_index];
      }

      if(currentXclbin->pl.memoryInfo.find(connctn->mem_data_index) == currentXclbin->pl.memoryInfo.end()) {
        const struct mem_data* memData = &(memTopologySection->m_mem_data[connctn->mem_data_index]);
        currentXclbin->pl.memoryInfo[connctn->mem_data_index]
                 = new Memory(memData->m_type, connctn->mem_data_index,
                              memData->m_base_address, memData->m_size, reinterpret_cast<const char*>(memData->m_tag), memData->m_used);
      }
      cu->addConnection(connctn->arg_index, connctn->mem_data_index);
    }
  }

  void VPStaticDatabase::annotateWorkgroupSize(XclbinInfo* currentXclbin,
                                               const char* embeddedMetadataSection,
                                               size_t embeddedMetadataSz)
  {
    if (currentXclbin == nullptr
        || embeddedMetadataSection == nullptr
        || embeddedMetadataSz <= 0)
      return;

    boost::property_tree::ptree xmlProject;
    std::stringstream xmlStream;
    xmlStream.write(embeddedMetadataSection, embeddedMetadataSz);
    boost::property_tree::read_xml(xmlStream, xmlProject);

    for(const auto& coreItem : xmlProject.get_child("project.platform.device.core")) {
      std::string coreItemName = coreItem.first;
      if(0 != coreItemName.compare("kernel")) {  // skip items other than "kernel"
        continue;
      }
      auto kernel = coreItem;
      auto kernelNameItem    = kernel.second.get_child("<xmlattr>");
      std::string kernelName = kernelNameItem.get<std::string>("name", "");

      std::string x ;
      std::string y ;
      std::string z ;

      try {
        auto workGroupSz = kernel.second.get_child("compileWorkGroupSize");
        x = workGroupSz.get<std::string>("<xmlattr>.x", "");
        y = workGroupSz.get<std::string>("<xmlattr>.y", "");
        z = workGroupSz.get<std::string>("<xmlattr>.z", "");
      } catch (...) {
        // RTL kernels might not have this information, so if the fetch
        //  fails default to 1:1:1
        x = "1" ;
        y = "1" ;
        z = "1" ;
      }

      // Find the ComputeUnitInstance
      for(const auto& cuItr : currentXclbin->pl.cus) {
        if(0 != cuItr.second->getKernelName().compare(kernelName)) {
          continue;
        }
        cuItr.second->setDim(std::stoi(x), std::stoi(y), std::stoi(z));
      }
    }
  }

  void VPStaticDatabase::initializeAM(DeviceInfo* devInfo,
                                      const std::string& name,
                                      const struct debug_ip_data* debugIpData)
  {
    ConfigInfo* config = devInfo->currentConfig() ;
    if (!config) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an AM without a loaded xclbin") ;
      return ;
    }

    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);

    XclbinInfo* xclbin = config->getPlXclbin();
    if (!xclbin) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an AM without a loaded PL xclbin") ;
      return;
    }

    // Find the compute unit that this AM is attached to.
    for (const auto& cu : xclbin->pl.cus) {
      ComputeUnitInstance* cuObj = cu.second ;
      int32_t cuId = cu.second->getIndex() ;

      if (0 == name.compare(cu.second->getName())) {
        // Set properties on this specific CU
        if(debugIpData->m_properties & XAM_STALL_PROPERTY_MASK) {
          cuObj->setStallEnabled(true);
        }

        Monitor* mon =
          new Monitor(static_cast<DEBUG_IP_TYPE>(debugIpData->m_type), index,
                      debugIpData->m_name, cuId) ;

        if (debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK)
          mon->traceEnabled = true ;

        // Assigning the compute unit's clock frequency to the monitor
        mon->clockFrequency = cuObj->getClockFrequency();

        // Add the monitor to the list of all monitors in this xclbin
        xclbin->pl.ams.push_back(mon);
        // Associate it with this compute unit
        cuObj->setAccelMon(mon->slotIndex) ;
        break ;
      }
    }
  }

  void VPStaticDatabase::initializeAIM(DeviceInfo* devInfo,
                                       const std::string& name,
                                       const struct debug_ip_data* debugIpData)
  {
    ConfigInfo* config = devInfo->currentConfig() ;
    if (!config) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an AIM without a loaded xclbin") ;
      return ;
    }

    XclbinInfo* xclbin = config->getPlXclbin() ;
    if (!xclbin) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an AIM without loaded PL xclbin");
      return;
    }


    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);

    // The current minimum trace ID assigned to AIMs is 0, so this code
    // currently has no effect and is being marked as incorrect in Coverity.
    // It should be uncommented if the minimum trace ID assigned in the
    // hardware ever chagnes.

    //if (index < min_trace_id_aim) {
    //  std::stringstream msg;
    //  msg << "AIM with incorrect index: " << index ;
    //  xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
    //                          msg.str());
    //  index = min_trace_id_aim ;
    //}

    // Parse name to find CU Name and Memory.  We expect the name in
    //  debug_ip_layout to be in the form of "cu_name/memory_name-port_name"
    size_t pos = name.find('/');
    std::string monCuName = name.substr(0, pos);

    if (monCuName == "memory_subsystem") {
      if (xclbin)
        xclbin->pl.hasMemoryAIM = true ;
    }

    std::string memName = "" ;
    std::string portName = "" ;
    size_t pos1 = name.find('-');
    if(pos1 != std::string::npos) {
      memName = name.substr(pos1+1);
      portName = name.substr(pos+1, pos1-pos-1);
    }

    ComputeUnitInstance* cuObj = nullptr ;
    int32_t cuId = -1 ;
    int32_t memId = -1;

    // Find both the compute unit this AIM is attached to (if applicable)
    //  and the memory this AIM is attached to (if applicable).
    for(const auto& cu : xclbin->pl.cus) {
      if(0 == monCuName.compare(cu.second->getName())) {
        cuId = cu.second->getIndex();
        cuObj = cu.second;
        break;
      }
    }
    for(const auto& mem : xclbin->pl.memoryInfo) {
      if (0 == memName.compare(mem.second->spTag)) {
        memId = mem.second->index;
        break;
      }
    }

    Monitor* mon = new Monitor(static_cast<DEBUG_IP_TYPE>(debugIpData->m_type),
                               index, debugIpData->m_name, cuId, memId);

    if (cuObj) {
      mon->cuPort = cuObj->getPort(portName);
      // Assigning the compute unit's clock frequency to the monitor
      mon->clockFrequency = cuObj->getClockFrequency();
    }
    if (debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) {
      mon->traceEnabled = true ;
    }

    // Add the monitor to the list of all AIMs
    xclbin->pl.aims.push_back(mon) ;

    // Attach to a CU if appropriate
    if (cuObj) {
      cuObj->addAIM(mon->slotIndex, mon->traceEnabled) ;
    }
    else if(mon->traceEnabled) {
      // If not connected to CU and not a shell monitor, then a floating monitor
      // This floating monitor is enabled for trace too
      xclbin->pl.hasFloatingAIMWithTrace = true ;
    }
  }

  void VPStaticDatabase::initializeASM(DeviceInfo* devInfo,
                                       const std::string& name,
                                       const struct debug_ip_data* debugIpData)
  {
    ConfigInfo* config = devInfo->currentConfig() ;
    if (!config) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an ASM without a loaded xclbin") ;
      return ;
    }

    XclbinInfo* xclbin = config->getPlXclbin() ;
    if (!xclbin) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an ASM without a loaded PL xclbin") ;
      return ;
    }

    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    if (index < util::min_trace_id_asm) {
      std::stringstream msg;
      msg << "ASM with incorrect index: " << index ;
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              msg.str());
      index = util::min_trace_id_asm ;
    }

    // Parse out the name of the compute unit this monitor is attached to if
    //  possible.  We expect the name in debug_ip_layout to be in the form of
    //  compute_unit_name/port_name.  If this is a floating

    size_t pos = name.find('/');
    std::string monCuName = name.substr(0, pos);

    std::string portName = "";
    ComputeUnitInstance* cuObj = nullptr ;
    int32_t cuId = -1 ;

    for(const auto& cu : xclbin->pl.cus) {
      if(0 == monCuName.compare(cu.second->getName())) {
        cuId = cu.second->getIndex();
        cuObj = cu.second;
        break;
      }
    }
    if(-1 != cuId) {
      size_t pos1 = name.find('-');
      if(std::string::npos != pos1) {
        portName = name.substr(pos+1, pos1-pos-1);
      }
    } else { /* (-1 == cuId) */
      pos = name.find("-");
      if(std::string::npos != pos) {
        pos = name.find_first_not_of(" ", pos+1);
        monCuName = name.substr(pos);
        pos = monCuName.find('/');

        size_t pos1 = monCuName.find('-');
        if(std::string::npos != pos1) {
          portName = monCuName.substr(pos+1, pos1-pos-1);
        }

        monCuName = monCuName.substr(0, pos);

        for(const auto& cu : xclbin->pl.cus) {
          if(0 == monCuName.compare(cu.second->getName())) {
            cuId = cu.second->getIndex();
            cuObj = cu.second;
            break;
          }
        }
      }
    }

    Monitor* mon = new Monitor(static_cast<DEBUG_IP_TYPE>(debugIpData->m_type),
                               index, debugIpData->m_name,
                               cuId);
    //mon->port = portName;
    if (cuObj){
      mon->cuPort = cuObj->getPort(portName);
      mon->clockFrequency = cuObj->getClockFrequency();
    }
    if (debugIpData->m_properties & 0x2) {
      mon->isStreamRead = true;
    }
    if (debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) {
      mon->traceEnabled = true ;
    }

    // Add this monitor to the list of all monitors
    xclbin->pl.asms.push_back(mon) ;

    // If the ASM is an User Space ASM i.e. either connected to a CU or floating but not shell ASM
    if (cuObj) {
      cuObj->addASM(mon->slotIndex, mon->traceEnabled) ;
    }
    else if (mon->traceEnabled) {
      // If not connected to CU and not a shell monitor, then a floating monitor
      // This floating monitor is enabled for trace too
      xclbin->pl.hasFloatingASMWithTrace = true ;
    }
  }

  void VPStaticDatabase::initializeNOC(DeviceInfo* devInfo,
                                       const struct debug_ip_data* debugIpData)
  {
    ConfigInfo* config = devInfo->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getAieXclbin() ;
    if (!xclbin)
      return;

    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    uint8_t readTrafficClass  = debugIpData->m_properties >> 2;
    uint8_t writeTrafficClass = debugIpData->m_properties & 0x3;

    NoCNode* noc = new NoCNode(index, debugIpData->m_name, readTrafficClass,
                               writeTrafficClass) ;
    xclbin->aie.nocList.push_back(noc) ;
    // nocList in xdp::PLDeviceIntf is sorted; Is that required here?
  }

  void VPStaticDatabase::initializeTS2MM(DeviceInfo* devInfo,
                                         const struct debug_ip_data* debugIpData)
  {
    ConfigInfo* config = devInfo->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo* xclbin = config->getAieXclbin() ;
    if (!xclbin)
      xclbin = config->getPlXclbin() ;

    // TS2MM IP for either AIE PLIO or PL trace offload
    if (debugIpData->m_properties & 0x1) {
      xclbin->aie.numTracePLIO++ ;
    }
    else {
      xclbin->pl.usesTs2mm = true ;
    }
  }

  void VPStaticDatabase::initializeFIFO(DeviceInfo* devInfo)
  {
    ConfigInfo* config = devInfo->currentConfig() ;
    if (!config)
      return ;

    XclbinInfo*  xclbin = config->getPlXclbin() ;
    if (!xclbin)
      return;
    xclbin->pl.usesFifo = true ;
  }

  void VPStaticDatabase::addCommandQueueAddress(uint64_t a)
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;

    commandQueueAddresses.emplace(a) ;
  }

  XclbinInfoType VPStaticDatabase::getXclbinType(xrt::xclbin& xclbin)
  {
    bool is_aie_available = false;
    bool is_pl_available  = false;

    auto data = xrt_core::xclbin_int::get_axlf_section(xclbin, AIE_METADATA);
    if (data.first && data.second)
        is_aie_available = true;

    data = xrt_core::xclbin_int::get_axlf_section(xclbin, IP_LAYOUT);
    if (!data.first || !data.second)
      data = xrt_core::xclbin_int::get_axlf_section(xclbin, DEBUG_IP_LAYOUT);
    if (data.first && data.second)
        is_pl_available = true;

    if(is_aie_available && is_pl_available)
      return XCLBIN_AIE_PL;
    else if (is_aie_available)
      return XCLBIN_AIE_ONLY;
    else
      return XCLBIN_PL_ONLY;
  }

  // This function is called from "trace_processor" tool
  // The tool creates events from raw PL trace data
  void VPStaticDatabase::updateDevice(uint64_t deviceId, const std::string& xclbinFile)
  {
    xrt::xclbin xrtXclbin = xrt::xclbin(xclbinFile);

    // The PL post-processor does not need a connection to the actual hardware
    updateDevice(deviceId, xrtXclbin, nullptr, false);
  }

  // Methods using xrt::xclbin to retrive static information

  DeviceInfo* VPStaticDatabase::updateDevice(uint64_t deviceId, xrt::xclbin xrtXclbin, std::unique_ptr<xdp::Device> xdpDevice, bool clientBuild, bool readAIEdata)
  {
    XclbinInfoType xclbinType = getXclbinType(xrtXclbin);
    // We need to update the device, but if we had an xclbin previously loaded
    //  then we need to mark it and remove the PL interface.  We'll
    //  create a new PL interface if necessary
    if (deviceInfo.find(deviceId) != deviceInfo.end()) {
      ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
      if (config) {
        xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", "Marking the end of last config xclbin");
        db->getDynamicInfo().markXclbinEnd(deviceId) ;

        // PL Device Interface deletion is delayed until new config is formed with new xclbin load.
        // This is to confirm that previous PL xclbin can be re-used for cases
        // like in PL and AIE mix xclbins workflow.
      }
    }

    DeviceInfo* devInfo = nullptr ;
    auto itr = deviceInfo.find(deviceId);
    if (itr == deviceInfo.end()) {
      // This is the first time this device was loaded with an xclbin
      deviceInfo[deviceId] = std::make_unique<DeviceInfo>();
      devInfo = deviceInfo[deviceId].get();
      devInfo->deviceId = deviceId;
      if (isEdge())
        devInfo->isEdgeDevice = true;
    } else {
      // This is a previously used device being reloaded with a new xclbin
      devInfo = itr->second.get();

      // Do not clean config if new xclbin is AIE type as it could be for mix xclbins run.
      // It is expected to have AIE type xclbin loaded after PL type.
      devInfo->cleanCurrentConfig(xclbinType);
    }

    XclbinInfo* currentXclbin = new XclbinInfo(xclbinType) ;
    currentXclbin->uuid = xrtXclbin.get_uuid();
    currentXclbin->pl.clockRatePLMHz = findClockRate(xrtXclbin) ;

    setDeviceNameFromXclbin(deviceId, xrtXclbin);
    if (readAIEdata) {
      readAIEMetadata(xrtXclbin, clientBuild);
      setAIEGeneration(deviceId, xrtXclbin);
    }

    /* Configure AMs if context monitoring is supported
     * else disable alll AMs on this device
     */
    devInfo->ctxInfo = xrt_core::config::get_kernel_channel_info();

    if (!initializeStructure(currentXclbin, xrtXclbin)) {
      if (xclbinType != XCLBIN_AIE_ONLY) {
        delete currentXclbin;
        return devInfo;
      }
    }

    devInfo->createConfig(currentXclbin);

    // Following functions require configInfo to be created first.
    if (readAIEdata)
      setAIEClockRateMHz(deviceId, xrtXclbin);
    initializeProfileMonitors(devInfo, std::move(xrtXclbin));

    devInfo->isReady = true;

    if (xdpDevice != nullptr)
      createPLDeviceIntf(deviceId, std::move(xdpDevice), xclbinType);
    
    return devInfo;
  }

  void VPStaticDatabase::setDeviceNameFromXclbin(uint64_t deviceId, xrt::xclbin xrtXclbin)
  {
    std::lock_guard<std::mutex> lock(deviceLock);

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return;
    if (!deviceInfo[deviceId]->deviceName.empty()) {
      return;
    }

    std::pair<const char*, size_t> systemMetadata =
       xrt_core::xclbin_int::get_axlf_section(xrtXclbin, SYSTEM_METADATA);

    if (systemMetadata.first == nullptr || systemMetadata.second <= 0) {
      // There is no SYSTEM_METADATA section
      return;
    }

    try {
      std::stringstream ss;
      ss.write(systemMetadata.first, systemMetadata.second);

      // Create a property tree based off of the JSON
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      deviceInfo[deviceId]->deviceName = pt.get<std::string>("system_diagram_metadata.xsa.name", "");
    } catch(...) {
      return;
    }
  }

  void VPStaticDatabase::readAIEMetadata(xrt::xclbin xrtXclbin, bool checkDisk)
  {
    // If "checkDisk" is specified, then look on disk only for the files
    // Look for aie_trace_config first, then check for aie_control_config
    // only if we cannot find it.
    if (checkDisk) {
      metadataReader =
        aie::readAIEMetadata("aie_trace_config.json", aieMetadata);
      if (!metadataReader)
        metadataReader =
          aie::readAIEMetadata("aie_control_config.json", aieMetadata);
      if (!metadataReader)
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
                                "AIE metadata read failed!");
      return;
    }
    
    // If we aren't checking the disk, then check the currently loaded xclbin
    auto data =
      xrt_core::xclbin_int::get_axlf_section(xrtXclbin, AIE_TRACE_METADATA);
    
    if (!data.first || !data.second)
      data = xrt_core::xclbin_int::get_axlf_section(xrtXclbin, AIE_METADATA);

    if (data.first && data.second) {
      metadataReader =
        aie::readAIEMetadata(data.first, data.second, aieMetadata);
    }

    if (!metadataReader)
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
                              "AIE metadata read failed!");
    else
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
                              "AIE metadata read successfully!");
  }

  const xdp::aie::BaseFiletypeImpl*
  VPStaticDatabase::getAIEmetadataReader() const
  {
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", "AIE metadataReader requested");
    return metadataReader.get();
  }

  void VPStaticDatabase::setAIEGeneration(uint64_t deviceId, xrt::xclbin xrtXclbin) {
    std::lock_guard<std::mutex> lock(deviceLock) ;
    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return;

    if (!metadataReader)
      return;

    try {
      auto hwGen = metadataReader->getHardwareGeneration();
      deviceInfo[deviceId]->setAIEGeneration(hwGen);
    } catch(...) {
      return;
    }
  }

  void VPStaticDatabase::setAIEClockRateMHz(uint64_t deviceId, xrt::xclbin xrtXclbin) {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return;

    ConfigInfo* config = deviceInfo[deviceId]->currentConfig() ;
    if (!config)
      return;
    XclbinInfo* xclbin = config->getAieXclbin();
    if (!xclbin)
      return;

    if (!metadataReader)
       return;

    try {
      xclbin->aie.clockRateAIEMHz = metadataReader->getAIEClockFreqMHz();
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", "read clockRateAIEMHz: "
                                                        + std::to_string(xclbin->aie.clockRateAIEMHz));
    } catch(...) {
      return;
    }
  }

  double VPStaticDatabase::findClockRate(xrt::xclbin xrtXclbin)
  {
    double defaultClockSpeed = 300.0 ;

    const clock_freq_topology* clockSection =
      reinterpret_cast<const clock_freq_topology*>(
        xrt_core::xclbin_int::get_axlf_section(xrtXclbin, CLOCK_FREQ_TOPOLOGY).first);

    if(clockSection) {
      for(int32_t i = 0; i < clockSection->m_count; i++) {
        const struct clock_freq* clk = &(clockSection->m_clock_freq[i]);
        if(clk->m_type != CT_DATA) {
          continue;
        }
        return clk->m_freq_Mhz ;
      }
    }

    if (isEdge()) {
      // On Edge, we can try to get the "DATA_CLK" from the embedded metadata
      std::pair<const char*, size_t> embeddedMetadata =
        xrt_core::xclbin_int::get_axlf_section(xrtXclbin, EMBEDDED_METADATA);

      if (nullptr == embeddedMetadata.first || 0 == embeddedMetadata.second)
        return defaultClockSpeed;

      std::stringstream ss;
      ss.write(embeddedMetadata.first, embeddedMetadata.second);

      // Create a property tree based off of the XML
      boost::property_tree::ptree pt;
      boost::property_tree::read_xml(ss, pt);

      // Dig in and find all of the kernel clocks
      for (auto& clock : pt.get_child("project.platform.device.core.kernelClocks")) {
        if (clock.first != "clock")
          continue;

        try {
          std::string port = clock.second.get<std::string>("<xmlattr>.port");
          if (port != "DATA_CLK")
            continue;
          std::string freq = clock.second.get<std::string>("<xmlattr>.frequency");
          std::string freqNumeral = freq.substr(0, freq.find('M')) ;
          double frequency = defaultClockSpeed ;
          std::stringstream convert ;
          convert << freqNumeral ;
          convert >> frequency ;
          return frequency ;
        }
        catch (std::exception& /*e*/) {
          continue ;
        }
      }
    }
    return defaultClockSpeed;
  }

  bool VPStaticDatabase::initializeStructure(XclbinInfo* currentXclbin, xrt::xclbin xrtXclbin)
  {
    // Step 1 -> Create the compute units based on the IP_LAYOUT and SYSTEM_METADATA section
    const ip_layout* ipLayoutSection =
      reinterpret_cast<const ip_layout*>(xrt_core::xclbin_int::get_axlf_section(xrtXclbin, IP_LAYOUT).first);

    std::pair<const char*, size_t> systemMetadata =
       xrt_core::xclbin_int::get_axlf_section(xrtXclbin, SYSTEM_METADATA);

    if(ipLayoutSection == nullptr)
      return true;

    createComputeUnits(currentXclbin, ipLayoutSection,systemMetadata.first, systemMetadata.second);

    // Step 2 -> Create the memory layout based on the MEM_TOPOLOGY section
    const mem_topology* memTopologySection =
      reinterpret_cast<const mem_topology*>(xrt_core::xclbin_int::get_axlf_section(xrtXclbin, MEM_TOPOLOGY).first);

    if(memTopologySection == nullptr)
      return false;

    createMemories(currentXclbin, memTopologySection);

    // Step 3 -> Connect the CUs with the memory resources using the
    //           CONNECTIVITY section
    const connectivity* connectivitySection =
      reinterpret_cast<const connectivity*>(xrt_core::xclbin_int::get_axlf_section(xrtXclbin, CONNECTIVITY).first);

    if(connectivitySection == nullptr)
      return true;

    createConnections(currentXclbin, ipLayoutSection, memTopologySection,
                      connectivitySection);

    // Step 4 -> Annotate all the compute units with workgroup size using
    //           the EMBEDDED_METADATA section
    std::pair<const char*, size_t> embeddedMetadata =
       xrt_core::xclbin_int::get_axlf_section(xrtXclbin, EMBEDDED_METADATA);

    annotateWorkgroupSize(currentXclbin, embeddedMetadata.first,
                          embeddedMetadata.second);

    // Step 5 -> Fill in the details like the name of the xclbin using
    //           the SYSTEM_METADATA section
    setXclbinName(currentXclbin, systemMetadata.first, systemMetadata.second);
    updateSystemDiagram(systemMetadata.first, systemMetadata.second);
    addPortInfo(currentXclbin, systemMetadata.first, systemMetadata.second);

    return true;
  }

  bool VPStaticDatabase::initializeProfileMonitors(DeviceInfo* devInfo, xrt::xclbin xrtXclbin)
  {
    // Look into the debug_ip_layout section and load information about Profile Monitors
    // Get DEBUG_IP_LAYOUT section
    const debug_ip_layout* debugIpLayoutSection =
      reinterpret_cast<const debug_ip_layout*>(xrt_core::xclbin_int::get_axlf_section(xrtXclbin, DEBUG_IP_LAYOUT).first);

    if(debugIpLayoutSection == nullptr) return false;

    for(uint16_t i = 0; i < debugIpLayoutSection->m_count; i++) {
      const struct debug_ip_data* debugIpData =
        &(debugIpLayoutSection->m_debug_ip_data[i]);
      uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
        (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);

      std::string name(debugIpData->m_name);

      std::stringstream msg;
      msg << "Initializing profile monitor " << i
          << ": name = " << name << ", index = " << index;
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              msg.str());

      switch (debugIpData->m_type) {
      case ACCEL_MONITOR:
        initializeAM(devInfo, name, debugIpData) ;
        break ;
      case AXI_MM_MONITOR:
        initializeAIM(devInfo, name, debugIpData) ;
        break ;
      case AXI_STREAM_MONITOR:
        initializeASM(devInfo, name, debugIpData) ;
       break ;
      case AXI_NOC:
        initializeNOC(devInfo, debugIpData) ;
        break ;
      case TRACE_S2MM:
        initializeTS2MM(devInfo, debugIpData) ;
        break ;
      case AXI_MONITOR_FIFO_LITE:
        initializeFIFO(devInfo) ;
        break ;
      default:
        break ;
      }
    }

    return true;
  }

} // end namespace xdp
