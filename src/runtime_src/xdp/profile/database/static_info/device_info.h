/**
 * Copyright (C) 2021-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef DEVICE_INFO_DOT_H
#define DEVICE_INFO_DOT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/common/system.h"

#include "xdp/config.h"
#include "xdp/profile/database/static_info/xclbin_types.h"

namespace xdp {

  // Forward declarations
  struct XclbinInfo ;
  struct Monitor ;
  struct NoCNode ;
  class aie_cfg_tile ;
  struct ConfigInfo ;

  // An application may be run on a system that has multiple physical
  //  (or emulated) devices.  The DeviceInfo struct collects all of the
  //  information related to a single physical (or emulated) device.
  //  As an application may load multiple xclbins onto the device, the
  //  DeviceInfo struct is responsible for keeping a history of all the
  //  history of xclbin information as the application progresses.
  struct DeviceInfo
  {
    // ***********************************************************
    // ****** Known information regardless of loaded XCLBIN ******
    // ***********************************************************

    // A unique number assigned by XDP.  This is not to be confused
    //  with any identifier used by the XRT side.  It should correlate
    //  with the id stored in VPDatabase, which differentiates devices
    //  based on the location of the sysfs path.
    uint64_t deviceId ;

    // A unique name for each device based on its shell and the deviceId
    std::string deviceName ;

    // Information about the shell that will never change
    uint64_t kdmaCount = 0 ;
    bool isEdgeDevice  = false ;
    bool isReady       = false ;
    bool isNoDMADevice = false ;

    // *******************************************************************
    // ****** Information specific to all previously loaded XCLBINs ******
    // *******************************************************************
    std::vector<std::unique_ptr<ConfigInfo>> loadedConfigInfos ;

    // Our AMs don't currently support profiling kernels that were compiled
    //  as multiple context kernels.  We call the XRT function
    //  "get_kernel_channel_info()" and store the result here.  If this string
    //  is non-empty, we disable all of our AMs as we won't be able to
    //  distinguish which kernels are multiple context kernels.  This
    //  string is reset every time we load a new xclbin
    std::string ctxInfo ;

    // The maximum bit width of an AXI connection between compute units
    // and memory.  This maximum is the same regardless of the xclbin loaded.
    uint32_t maxConnectionBitWidth = 512;

    // Generation of AI Engine hardware on device, regardless of xclbin loaded.
    uint8_t aieGeneration = 1;

    ~DeviceInfo() ;

    // ****** Functions for Device ConfigInfo ******
    XDP_CORE_EXPORT XclbinInfo* createXclbinFromLastConfig(XclbinInfoType xclbinQueryType) ;
    XDP_CORE_EXPORT void createConfig(XclbinInfo* xclbin) ;
    
    // ****** Functions for information on the device ******
    XDP_CORE_EXPORT std::string getUniqueDeviceName() const ;
    XDP_CORE_EXPORT xrt_core::uuid currentXclbinUUID() ;

    // ****** Functions for information on the device for the current config ******
    const std::vector<std::unique_ptr<ConfigInfo>>& getLoadedConfigs() const { return loadedConfigInfos ;}
    XDP_CORE_EXPORT ConfigInfo* currentConfig() const ;
    XDP_CORE_EXPORT void cleanCurrentConfig(XclbinInfoType type);
    inline bool isNoDMA() const { return isNoDMADevice ; }
    double getMaxClockRatePLMHz();

    void setAIEGeneration(uint8_t hw_gen) { aieGeneration = hw_gen; }
    inline uint8_t getAIEGeneration() const { return aieGeneration ; }

    // ****** Functions for information on the currently loaded xclbin *******
    XDP_CORE_EXPORT bool hasDMAMonitor() ;
    XDP_CORE_EXPORT bool hasDMABypassMonitor() ;
    XDP_CORE_EXPORT bool hasKDMAMonitor() ;
    XDP_CORE_EXPORT bool hasAIMNamed(const std::string& name) ;

    // ****** Functions for PL information on a specific xclbin ******
    XDP_CORE_EXPORT bool hasFloatingAIMWithTrace(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT bool hasFloatingASMWithTrace(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumAM(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumUserAMWithTrace(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumAIM(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumUserAIM(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumUserAIMWithTrace(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumASM(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumUserASM(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT uint64_t getNumUserASMWithTrace(XclbinInfo* xclbin) const ;

    // Functions that get specific information on monitors
    XDP_CORE_EXPORT Monitor* getAMonitor(XclbinInfo* xclbin, uint64_t slotId) ;
    XDP_CORE_EXPORT Monitor* getAIMonitor(XclbinInfo* xclbin, uint64_t slotId) ;
    XDP_CORE_EXPORT Monitor* getASMonitor(XclbinInfo* xclbin, uint64_t slotId) ;

    XDP_CORE_EXPORT std::vector<Monitor*>* getAIMonitors(XclbinInfo* xclbin) ;
    XDP_CORE_EXPORT std::vector<Monitor*>* getASMonitors(XclbinInfo* xclbin) ;
    XDP_CORE_EXPORT std::vector<Monitor*> getUserAIMsWithTrace(XclbinInfo* xclbin) ;
    XDP_CORE_EXPORT std::vector<Monitor*> getUserASMsWithTrace(XclbinInfo* xclbin) ;
    
    // ****** Functions for AIE information on a specific xclbin ******
    XDP_CORE_EXPORT uint64_t getNumNOC(XclbinInfo* xclbin) const ;
    XDP_CORE_EXPORT NoCNode* getNOC(XclbinInfo* xclbin, uint64_t idx) ;

    // ****** Functions for AIE information on the current xclbin ******
    XDP_CORE_EXPORT
    void addTraceGMIO(uint32_t i, uint8_t col, uint8_t num, uint8_t stream,
                      uint8_t len) ;
    XDP_CORE_EXPORT
    void addAIECounter(uint32_t i, uint8_t col, uint8_t row, uint8_t num,
                       uint16_t start, uint16_t end, uint8_t reset,
                       uint64_t load, double freq, const std::string& mod,
                       const std::string& aieName, uint8_t streamId=0) ;
    XDP_CORE_EXPORT
    void addAIECounterResources(uint32_t numCounters, uint32_t numTiles,
                                uint8_t moduleType) ;
    XDP_CORE_EXPORT
    void addAIECoreEventResources(uint32_t numEvents, uint32_t numTiles) ;
    XDP_CORE_EXPORT
    void addAIEMemoryEventResources(uint32_t numEvents, uint32_t numTiles) ;
    XDP_CORE_EXPORT
    void addAIEShimEventResources(uint32_t numEvents, uint32_t numTiles) ;
    XDP_CORE_EXPORT
    void addAIEMemTileEventResources(uint32_t numEvents, uint32_t numTiles) ;
    XDP_CORE_EXPORT
    void addAIECfgTile(std::unique_ptr<aie_cfg_tile>& tile) ;
  } ;

} // end namespace xdp

#endif
