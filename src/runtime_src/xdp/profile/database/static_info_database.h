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

#ifndef STATIC_INFO_DATABASE_DOT_H
#define STATIC_INFO_DATABASE_DOT_H

#include <list>
#include <map>
#include <memory> // for unique_ptr
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "core/common/system.h"
#include "core/common/device.h"

#include "xdp/config.h"

namespace xdp {

  // Forward declarations of general XDP constructs
  class VPDatabase;
  class VPWriter;
  class DeviceIntf ;

  // Forward declarations of PL contents
  struct Monitor ;
  struct Memory ;
  class ComputeUnitInstance ;

  // Forward declarations of AIE contents
  struct AIECounter ;
  struct TraceGMIO ;
  struct NoCNode ;
  class aie_cfg_tile ;

  // Forward declarations of device and xclbin contents
  struct DeviceInfo ;
  struct XclbinInfo ;

  // The VPStaticDatabase contains information that is expected to not change
  //  throughout the execution of the program.  For device information,
  //  we keep track of the structure of the hardware in all the xclbins
  //  that are loaded per device.  While each part of the hardware can only
  //  have one configuration at a time, we must keep information on all the
  //  xclbins we have seen so we can provide a complete picture at the
  //  end of the application when we dump summary information.
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
    uint64_t applicationStartTime = 0 ;
    
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

    // Device Specific Information mapped to the Unique Device Id
    std::map<uint64_t, DeviceInfo*> deviceInfo;

    // Static info can be accessed via any host thread, so we have
    //  fine grained locks on each of the types of data.
    std::mutex summaryLock ;
    std::mutex openCLLock ;
    std::mutex deviceLock ;
    std::mutex aieLock ;

    // AIE device (Supported devices only)
    void* aieDevInst = nullptr ; // XAie_DevInst
    void* aieDevice = nullptr ; // xaiefal::XAieDev
    std::function<void (void*)> deallocateAieDevice = nullptr ;

    bool resetDeviceInfo(uint64_t deviceId, const std::shared_ptr<xrt_core::device>& device);

    // Helper functions that fill in device information
    bool setXclbinName(XclbinInfo*, const std::shared_ptr<xrt_core::device>& device);
    bool initializeComputeUnits(XclbinInfo*, const std::shared_ptr<xrt_core::device>&);
    bool initializeProfileMonitors(DeviceInfo*, const std::shared_ptr<xrt_core::device>&);
    void initializeAM(DeviceInfo* devInfo, const std::string& name,
                      const struct debug_ip_data* debugIpData) ;
    void initializeAIM(DeviceInfo* devInfo, const std::string& name,
                       const struct debug_ip_data* debugIpData) ;
    void initializeASM(DeviceInfo* devInfo, const std::string& name,
                       const struct debug_ip_data* debugIpData) ;
    void initializeNOC(DeviceInfo* devInfo,
                       const struct debug_ip_data* debugIpData) ;
    void initializeTS2MM(DeviceInfo* devInfo,
                         const struct debug_ip_data* debugIpData) ;
    double findClockRate(std::shared_ptr<xrt_core::device> device) ;

  public:
    VPStaticDatabase(VPDatabase* d) ;
    ~VPStaticDatabase() ;

    // ********************************************************
    // ***** Functions related to the running application *****
    XDP_EXPORT int getPid() const ;

    // The first profiling plugin loaded sets the application start time.
    //  It does not capture the true application start time, but rather
    //  the earliest time our constructs can capture when the shared libraries
    //  are loaded.
    XDP_EXPORT uint64_t getApplicationStartTime() const ;
    XDP_EXPORT void setApplicationStartTime(uint64_t t) ;

    // Due to changes in hardware IP, we can only support profiling on
    //  xclbins built using 2019.2 or later tools.
    inline double earliestSupportedToolVersion() const { return 2019.2 ; }
    XDP_EXPORT bool validXclbin(void* devHandle) ;

    // ****************************************************
    // ***** Functions related to OpenCL information. *****
    //  These are only used in OpenCL applications and called from
    //  OpenCL plugins.

    // OpenCL applications can create and destroy any number of command
    //  queues.  We keep track of the ones that were used (not every
    //  one that was created).
    XDP_EXPORT std::set<uint64_t>& getCommandQueueAddresses() ;
    XDP_EXPORT void addCommandQueueAddress(uint64_t a) ;

    // For every OpenCL EnqueueNDRange or EnqueueTask call, we keep
    //  track of the device:binary:kernel information so we can display
    //  it in the trace as a separate row.  The names are specific
    //  to the OpenCL layer.
    XDP_EXPORT std::set<std::string>& getEnqueuedKernels() ;
    XDP_EXPORT void addEnqueuedKernel(const std::string& identifier) ;

    // OpenCL can group devices into contexts.  These functions keep
    //  track of this information
    XDP_EXPORT void setNumDevices(uint64_t contextId, uint64_t numDevices) ;
    XDP_EXPORT uint64_t getNumDevices(uint64_t contextId) ;

    // We do not have device information for software emulation, so
    //  we pick up the name of the device in OpenCL when a kernel is executed.
    //  We assume there is only one device in software emulation.
    XDP_EXPORT std::string getSoftwareEmulationDeviceName() ;
    XDP_EXPORT void setSoftwareEmulationDeviceName(const std::string& name) ;

    // In software emulation, we can pick up information on exactly which
    //  compute units are used in each kernel purely from OpenCL calls
    //  (without any hardware monitors).  We collect this information
    //  to summarize CU usage and provide guidance.
    XDP_EXPORT std::map<std::string, uint64_t> getSoftwareEmulationCUCounts() ;
    XDP_EXPORT void addSoftwareEmulationCUInstance(const std::string& kernelName) ;

    // We provide guidance on what memory resources are used on each device.
    //  For software emulation, since we do not have device information,
    //  we need to dig into the xclbin from OpenCL constructs.  We store
    //  that information here since there is no DeviceInfo.
    XDP_EXPORT std::map<std::string, bool>& getSoftwareEmulationMemUsage() ;
    XDP_EXPORT
    void addSoftwareEmulationMemUsage(const std::string& mem, bool used) ;

    // We provide guidance on the bit width of all of the compute unit
    //  ports to recommend if different bit widths might provide better
    //  performance.  For software emulation, since we do not have device
    //  information, we need to dig into the xclbin from OpenCL constructs.
    //  We store that information here since there is no DeviceInfo.
    XDP_EXPORT std::vector<std::string>& getSoftwareEmulationPortBitWidths() ;
    XDP_EXPORT void addSoftwareEmulationPortBitWidth(const std::string& s) ;

    // ************************************************
    // ***** Functions related to the run summary *****
    XDP_EXPORT
    std::vector<std::pair<std::string, std::string>>& getOpenedFiles() ;
    XDP_EXPORT
    void addOpenedFile(const std::string& name, const std::string& type) ;
    XDP_EXPORT std::string getSystemDiagram() ;

    // ***************************************************************
    // ***** Functions related to information on all the devices *****
    XDP_EXPORT uint64_t getNumDevices() ;
    XDP_EXPORT DeviceInfo* getDeviceInfo(uint64_t deviceId) ;
    XDP_EXPORT std::vector<std::string> getDeviceNames() ;
    XDP_EXPORT std::vector<DeviceInfo*> getDeviceInfos() ;
    // If any compute unit on any xclbin on any device has stall enabled,
    //  then we output a table of stall information.
    XDP_EXPORT bool hasStallInfo() ;
    XDP_EXPORT XclbinInfo* getCurrentlyLoadedXclbin(uint64_t deviceId) ;
    XDP_EXPORT void deleteCurrentlyUsedDeviceInterface(uint64_t deviceId) ;
    XDP_EXPORT bool isDeviceReady(uint64_t deviceId) ;
    XDP_EXPORT double getClockRateMHz(uint64_t deviceId, bool PL = true) ;
    XDP_EXPORT void setDeviceName(uint64_t deviceId, const std::string& name) ;
    XDP_EXPORT std::string getDeviceName(uint64_t deviceId) ;
    XDP_EXPORT void setDeviceIntf(uint64_t deviceId, DeviceIntf* devIntf) ;
    XDP_EXPORT DeviceIntf* getDeviceIntf(uint64_t deviceId) ;
    XDP_EXPORT void setKDMACount(uint64_t deviceId, uint64_t num) ;
    XDP_EXPORT uint64_t getKDMACount(uint64_t deviceId) ;
    XDP_EXPORT void setMaxReadBW(uint64_t deviceId, double bw) ;
    XDP_EXPORT double getMaxReadBW(uint64_t deviceId) ;
    XDP_EXPORT void setMaxWriteBW(uint64_t deviceId, double bw) ;
    XDP_EXPORT double getMaxWriteBW(uint64_t deviceId) ;
    XDP_EXPORT std::string getXclbinName(uint64_t deviceId) ;
    XDP_EXPORT std::vector<XclbinInfo*> getLoadedXclbins(uint64_t deviceId) ;
    XDP_EXPORT ComputeUnitInstance* getCU(uint64_t deviceId, int32_t cuId) ;
    XDP_EXPORT
    std::map<int32_t, ComputeUnitInstance*>* getCUs(uint64_t deviceId) ;
    XDP_EXPORT std::map<int32_t, Memory*>* getMemoryInfo(uint64_t deviceId) ;
    XDP_EXPORT Memory* getMemory(uint64_t deviceId, int32_t memId) ;
    // Reseting device information whenever a new xclbin is added
    XDP_EXPORT void updateDevice(uint64_t deviceId, void* devHandle) ;

    // *********************************************************
    // ***** Functions related to AIE specific information *****
    XDP_EXPORT bool isAIECounterRead(uint64_t deviceId) ;
    XDP_EXPORT void setIsAIECounterRead(uint64_t deviceId, bool val) ;
    XDP_EXPORT void setIsGMIORead(uint64_t deviceId, bool val) ;
    XDP_EXPORT bool isGMIORead(uint64_t deviceId) ;
    XDP_EXPORT uint64_t getNumAIECounter(uint64_t deviceId) ;
    XDP_EXPORT uint64_t getNumTraceGMIO(uint64_t deviceId) ;
    XDP_EXPORT AIECounter* getAIECounter(uint64_t deviceId, uint64_t idx) ;
    XDP_EXPORT
    std::map<uint32_t, uint32_t>*
    getAIECoreCounterResources(uint64_t deviceId) ;
    XDP_EXPORT
    std::map<uint32_t, uint32_t>*
    getAIEMemoryCounterResources(uint64_t deviceId) ;
    XDP_EXPORT
    std::map<uint32_t, uint32_t>*
    getAIEShimCounterResources(uint64_t deviceId) ;
    XDP_EXPORT
    std::map<uint32_t, uint32_t>* getAIECoreEventResources(uint64_t deviceId) ;
    XDP_EXPORT
    std::map<uint32_t, uint32_t>* getAIEMemoryEventResources(uint64_t deviceId);
    XDP_EXPORT
    std::map<uint32_t, uint32_t>* getAIEShimEventResources(uint64_t deviceId) ;
    XDP_EXPORT
    std::vector<std::unique_ptr<aie_cfg_tile>>*
    getAIECfgTiles(uint64_t deviceId) ;
    XDP_EXPORT TraceGMIO* getTraceGMIO(uint64_t deviceId, uint64_t idx) ;
    XDP_EXPORT void addTraceGMIO(uint64_t deviceId, uint32_t i, uint16_t col,
                                 uint16_t num, uint16_t stream, uint16_t len) ;
    XDP_EXPORT void addAIECounter(uint64_t deviceId, uint32_t i, uint16_t col,
                                  uint16_t r, uint8_t num, uint16_t start,
                                  uint16_t end, uint8_t reset, double freq,
                                  const std::string& mod,
				  const std::string& aieName) ;
    XDP_EXPORT void addAIECounterResources(uint64_t deviceId,
                                           uint32_t numCounters,
                                           uint32_t numTiles,
                                           bool isCore) ;
    XDP_EXPORT void addAIECoreEventResources(uint64_t deviceId,
                                             uint32_t numEvents,
                                             uint32_t numTiles) ;
    XDP_EXPORT void addAIEMemoryEventResources(uint64_t deviceId,
                                               uint32_t numEvents,
                                               uint32_t numTiles) ;
    XDP_EXPORT void addAIECfgTile(uint64_t deviceId,
                                  std::unique_ptr<aie_cfg_tile>& tile) ;
    XDP_EXPORT uint64_t getNumTracePLIO(uint64_t deviceId) ;
    XDP_EXPORT uint64_t getNumAIETraceStream(uint64_t deviceId) ;
    XDP_EXPORT void* getAieDevInst(std::function<void* (void*)> fetch,
                                   void* devHandle) ;
    XDP_EXPORT void* getAieDevice(std::function<void* (void*)> allocate,
                                  std::function<void (void*)> deallocate,
                                  void* devHandle) ;

    // ************************************************************************
    // ***** Functions for information from a specific xclbin on a device *****
    XDP_EXPORT uint64_t getNumAM(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT
    uint64_t getNumUserAMWithTrace(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT uint64_t getNumAIM(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT uint64_t getNumUserAIM(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT
    uint64_t getNumUserAIMWithTrace(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT uint64_t getNumASM(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT uint64_t getNumUserASM(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT
    uint64_t getNumUserASMWithTrace(uint64_t deviceId, XclbinInfo* xclbin) ;
    XDP_EXPORT uint64_t getNumNOC(uint64_t deviceId, XclbinInfo* xclbin) ;
    // Functions that get all of a certain type of monitor
    XDP_EXPORT
    std::vector<Monitor*>* getAIMonitors(uint64_t deviceId, XclbinInfo* xclbin);
    XDP_EXPORT
    std::vector<Monitor*>  getUserAIMsWithTrace(uint64_t deviceId,
                                                XclbinInfo* xclbin) ;
    XDP_EXPORT
    std::vector<Monitor*>* getASMonitors(uint64_t deviceId, XclbinInfo* xclbin);
    XDP_EXPORT
    std::vector<Monitor*>  getUserASMsWithTrace(uint64_t deviceId,
                                                XclbinInfo* xclbin) ;
    XDP_EXPORT bool hasFloatingAIMWithTrace(uint64_t deviceId,
                                            XclbinInfo* xclbin) ;
    XDP_EXPORT bool hasFloatingASMWithTrace(uint64_t deviceId,
                                            XclbinInfo* xclbin) ;

    // ********************************************************************
    // ***** Functions for single monitors from an xclbin on a device *****
    XDP_EXPORT
    Monitor* getAMonitor(uint64_t deviceId, XclbinInfo* xclbin,
                         uint64_t slotId) ;
    XDP_EXPORT
    Monitor* getAIMonitor(uint64_t deviceId, XclbinInfo* xclbin,
                          uint64_t slotId) ;
    XDP_EXPORT
    Monitor* getASMonitor(uint64_t deviceId, XclbinInfo* xclbin,
                          uint64_t slotID) ;
    XDP_EXPORT
    NoCNode* getNOC(uint64_t deviceId, XclbinInfo* xclbin, uint64_t idx) ;
    // This function takes a pre-allocated array of bools to fill with 
    //  the status of each compute unit's AM dataflow enabled status
    XDP_EXPORT
    void getDataflowConfiguration(uint64_t deviceId, bool* config, size_t size);
    // This fuction taks a pre-allocated array of bools to fill with
    //  information if each compute unit has a fast adapter or not.
    XDP_EXPORT
    void getFaConfiguration(uint64_t deviceId, bool* config, size_t size) ;
    XDP_EXPORT std::string getCtxInfo(uint64_t deviceId) ;
  } ;

}

#endif
