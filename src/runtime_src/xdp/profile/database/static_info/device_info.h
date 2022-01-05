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

#ifndef DEVICE_INFO_DOT_H
#define DEVICE_INFO_DOT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/common/system.h"

#include "xdp/config.h"

namespace xdp {

  // Forward declarations
  struct XclbinInfo ;
  struct Monitor ;
  struct NoCNode ;
  class aie_cfg_tile ;

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

    // *******************************************************************
    // ****** Information specific to all previously loaded XCLBINs ******
    // *******************************************************************
    std::vector<XclbinInfo*> loadedXclbins ;

    // Our AMs don't currently support profiling kernels that were compiled
    //  as multiple context kernels.  We call the XRT function
    //  "get_kernel_channel_info()" and store the result here.  If this string
    //  is non-empty, we disable all of our AMs as we won't be able to
    //  distinguish which kernels are multiple context kernels.  This
    //  string is reset every time we load a new xclbin
    std::string ctxInfo ;

    ~DeviceInfo() ;

    // ****** Functions for information on the device ******
    XDP_EXPORT std::string getUniqueDeviceName() const ;
    XDP_EXPORT xrt_core::uuid currentXclbinUUID() ;
    inline std::vector<XclbinInfo*> getLoadedXclbins() { return loadedXclbins ;}
    XDP_EXPORT void cleanCurrentXclbinInfo() ;

    // ****** Functions for information on the currently loaded xclbin *******
    XDP_EXPORT XclbinInfo* currentXclbin() ;
    XDP_EXPORT void addXclbin(XclbinInfo* xclbin) ;
    XDP_EXPORT bool hasDMAMonitor() ;
    XDP_EXPORT bool hasDMABypassMonitor() ;
    XDP_EXPORT bool hasKDMAMonitor() ;
    XDP_EXPORT bool hasAIMNamed(const std::string& name) ;

    // ****** Functions for PL information on a specific xclbin ******
    XDP_EXPORT bool hasFloatingAIMWithTrace(XclbinInfo* xclbin) const ;
    XDP_EXPORT bool hasFloatingASMWithTrace(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumAM(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumUserAMWithTrace(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumAIM(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumUserAIM(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumUserAIMWithTrace(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumASM(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumUserASM(XclbinInfo* xclbin) const ;
    XDP_EXPORT uint64_t getNumUserASMWithTrace(XclbinInfo* xclbin) const ;

    // Functions that get specific information on monitors
    XDP_EXPORT Monitor* getAMonitor(XclbinInfo* xclbin, uint64_t slotId) ;
    XDP_EXPORT Monitor* getAIMonitor(XclbinInfo* xclbin, uint64_t slotId) ;
    XDP_EXPORT Monitor* getASMonitor(XclbinInfo* xclbin, uint64_t slotId) ;

    XDP_EXPORT std::vector<Monitor*>* getAIMonitors(XclbinInfo* xclbin) ;
    XDP_EXPORT std::vector<Monitor*>* getASMonitors(XclbinInfo* xclbin) ;
    XDP_EXPORT std::vector<Monitor*> getUserAIMsWithTrace(XclbinInfo* xclbin) ;
    XDP_EXPORT std::vector<Monitor*> getUserASMsWithTrace(XclbinInfo* xclbin) ;
    
    // ****** Functions for AIE information on a specific xclbin ******
    XDP_EXPORT uint64_t getNumNOC(XclbinInfo* xclbin) const ;
    XDP_EXPORT NoCNode* getNOC(XclbinInfo* xclbin, uint64_t idx) ;

    // ****** Functions for AIE information on the current xclbin ******
    XDP_EXPORT
    void addTraceGMIO(uint32_t i, uint16_t col, uint16_t num, uint16_t stream,
                      uint16_t len) ;
    XDP_EXPORT
    void addAIECounter(uint32_t i, uint16_t col, uint16_t r, uint8_t num,
                       uint16_t start, uint16_t end, uint8_t reset,
                       double freq, const std::string& mod,
                       const std::string& aieName) ;
    XDP_EXPORT
    void addAIECounterResources(uint32_t numCounters, uint32_t numTiles,
                                uint8_t moduleType) ;
    XDP_EXPORT
    void addAIECoreEventResources(uint32_t numEvents, uint32_t numTiles) ;
    XDP_EXPORT
    void addAIEMemoryEventResources(uint32_t numEvents, uint32_t numTiles) ;
    XDP_EXPORT
    void addAIEShimEventResources(uint32_t numEvents, uint32_t numTiles) ;
    XDP_EXPORT
    void addAIECfgTile(std::unique_ptr<aie_cfg_tile>& tile) ;
  } ;

} // end namespace xdp

#endif
