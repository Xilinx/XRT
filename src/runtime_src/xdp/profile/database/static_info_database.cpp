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
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"

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
    delete deviceIntf;

    for(auto& i : cus) {
      delete i.second;
    }
    cus.clear();
    for(auto& i : memoryInfo) {
      delete i.second;
    }
    memoryInfo.clear();

    for(auto& i : amMap) {
      delete i.second;
    }
    amMap.clear();
    for(auto& i : aimMap) {
      delete i.second;
    }
    aimMap.clear();
    for(auto& i : asmMap) {
      delete i.second;
    }
    asmMap.clear();

    for(auto i : nocList) {
      delete i;
    }
    nocList.clear();
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

  VPStaticDatabase::VPStaticDatabase(VPDatabase* d) : db(d), runSummary(nullptr)
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
    {
      runSummary->write(false) ;
      delete runSummary ;
    }
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

    DeviceInfo *devInfo = new DeviceInfo();
    devInfo->platformInfo.kdmaCount = 0;

    deviceInfo[deviceId] = devInfo;    // update this at the end ??

    const clock_freq_topology* clockSection = device->get_axlf_section<const clock_freq_topology*>(CLOCK_FREQ_TOPOLOGY);

    if(clockSection) {
      for(int32_t i = 0; i < clockSection->m_count; i++) {
        const struct clock_freq* clk = &(clockSection->m_clock_freq[i]);
        if(clk->m_type != CT_DATA) {
          continue;
        }
        devInfo->clockRateMHz = clk->m_freq_Mhz;
      }
    } else {
      devInfo->clockRateMHz = 300;
    }
    /* Configure AMs if context monitoring is supported
     * else disable alll AMs on this device
     */
    devInfo->ctxInfo = xrt_core::config::get_kernel_channel_info();

//    if (!setXclbinUUID(devInfo, device)) return;
    if (!setXclbinName(devInfo, device)) return;
    if (!initializeComputeUnits(devInfo, device)) return ;
    if (!initializeProfileMonitors(devInfo, device)) return ;
    devInfo->isReady = true;
  }

  bool VPStaticDatabase::resetDeviceInfo(uint64_t deviceId, const std::shared_ptr<xrt_core::device>& device)
  {
    std::lock_guard<std::mutex> lock(dbLock);

    auto itr = deviceInfo.find(deviceId);
    if(itr != deviceInfo.end()) {
      DeviceInfo *devInfo = itr->second;
      if(device->get_xclbin_uuid() == devInfo->loadedXclbinUUID) {
        // loading same xclbin multiple times ?
        return false;
      }
      delete itr->second;
      deviceInfo.erase(deviceId);
    }
    return true;
  }

#if 0
  bool VPStaticDatabase::setXclbinUUID(DeviceInfo* devInfo, const std::shared_ptr<xrt_core::device>& device)
  {
    devInfo->loadedXclbinUUID = device->get_xclbin_uuid();
    return true;
  }
#endif

  bool VPStaticDatabase::setXclbinName(DeviceInfo* devInfo, const std::shared_ptr<xrt_core::device>& device)
  {
    // Get SYSTEM_METADATA section
    std::pair<const char*, size_t> systemMetadata = device->get_axlf_section(SYSTEM_METADATA);
    const char* systemMetadataSection = systemMetadata.first;
    size_t      systemMetadataSz      = systemMetadata.second;
    if(systemMetadataSection == nullptr) return false;

    try {
      std::stringstream ss;
      ss.write(systemMetadataSection, systemMetadataSz);

      // Create a property tree and determine if the variables are all default values
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      devInfo->loadedXclbin = pt.get<std::string>("system_diagram_metadata.xclbin.generated_by.xclbin_name", "");
      if(!devInfo->loadedXclbin.empty()) {
        devInfo->loadedXclbin += ".xclbin";
      }
    } catch(...) {
      // keep default value in "devInfo->loadedXclbin" i.e. empty string
    }
    return true;
  }

  bool VPStaticDatabase::initializeComputeUnits(DeviceInfo* devInfo, const std::shared_ptr<xrt_core::device>& device)
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
      devInfo->cus[i] = cu;
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
      devInfo->memoryInfo[i] = new Memory(memData->m_type, i, memData->m_base_address, memData->m_size,
                                          reinterpret_cast<const char*>(memData->m_tag));
    }

    // Look into the connectivity section and load information about Compute Units and their Memory connections
    // Get CONNECTIVITY section
    const connectivity* connectivitySection = device->get_axlf_section<const connectivity*>(CONNECTIVITY);    
    if(connectivitySection == nullptr) return true;

    // Now make the connections
    cu = nullptr;
    for(int32_t i = 0; i < connectivitySection->m_count; i++) {
      const struct connection* connctn = &(connectivitySection->m_connection[i]);

      if(devInfo->cus.find(connctn->m_ip_layout_index) == devInfo->cus.end()) {
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
        devInfo->cus[connctn->m_ip_layout_index] = cu;
        if((ipData->properties >> IP_CONTROL_SHIFT) & AP_CTRL_CHAIN) {
          cu->setDataflowEnabled(true);
        } else
        if((ipData->properties >> IP_CONTROL_SHIFT) & FAST_ADAPTER) {
          cu->setFaEnabled(true);
        }
      } else {
        cu = devInfo->cus[connctn->m_ip_layout_index];
      }

      if(devInfo->memoryInfo.find(connctn->mem_data_index) == devInfo->memoryInfo.end()) {
        const struct mem_data* memData = &(memTopologySection->m_mem_data[connctn->mem_data_index]);
        devInfo->memoryInfo[connctn->mem_data_index]
                 = new Memory(memData->m_type, connctn->mem_data_index,
                              memData->m_base_address, memData->m_size, reinterpret_cast<const char*>(memData->m_tag));
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

      auto workGroupSz = kernel.second.get_child("compileWorkGroupSize");
      std::string x = workGroupSz.get<std::string>("<xmlattr>.x", "");
      std::string y = workGroupSz.get<std::string>("<xmlattr>.y", "");
      std::string z = workGroupSz.get<std::string>("<xmlattr>.z", "");

      // Find the ComputeUnitInstance
      for(auto cuItr : devInfo->cus) {
        if(0 != cuItr.second->getKernelName().compare(kernelName)) {
          continue;
        }
        cuItr.second->setDim(std::stoi(x), std::stoi(y), std::stoi(z));
      }
    }

    return true;
  }

  bool VPStaticDatabase::initializeProfileMonitors(DeviceInfo* devInfo, const std::shared_ptr<xrt_core::device>& device)
  {
    // Look into the debug_ip_layout section and load information about Profile Monitors
    // Get DEBUG_IP_LAYOUT section 
    const debug_ip_layout* debugIpLayoutSection = device->get_axlf_section<const debug_ip_layout*>(DEBUG_IP_LAYOUT);
    if(debugIpLayoutSection == nullptr) return false;

    for(uint16_t i = 0; i < debugIpLayoutSection->m_count; i++) {
      const struct debug_ip_data* debugIpData = &(debugIpLayoutSection->m_debug_ip_data[i]);
      uint64_t index = static_cast<uint64_t>(debugIpData->m_index_lowbyte) |
                       (static_cast<uint64_t>(debugIpData->m_index_highbyte) << 8);
      Monitor* mon = nullptr;

      std::string name(debugIpData->m_name);
      int32_t cuId  = -1;
      ComputeUnitInstance* cuObj = nullptr;
      // find CU
      if(debugIpData->m_type == ACCEL_MONITOR) {
        for(auto cu : devInfo->cus) {
          if(0 == name.compare(cu.second->getName())) {
            cuObj = cu.second;
            cuId = cu.second->getIndex();
            mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name, cuId);
            if((debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) && (index >= MIN_TRACE_ID_AM)) {
              uint64_t slotID = (index - MIN_TRACE_ID_AM) / 16;
              devInfo->amMap.emplace(slotID, mon);
              cuObj->setAccelMon(slotID);
            } else {
              devInfo->noTraceAMs.push_back(mon);
            }
            if(debugIpData->m_properties & XAM_STALL_PROPERTY_MASK) {
              cuObj->setStallEnabled(true);
            }
            break;
          }
        }
      } else if(debugIpData->m_type == AXI_MM_MONITOR) {
        // parse name to find CU Name and Memory
        size_t pos = name.find('/');
        std::string monCuName = name.substr(0, pos);

        pos = name.find('-');
        std::string memName = name.substr(pos+1);

        int32_t memId = -1;
        for(auto cu : devInfo->cus) {
          if(0 == monCuName.compare(cu.second->getName())) {
            cuId = cu.second->getIndex();
            cuObj = cu.second;
            break;
          }
        }
        for(auto mem : devInfo->memoryInfo) {
          if(0 == memName.compare(mem.second->name)) {
            memId = mem.second->index;
            break;
          }
        }
        mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name, cuId, memId);
        // If the AIM is an User Space AIM with trace enabled i.e. either connected to a CU or floating but not shell AIM
        if((debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) && (index >= MIN_TRACE_ID_AIM)) {
          uint64_t slotID = (index - MIN_TRACE_ID_AIM) / 2;
          devInfo->aimMap.emplace(slotID, mon);
          if(cuObj) {
            cuObj->addAIM(slotID);
          } else {
            // If not connected to CU and not a shell monitor, then a floating monitor
            devInfo->hasFloatingAIM = true;
          }
        } else {
          devInfo->noTraceAIMs.push_back(mon);
        }
      } else if(debugIpData->m_type == AXI_STREAM_MONITOR) {
        // associate with the first CU
        size_t pos = name.find('/');
        std::string monCuName = name.substr(0, pos);
        
        for(auto cu : devInfo->cus) {
          if(0 == monCuName.compare(cu.second->getName())) {
            cuId = cu.second->getIndex();
            cuObj = cu.second;
            break;
          }
        }
        if(-1 == cuId) {
          pos = name.find("-");
          if(std::string::npos != pos) {
            pos = name.find_first_not_of(" ", pos+1);
            monCuName = name.substr(pos);
            pos = monCuName.find('/');
            monCuName = monCuName.substr(0, pos);

            for(auto cu : devInfo->cus) {
              if(0 == monCuName.compare(cu.second->getName())) {
                cuId = cu.second->getIndex();
                cuObj = cu.second;
                break;
              }
            }
          }
        }

        mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name, cuId);
        if(debugIpData->m_properties & 0x2) {
          mon->isRead = true;
        }
        // If the ASM is an User Space ASM with trace enabled i.e. either connected to a CU or floating but not shell ASM
        if((debugIpData->m_properties & XMON_TRACE_PROPERTY_MASK) && (index >= MIN_TRACE_ID_ASM)) {
          uint64_t slotID = (index - MIN_TRACE_ID_ASM);
          devInfo->asmMap.emplace(slotID, mon);
          if(cuObj) {
            cuObj->addASM(slotID);
          } else {
            // If not connected to CU and not a shell monitor, then a floating monitor
            devInfo->hasFloatingASM = true;
          }
        } else {
          devInfo->noTraceASMs.push_back(mon);
        }
      } else if(debugIpData->m_type == AXI_NOC) {
        uint8_t readTrafficClass  = debugIpData->m_properties >> 2;
        uint8_t writeTrafficClass = debugIpData->m_properties & 0x3;

        mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name,
                          readTrafficClass, writeTrafficClass);
        devInfo->nocList.push_back(mon);
        // nocList in xdp::DeviceIntf is sorted; Is that required here?
      } else if(debugIpData->m_type == TRACE_S2MM && (debugIpData->m_properties & 0x1)) {
//        mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name);
        devInfo->numTracePLIO++;
      } else {
//        mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name);
      }
    }

    return true; 
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
      // Since turning on the OpenCL architecture will generate its own
      //  run summary, different than the one generated by the new
      //  architecture, we name this one with ".ex" added
      runSummary = new VPRunSummaryWriter("xclbin.ex.run_summary") ;
    }
    runSummary->write(false) ;
  }

}
