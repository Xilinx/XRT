/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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
#include "core/include/xclbin.h"

#define XDP_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/device_info.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"

#define XAM_STALL_PROPERTY_MASK        0x4
#define XMON_TRACE_PROPERTY_MASK       0x1

namespace xdp {

std::string convertMemoryName(std::string mem)
{
  if (0 == mem.compare("DDR[0]"))
    return "bank0";
  if (0 == mem.compare("DDR[1]"))
    return "bank1";
  if (0 == mem.compare("DDR[2]"))
    return "bank2";
  if (0 == mem.compare("DDR[3]"))
    return "bank3";

  return mem;
}

  VPStaticDatabase::VPStaticDatabase(VPDatabase* d)
    : db(d)
    , runSummary(nullptr)
    , systemDiagram("")
    , softwareEmulationDeviceName("")
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
    if (runSummary != nullptr) {
      runSummary->write(false) ;
      delete runSummary ;
    }

    // AIE specific functions
    if (aieDevice != nullptr && deallocateAieDevice != nullptr)
      deallocateAieDevice(aieDevice) ;

    for (auto iter : deviceInfo) {
      delete iter.second ;
    }
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
        runSummary = new VPRunSummaryWriter("xrt.run_summary", db) ;
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
    return deviceInfo[deviceId] ;
  }

  std::vector<std::string> VPStaticDatabase::getDeviceNames()
  {
    std::vector<std::string> uniqueNames ;
    std::lock_guard<std::mutex> lock(deviceLock) ;

    for (auto device : deviceInfo) {
      uniqueNames.push_back(device.second->getUniqueDeviceName()) ;
    }
    return uniqueNames ;
  }

  std::vector<DeviceInfo*> VPStaticDatabase::getDeviceInfos()
  {
    std::vector<DeviceInfo*> infos ;
    std::lock_guard<std::mutex> lock(deviceLock) ;

    for (auto device : deviceInfo) {
      infos.push_back(device.second) ;
    }
    return infos ;
  }

  bool VPStaticDatabase::hasStallInfo()
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    for (auto device : deviceInfo) {
      for (auto xclbin : device.second->getLoadedXclbins()) {
        for (auto cu : xclbin->pl.cus) {
          if (cu.second->getStallEnabled())
            return true ;
        }
      }
    }
    return false ;
  }

  XclbinInfo* VPStaticDatabase::getCurrentlyLoadedXclbin(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    return deviceInfo[deviceId]->currentXclbin() ;
  }

  void VPStaticDatabase::deleteCurrentlyUsedDeviceInterface(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;
    if (xclbin->deviceIntf) {
      delete xclbin->deviceIntf ;
      xclbin->deviceIntf = nullptr ;
    }
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

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return PL ? 300.0 : 1000.0 ;

    if (PL)
      return xclbin->pl.clockRatePLMHz ;
    else
      return xclbin->aie.clockRateAIEMHz ;
  }

  void
  VPStaticDatabase::setDeviceName(uint64_t deviceId, const std::string& name)
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

  void VPStaticDatabase::setDeviceIntf(uint64_t deviceId, DeviceIntf* devIntf)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;
    xclbin->deviceIntf = devIntf ;
  }

  DeviceIntf* VPStaticDatabase::getDeviceIntf(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;
    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return xclbin->deviceIntf ;
  }

  void VPStaticDatabase::setKDMACount(uint64_t deviceId, uint64_t num)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->kdmaCount = num ;
  }

  uint64_t VPStaticDatabase::getKDMACount(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;
    return deviceInfo[deviceId]->kdmaCount ;
  }

  void VPStaticDatabase::setMaxReadBW(uint64_t deviceId, double bw)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;
    xclbin->pl.maxReadBW = bw ;
  }

  double VPStaticDatabase::getMaxReadBW(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0.0 ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return 0.0 ;

    return xclbin->pl.maxReadBW ;
  }

  void VPStaticDatabase::setMaxWriteBW(uint64_t deviceId, double bw)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->pl.maxWriteBW = bw ;
  }

  double VPStaticDatabase::getMaxWriteBW(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0.0 ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return 0.0 ;

    return xclbin->pl.maxWriteBW ;
  }

  std::string VPStaticDatabase::getXclbinName(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return "" ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return "" ;

    return xclbin->name ;
  }

  std::vector<XclbinInfo*> VPStaticDatabase::getLoadedXclbins(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end()) {
      std::vector<XclbinInfo*> blank ;
      return blank ;
    }
    return deviceInfo[deviceId]->getLoadedXclbins() ;
  }

  ComputeUnitInstance* VPStaticDatabase::getCU(uint64_t deviceId, int32_t cuId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return xclbin->pl.cus[cuId] ;
  }

  std::map<int32_t, ComputeUnitInstance*>*
  VPStaticDatabase::getCUs(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->pl.cus) ;
  }

  std::map<int32_t, Memory*>*
  VPStaticDatabase::getMemoryInfo(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->pl.memoryInfo) ;
  }

  Memory* VPStaticDatabase::getMemory(uint64_t deviceId, int32_t memId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

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

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;

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

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;

    // User space AM in sorted order of their slotIds.  Matches with
    //  sorted list of AM in xdp::DeviceIntf
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
  bool VPStaticDatabase::isAIECounterRead(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return false ;

    for (auto xclbin : deviceInfo[deviceId]->getLoadedXclbins()) {
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
    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.isAIEcounterRead = val ;
  }

  void VPStaticDatabase::setIsGMIORead(uint64_t deviceId, bool val)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return ;

    xclbin->aie.isGMIORead = val ;
  }

  bool VPStaticDatabase::isGMIORead(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return false ;
    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return false ;
    return xclbin->aie.isGMIORead ;
  }

  uint64_t VPStaticDatabase::getNumAIECounter(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return 0 ;

    return xclbin->aie.aieList.size() ;
  }

  uint64_t VPStaticDatabase::getNumTraceGMIO(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return 0 ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return 0 ;

    return xclbin->aie.gmioList.size() ;
  }

  AIECounter* VPStaticDatabase::getAIECounter(uint64_t deviceId, uint64_t idx)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return xclbin->aie.aieList[idx] ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIECoreCounterResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->aie.aieCoreCountersMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEMemoryCounterResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->aie.aieMemoryCountersMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEShimCounterResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->aie.aieShimCountersMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIECoreEventResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->aie.aieCoreEventsMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEMemoryEventResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->aie.aieMemoryEventsMap) ;
  }

  std::map<uint32_t, uint32_t>*
  VPStaticDatabase::getAIEShimEventResources(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->aie.aieShimEventsMap) ;
  }

  std::vector<std::unique_ptr<aie_cfg_tile>>*
  VPStaticDatabase::getAIECfgTiles(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    return &(xclbin->aie.aieCfgList) ;
  }

  TraceGMIO* VPStaticDatabase::getTraceGMIO(uint64_t deviceId, uint64_t idx)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return nullptr ;

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return nullptr ;

    if (xclbin->aie.gmioList.size() <= idx)
      return nullptr ;
    return xclbin->aie.gmioList[idx] ;
  }

  void VPStaticDatabase::addTraceGMIO(uint64_t deviceId, uint32_t i,
                                      uint16_t col, uint16_t num,
                                      uint16_t stream, uint16_t len)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addTraceGMIO(i, col, num, stream, len) ;
  }

  void VPStaticDatabase::addAIECounter(uint64_t deviceId, uint32_t i,
                                       uint16_t col, uint16_t r, uint8_t num,
                                       uint16_t start, uint16_t end,
                                       uint8_t reset, uint32_t load,
                                       double freq, const std::string& mod,
                                       const std::string& aieName)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIECounter(i, col, r, num, start, end, reset,
                                        load, freq, mod, aieName) ;
  }

  void VPStaticDatabase::addAIECounterResources(uint64_t deviceId,
                                                uint32_t numCounters,
                                                uint32_t numTiles,
                                                bool isCore)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;

    if (deviceInfo.find(deviceId) == deviceInfo.end())
      return ;
    deviceInfo[deviceId]->addAIECounterResources(numCounters, numTiles, isCore);
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

    XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
    if (!xclbin)
      return 0 ;
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

      XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
      if (!xclbin)
        return 0 ;
      return xclbin->aie.gmioList.size() ;
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
  // ***** Functions for  *****


  bool VPStaticDatabase::validXclbin(void* devHandle)
  {
    std::shared_ptr<xrt_core::device> device =
      xrt_core::get_userpf_device(devHandle);

    // If this xclbin was built with tools before the 2019.2 release, we
    //  do not support profiling
    std::pair<const char*, size_t> buildMetadata =
      device->get_axlf_section(BUILD_METADATA);
    const char* buildMetadataSection = buildMetadata.first;
    size_t      buildMetadataSz      = buildMetadata.second;
    if (buildMetadataSection != nullptr) {
      std::stringstream ss ;
      ss.write(buildMetadataSection, buildMetadataSz) ;

      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      std::string version =
        pt.get<std::string>("build_metadata.xclbin.generated_by.version", "") ;

      if (version == "" || version.find(".") == std::string::npos)
        return false ;

      // The stod function handles strings that have one decimal point
      //  or multiple decimal points and return only the major number
      //  and minor number and strips away the revision.
      double majorAndMinor = std::stod(version, nullptr) ;
      if (majorAndMinor < earliestSupportedToolVersion())
        return false ;

      return true ;
    }

    // If the build section does not exist, then this is not a valid xclbin
    return false ;
  }







  double VPStaticDatabase::findClockRate(std::shared_ptr<xrt_core::device> device)
  {
    double defaultClockSpeed = 300.0 ;

    // First, check the clock frequency topology
    const clock_freq_topology* clockSection =
      device->get_axlf_section<const clock_freq_topology*>(CLOCK_FREQ_TOPOLOGY);

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
      std::pair<const char*, size_t> metadataSection =
        device->get_axlf_section(EMBEDDED_METADATA) ;
      const char* rawXml = metadataSection.first ;
      size_t xmlSize = metadataSection.second ;
      if (rawXml == nullptr || xmlSize == 0)
        return defaultClockSpeed ;

      // Convert the raw character stream into a boost::property_tree
      std::string xmlFile ;
      xmlFile.assign(rawXml, xmlSize) ;
      std::stringstream xmlStream ;
      xmlStream << xmlFile ;
      boost::property_tree::ptree xmlProject ;
      boost::property_tree::read_xml(xmlStream, xmlProject) ;

      // Dig in and find all of the kernel clocks
      for (auto& clock : xmlProject.get_child("project.platform.device.core.kernelClocks")) {
        if (clock.first != "clock")
          continue ;

        try {
          std::string port = clock.second.get<std::string>("<xmlattr>.port") ;
          if (port != "DATA_CLK")
            continue ;
          std::string freq = clock.second.get<std::string>("<xmlattr>.frequency") ;
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

    // We didn't find it in any section, so just assume 300 MHz for now
    return defaultClockSpeed ;
  }

  // This function is called whenever a device is loaded with an 
  //  xclbin.  It has to clear out any previous device information and
  //  reload our information.
  void VPStaticDatabase::updateDevice(uint64_t deviceId, void* devHandle)
  {
    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(devHandle);
    if(nullptr == device) return;

    if(false == resetDeviceInfo(deviceId, device)) {
      /* If multiple plugins are enabled for the current run, the first plugin has already updated device information
       * in the static data base. So, no need to read the xclbin information again.
       */
      return;
    }
    
    // We need to update the device, but if we had an xclbin previously loaded
    //  then we need to mark it
    if (deviceInfo.find(deviceId) != deviceInfo.end()) {
      XclbinInfo* xclbin = deviceInfo[deviceId]->currentXclbin() ;
      if (xclbin)
        db->getDynamicInfo().markXclbinEnd(deviceId) ;
    }

    DeviceInfo* devInfo = nullptr ;
    auto itr = deviceInfo.find(deviceId);
    if (itr == deviceInfo.end()) {
      // This is the first time this device was loaded with an xclbin
      devInfo = new DeviceInfo();
      devInfo->deviceId = deviceId ;
      if (isEdge()) devInfo->isEdgeDevice = true ;
      deviceInfo[deviceId] = devInfo ;

    } else {
      // This is a previously used device being reloaded with a new xclbin
      devInfo = itr->second ;
      devInfo->cleanCurrentXclbinInfo() ;
    }
    
    XclbinInfo* currentXclbin = new XclbinInfo() ;
    currentXclbin->uuid = device->get_xclbin_uuid() ;
    currentXclbin->pl.clockRatePLMHz = findClockRate(device) ;


    /* Configure AMs if context monitoring is supported
     * else disable alll AMs on this device
     */
    devInfo->ctxInfo = xrt_core::config::get_kernel_channel_info();

    if (!setXclbinName(currentXclbin, device)) {
      // If there is no SYSTEM_METADATA section, use a default name
      currentXclbin->name = "default.xclbin";
    }
    if (!initializeComputeUnits(currentXclbin, device)) {
      delete currentXclbin;
      return;
    }

    devInfo->addXclbin(currentXclbin);
    initializeProfileMonitors(devInfo, device);
    devInfo->isReady = true;
  }

  // Return true if we should reset the device information.
  // Return false if we should not reset device information
  bool VPStaticDatabase::resetDeviceInfo(uint64_t deviceId, const std::shared_ptr<xrt_core::device>& device)
  {
    std::lock_guard<std::mutex> lock(deviceLock);

    auto itr = deviceInfo.find(deviceId);
    if(itr != deviceInfo.end()) {
      DeviceInfo *devInfo = itr->second;
      // Are we attempting to load the same xclbin multiple times?
      XclbinInfo* xclbin = devInfo->currentXclbin() ;
      if (xclbin && device->get_xclbin_uuid() == xclbin->uuid) {
        return false ;
      }
    }
    return true;
  }

  bool VPStaticDatabase::setXclbinName(XclbinInfo* currentXclbin, const std::shared_ptr<xrt_core::device>& device)
  {
    // Get SYSTEM_METADATA section
    std::pair<const char*, size_t> systemMetadata = device->get_axlf_section(SYSTEM_METADATA);
    const char* systemMetadataSection = systemMetadata.first;
    size_t      systemMetadataSz      = systemMetadata.second;
    if(systemMetadataSection == nullptr) return false;

    // For now, also update the System metadata for the run summary.
    //  TODO: Expand this so that multiple devices and multiple xclbins
    //  don't overwrite the single system diagram information
    std::ostringstream buf ;
    for (size_t index = 0 ; index < systemMetadataSz ; ++index)
    {
      buf << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)(systemMetadataSection[index]);
    }
    {
      std::lock_guard<std::mutex> lock(summaryLock) ;
      systemDiagram = buf.str() ;
    }

    try {
      std::stringstream ss;
      ss.write(systemMetadataSection, systemMetadataSz);

      // Create a property tree and determine if the variables are all default values
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      currentXclbin->name = pt.get<std::string>("system_diagram_metadata.xclbin.generated_by.xclbin_name", "");
      if(!currentXclbin->name.empty()) {
        currentXclbin->name += ".xclbin";
      }
    } catch(...) {
      // keep default value in "currentXclbin.name" i.e. empty string
    }
    return true;
  }

  bool VPStaticDatabase::initializeComputeUnits(XclbinInfo* currentXclbin, const std::shared_ptr<xrt_core::device>& device)
  {
    // Get IP_LAYOUT section 
    const ip_layout* ipLayoutSection = device->get_axlf_section<const ip_layout*>(IP_LAYOUT);
    if(ipLayoutSection == nullptr) return true;

    ComputeUnitInstance* cu = nullptr;
    for(int32_t i = 0; i < ipLayoutSection->m_count; i++) {
      const struct ip_data* ipData = &(ipLayoutSection->m_ip_data[i]);
      if(ipData->m_type != IP_KERNEL) {
        continue;
      }
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
    }

    // Get MEM_TOPOLOGY section 
    const mem_topology* memTopologySection = device->get_axlf_section<const mem_topology*>(MEM_TOPOLOGY);
    if(memTopologySection == nullptr) return false;

    for(int32_t i = 0; i < memTopologySection->m_count; i++) {
      const struct mem_data* memData = &(memTopologySection->m_mem_data[i]);
      currentXclbin->pl.memoryInfo[i] = new Memory(memData->m_type, i, memData->m_base_address, memData->m_size,
                                          reinterpret_cast<const char*>(memData->m_tag), memData->m_used);
    }

    // Look into the connectivity section and load information about Compute Units and their Memory connections
    // Get CONNECTIVITY section
    const connectivity* connectivitySection = device->get_axlf_section<const connectivity*>(CONNECTIVITY);    
    if(connectivitySection == nullptr) return true;

    // Now make the connections
    cu = nullptr;
    for(int32_t i = 0; i < connectivitySection->m_count; i++) {
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

    // Set Static WorkGroup Size of CUs using the EMBEDDED_METADATA section
    std::pair<const char*, size_t> embeddedMetadata = device->get_axlf_section(EMBEDDED_METADATA);
    const char* embeddedMetadataSection = embeddedMetadata.first;
    size_t      embeddedMetadataSz      = embeddedMetadata.second;

    boost::property_tree::ptree xmlProject;
    std::stringstream xmlStream;
    xmlStream.write(embeddedMetadataSection, embeddedMetadataSz);
    boost::property_tree::read_xml(xmlStream, xmlProject);

    for(auto coreItem : xmlProject.get_child("project.platform.device.core")) {
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
      for(auto cuItr : currentXclbin->pl.cus) {
        if(0 != cuItr.second->getKernelName().compare(kernelName)) {
          continue;
        }
        cuItr.second->setDim(std::stoi(x), std::stoi(y), std::stoi(z));
      }
    }

    return true;
  }

  void VPStaticDatabase::initializeAM(DeviceInfo* devInfo,
                                      const std::string& name,
                                      const struct debug_ip_data* debugIpData)
  {
    XclbinInfo* xclbin = devInfo->currentXclbin() ;
    if (!xclbin) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an AM without a loaded xclbin") ;
      return ;
    }

    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);

    // Find the compute unit that this AM is attached to.
    for (auto cu : xclbin->pl.cus) {
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
    XclbinInfo* xclbin = devInfo->currentXclbin() ;
    if (!xclbin) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an AIM without a loaded xclbin") ;
      return ;
    }

    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    if (index < MIN_TRACE_ID_AIM) {
      std::stringstream msg;
      msg << "AIM with incorrect index: " << index ;
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              msg.str());
      index = MIN_TRACE_ID_AIM ;
    }

    // Parse name to find CU Name and Memory.  We expect the name in
    //  debug_ip_layout to be in the form of "cu_name/memory_name-port_name"
    size_t pos = name.find('/');
    std::string monCuName = name.substr(0, pos);

    if (monCuName == "memory_subsystem") {
      if (xclbin)
        xclbin->pl.hasMemoryAIM = true ;
    }

    std::string memName = "" ;
    std::string memName1 = "" ;
    std::string portName = "" ;
    size_t pos1 = name.find('-');
    if(pos1 != std::string::npos) {
      memName = name.substr(pos1+1);
      portName = name.substr(pos+1, pos1-pos-1);
      memName1 = convertMemoryName(memName);
    }

    ComputeUnitInstance* cuObj = nullptr ;
    int32_t cuId = -1 ;
    int32_t memId = -1;

    // Find both the compute unit this AIM is attached to (if applicable)
    //  and the memory this AIM is attached to (if applicable).
    for(auto cu : xclbin->pl.cus) {
      if(0 == monCuName.compare(cu.second->getName())) {
        cuId = cu.second->getIndex();
        cuObj = cu.second;
        break;
      }
    }
    for(auto mem : xclbin->pl.memoryInfo) {
      if(0 == memName1.compare(mem.second->name)) {
        memId = mem.second->index;
        break;
      }
    }
    Monitor* mon = new Monitor(static_cast<DEBUG_IP_TYPE>(debugIpData->m_type),
                               index, debugIpData->m_name, cuId, memId);
    mon->port = portName;
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
    XclbinInfo* xclbin = devInfo->currentXclbin() ;
    if (!xclbin) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                              "Attempt to initialize an ASM without a loaded xclbin") ;
      return ;
    }

    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    if (index < MIN_TRACE_ID_ASM) {
      std::stringstream msg;
      msg << "ASM with incorrect index: " << index ;
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              msg.str());
      index = MIN_TRACE_ID_ASM ;
    }

    // Parse out the name of the compute unit this monitor is attached to if
    //  possible.  We expect the name in debug_ip_layout to be in the form of
    //  compute_unit_name/port_name.  If this is a floating 

    size_t pos = name.find('/');
    std::string monCuName = name.substr(0, pos);

    std::string portName = "";
    ComputeUnitInstance* cuObj = nullptr ;
    int32_t cuId = -1 ;

    for(auto cu : xclbin->pl.cus) {
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

        for(auto cu : xclbin->pl.cus) {
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
    mon->port = portName;
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
    XclbinInfo* xclbin = devInfo->currentXclbin() ;
    if (!xclbin)
      return ;

    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    uint8_t readTrafficClass  = debugIpData->m_properties >> 2;
    uint8_t writeTrafficClass = debugIpData->m_properties & 0x3;

    NoCNode* noc = new NoCNode(index, debugIpData->m_name, readTrafficClass,
                               writeTrafficClass) ;
    xclbin->aie.nocList.push_back(noc) ;
    // nocList in xdp::DeviceIntf is sorted; Is that required here?
  }

  void VPStaticDatabase::initializeTS2MM(DeviceInfo* devInfo,
                                         const struct debug_ip_data* debugIpData)
  {
    XclbinInfo* xclbin = devInfo->currentXclbin() ;
    if (!xclbin)
      return ;

    // TS2MM IP for either AIE PLIO or PL trace offload
    if (debugIpData->m_properties & 0x1)
      xclbin->aie.numTracePLIO++ ;
    else
      xclbin->pl.usesTs2mm = true ;
  }

  bool VPStaticDatabase::initializeProfileMonitors(DeviceInfo* devInfo, const std::shared_ptr<xrt_core::device>& device)
  {
    // Look into the debug_ip_layout section and load information about Profile Monitors
    // Get DEBUG_IP_LAYOUT section
    const debug_ip_layout* debugIpLayoutSection =
      device->get_axlf_section<const debug_ip_layout*>(DEBUG_IP_LAYOUT);
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
      default:
        break ;
      }
    }

    return true; 
  }



  void VPStaticDatabase::addCommandQueueAddress(uint64_t a)
  {
    std::lock_guard<std::mutex> lock(openCLLock) ;

    commandQueueAddresses.emplace(a) ;
  }



} // end namespace xdp
