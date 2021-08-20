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

#define XDP_SOURCE

#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include "core/include/xclbin.h"
#include "core/include/xclperf.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"

#define XAM_STALL_PROPERTY_MASK        0x4
#define XMON_TRACE_PROPERTY_MASK       0x1

namespace xdp {

  ComputeUnitInstance::ComputeUnitInstance(int32_t i, const std::string &n)
    : index(i),
      amId(-1)
  {
    std::string fullName(n);
    size_t pos = fullName.find(':');
    kernelName = fullName.substr(0, pos);
    name = fullName.substr(pos+1);
    
    dim[0] = 0 ;
    dim[1] = 0 ;
    dim[2] = 0 ;
  }

  ComputeUnitInstance::~ComputeUnitInstance()
  {
  }

  std::string ComputeUnitInstance::getDim()
  {
    std::string combined ;

    combined = std::to_string(dim[0]) ;
    combined += ":" ;
    combined += std::to_string(dim[1]) ;
    combined += ":" ;
    combined += std::to_string(dim[2]) ;
    return combined ;
  }

  void ComputeUnitInstance::addConnection(int32_t argIdx, int32_t memIdx)
  {
    if(connections.find(argIdx) == connections.end()) {
      std::vector<int32_t> mems(1, memIdx);
      connections[argIdx] = mems;
      return;
    }
    connections[argIdx].push_back(memIdx);
  }

  DeviceInfo::~DeviceInfo()
  {
    for (auto& i : loadedXclbins) {
      delete i ;
    }
    loadedXclbins.clear() ;

    for(auto i : aieList) {
      delete i;
    }
    aieList.clear();
    for(auto i : gmioList) {
      delete i;
    }
    gmioList.clear();
  }

  void DeviceInfo::addTraceGMIO(uint32_t id, uint16_t col, uint16_t num,
                                uint16_t stream, uint16_t len)
  {
    TraceGMIO* traceGmio = new TraceGMIO(id, col, num, stream, len);
    gmioList.push_back(traceGmio);
  }

  void DeviceInfo::addAIECounter(uint32_t i, uint16_t col, uint16_t r,
                                 uint8_t num, uint8_t start, uint8_t end,
                                 uint8_t reset, double freq,
                                 const std::string& mod,
                                 const std::string& aieName)
  {
      AIECounter* aie = new AIECounter(i, col, r, num, start, end,
                                       reset, freq, mod, aieName);
      aieList.push_back(aie);
  }

  VPStaticDatabase::VPStaticDatabase(VPDatabase* d) :
    db(d), runSummary(nullptr), systemDiagram(""),
    softwareEmulationDeviceName(""), aieDevInst(nullptr),
    aieDevice(nullptr), deallocateAieDevice(nullptr)
  {
#ifdef _WIN32
    pid = _getpid() ;
#else
    pid = static_cast<int>(getpid()) ;
#endif
    applicationStartTime = 0 ;
  }

  VPStaticDatabase::~VPStaticDatabase()
  {
    if (runSummary != nullptr)
    {
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

  std::vector<std::string> VPStaticDatabase::getDeviceNames()
  {
    std::vector<std::string> deviceNames ;
    for (auto device : deviceInfo)
    {
      deviceNames.push_back((device.second)->getUniqueDeviceName()) ;
    }

    return deviceNames ;
  }

  std::vector<DeviceInfo*> VPStaticDatabase::getDeviceInfos()
  {
    std::vector<DeviceInfo*> infos ;
    for (auto device : deviceInfo)
    {
      infos.push_back(device.second) ;
    }
    return infos ;
  }

  bool VPStaticDatabase::hasStallInfo()
  {
    for (auto device : deviceInfo)
    {
      if ((device.second)->loadedXclbins.size() <= 0) continue ;
      for (auto cu : (device.second)->loadedXclbins.back()->cus)
      {
        if ((cu.second)->stallEnabled()) return true ;
      }
    }
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
    if ((deviceInfo.find(deviceId) != deviceInfo.end()) &&
        deviceInfo[deviceId]->loadedXclbins.size() >= 1) {
      (db->getDynamicInfo()).markXclbinEnd(deviceId) ;
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
    currentXclbin->clockRateMHz = findClockRate(device) ;


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
    std::lock_guard<std::mutex> lock(dbLock);

    auto itr = deviceInfo.find(deviceId);
    if(itr != deviceInfo.end()) {
      DeviceInfo *devInfo = itr->second;
      // Are we attempting to load the same xclbin multiple times?
      if (devInfo->loadedXclbins.size() > 0 &&
          device->get_xclbin_uuid() == devInfo->loadedXclbins.back()->uuid) {
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
    systemDiagram = buf.str() ;

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
      currentXclbin->cus[i] = cu ;
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
      currentXclbin->memoryInfo[i] = new Memory(memData->m_type, i, memData->m_base_address, memData->m_size,
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

      if(currentXclbin->cus.find(connctn->m_ip_layout_index) == currentXclbin->cus.end()) {
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
        currentXclbin->cus[connctn->m_ip_layout_index] = cu;
        if((ipData->properties >> IP_CONTROL_SHIFT) & AP_CTRL_CHAIN) {
          cu->setDataflowEnabled(true);
        } else
        if((ipData->properties >> IP_CONTROL_SHIFT) & FAST_ADAPTER) {
          cu->setFaEnabled(true);
        }
      } else {
        cu = currentXclbin->cus[connctn->m_ip_layout_index];
      }

      if(currentXclbin->memoryInfo.find(connctn->mem_data_index) == currentXclbin->memoryInfo.end()) {
        const struct mem_data* memData = &(memTopologySection->m_mem_data[connctn->mem_data_index]);
        currentXclbin->memoryInfo[connctn->mem_data_index]
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
      for(auto cuItr : currentXclbin->cus) {
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
    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    // MIN_TRACE_ID_AM is 0, so the index is always >=
    uint64_t slotId = (index - MIN_TRACE_ID_AM) / 16;

    for (auto cu : devInfo->loadedXclbins.back()->cus) {
      ComputeUnitInstance* cuObj = cu.second ;
      int32_t cuId = cu.second->getIndex() ;

      if (0 == name.compare(cu.second->getName())) {
        // Set properties on this specific CU
        if(debugIpData->m_properties & XAM_STALL_PROPERTY_MASK) {
          cuObj->setStallEnabled(true);
        }

        Monitor* mon = new Monitor(debugIpData->m_type, index,
                                   debugIpData->m_name, cuId, -1 /* memId */,
                                   slotId) ;
        // First, add this monitor to the list of all AMs
        devInfo->loadedXclbins.back()->amList.push_back(mon);
        // Second, determine if this is a trace-enabled AM or just a counter AM
        //  and add it to the appropriate container
        if (debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) {
          devInfo->loadedXclbins.back()->amMap.emplace(slotId, mon);
          cuObj->setAccelMon(slotId);
        }
        else {
          devInfo->loadedXclbins.back()->noTraceAMs.push_back(mon);
        }
        break ;
      }
    }
  }

  void VPStaticDatabase::initializeAIM(DeviceInfo* devInfo,
                                       const std::string& name,
                                       const struct debug_ip_data* debugIpData)
  {
    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    if (index < MIN_TRACE_ID_AIM) {
      std::stringstream msg;
      msg << "AIM with incorrect index: " << index ;
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              msg.str());
      index = MIN_TRACE_ID_AIM ;
    }
    uint64_t slotId = (index - MIN_TRACE_ID_AIM) / 2;

    // Parse name to find CU Name and Memory
    size_t pos = name.find('/');
    std::string monCuName = name.substr(0, pos);

    if (monCuName == "memory_subsystem") {
      devInfo->loadedXclbins.back()->hasMemoryAIM = true ;
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

    for(auto cu : devInfo->loadedXclbins.back()->cus) {
      if(0 == monCuName.compare(cu.second->getName())) {
        cuId = cu.second->getIndex();
        cuObj = cu.second;
        break;
      }
    }
    for(auto mem : devInfo->loadedXclbins.back()->memoryInfo) {
      if(0 == memName.compare(mem.second->name)) {
        memId = mem.second->index;
        break;
      }
    }
    Monitor* mon = new Monitor(debugIpData->m_type, index,
                               debugIpData->m_name, cuId, memId, slotId);
    mon->port = portName;

    // Add the monitor to the list of all AIMs
    devInfo->loadedXclbins.back()->aimList.push_back(mon) ;
    bool traceEnabled =
      (debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) != 0 ;

    // Attach to a CU if appropriate
    if (cuObj) {
      cuObj->addAIM(slotId, traceEnabled) ;
    }
    else if(traceEnabled) {
      // If not connected to CU and not a shell monitor, then a floating monitor
      // This floating monitor is enabled for trace too
      devInfo->loadedXclbins.back()->hasFloatingAIMwithTrace = true;
    }

    // If the AIM is an User Space AIM with trace enabled i.e. either
    //  connected to a CU or floating but not shell AIM
    if(traceEnabled) {
      devInfo->loadedXclbins.back()->aimMap.emplace(slotId, mon);
    } else {
      devInfo->loadedXclbins.back()->noTraceAIMs.push_back(mon);
    }
  }

  void VPStaticDatabase::initializeASM(DeviceInfo* devInfo,
                                       const std::string& name,
                                       const struct debug_ip_data* debugIpData)
  {
    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    if (index < MIN_TRACE_ID_ASM) {
      std::stringstream msg;
      msg << "ASM with incorrect index: " << index ;
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              msg.str());
      index = MIN_TRACE_ID_ASM ;
    }
    uint64_t slotId = (index - MIN_TRACE_ID_ASM);

    // associate with the first CU
    size_t pos = name.find('/');
    std::string monCuName = name.substr(0, pos);

    std::string portName = "";
    ComputeUnitInstance* cuObj = nullptr ;
    int32_t cuId = -1 ;

    for(auto cu : devInfo->loadedXclbins.back()->cus) {
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

        for(auto cu : devInfo->loadedXclbins.back()->cus) {
          if(0 == monCuName.compare(cu.second->getName())) {
            cuId = cu.second->getIndex();
            cuObj = cu.second;
            break;
          }
        }
      }
    }

    Monitor* mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name,
                               cuId, -1 /* memId */, slotId);
    mon->port = portName;
    if(debugIpData->m_properties & 0x2) {
      mon->isRead = true;
    }

    // Add this monitor to the list of all monitors
    devInfo->loadedXclbins.back()->asmList.push_back(mon) ;
    bool traceEnabled =
      (debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) != 0 ;

    // If the ASM is an User Space ASM i.e. either connected to a CU or floating but not shell ASM
    if (cuObj) {
      cuObj->addASM(slotId, traceEnabled) ;
    }
    else if (traceEnabled) {
      // If not connected to CU and not a shell monitor, then a floating monitor
      // This floating monitor is enabled for trace too
      devInfo->loadedXclbins.back()->hasFloatingASMwithTrace = true ;
    }

    if (traceEnabled) {
      devInfo->loadedXclbins.back()->asmMap.emplace(slotId, mon) ;
    }
    else {
      devInfo->loadedXclbins.back()->noTraceASMs.push_back(mon);
    }
  }

  void VPStaticDatabase::initializeNOC(DeviceInfo* devInfo,
                                       const struct debug_ip_data* debugIpData)
  {
    uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
      (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
    uint8_t readTrafficClass  = debugIpData->m_properties >> 2;
    uint8_t writeTrafficClass = debugIpData->m_properties & 0x3;

    Monitor* mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name,
                               readTrafficClass, writeTrafficClass);
    devInfo->loadedXclbins.back()->nocList.push_back(mon);
    // nocList in xdp::DeviceIntf is sorted; Is that required here?
  }

  void VPStaticDatabase::initializeTS2MM(DeviceInfo* devInfo,
                                         const struct debug_ip_data* debugIpData)
  {
    // TS2MM IP for either AIE PLIO or PL trace offload
    if (debugIpData->m_properties & 0x1)
      devInfo->loadedXclbins.back()->numTracePLIO++;
    else
      devInfo->loadedXclbins.back()->usesTs2mm = true;
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
    if (!aieDevInst) return nullptr ;

    deallocateAieDevice = deallocate ;
    aieDevice = allocate(devHandle) ;
    return aieDevice ;
  }

  void VPStaticDatabase::addCommandQueueAddress(uint64_t a)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    commandQueueAddresses.emplace(a) ;
  }

  void VPStaticDatabase::addOpenedFile(const std::string& name,
                                       const std::string& type)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    openedFiles.push_back(std::make_pair(name, type)) ;

    if (runSummary == nullptr)
    {
      runSummary = new VPRunSummaryWriter("xrt.run_summary", db) ;
    }
    runSummary->write(false) ;
  }

  void VPStaticDatabase::addEnqueuedKernel(const std::string& identifier)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    enqueuedKernels.emplace(identifier) ;
  }

}
