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
#pragma warning (disable : 4996 4267)
/* 4267 : Disable warning for conversion of size_t to int32_t */
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

#define XDP_SOURCE

#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "core/include/xclbin.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"

#define XAM_STALL_PROPERTY_MASK        0x4



namespace xdp {

  ComputeUnitInstance::ComputeUnitInstance(int32_t i, const char* n)
    : index(i)
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
    resetDeviceInfo(deviceId);

    DeviceInfo *devInfo = new DeviceInfo();
    devInfo->platformInfo.kdmaCount = 0;

    deviceInfo[deviceId] = devInfo;

    std::shared_ptr<xrt_core::device> device = xrt_core::get_userpf_device(devHandle);

    if(nullptr == device) return;

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

//    if (!setXclbinUUID(devInfo, device)) return;
    if (!setXclbinName(devInfo, device)) return;
    if (!initializeComputeUnits(devInfo, device)) return ;
    if (!initializeProfileMonitors(devInfo, device)) return ;
  }

  void VPStaticDatabase::resetDeviceInfo(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(dbLock);

    auto itr = deviceInfo.find(deviceId);
    if(itr != deviceInfo.end()) {
      delete itr->second;
      deviceInfo.erase(deviceId);
    }
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
    if(ipLayoutSection == nullptr) return false;

    ComputeUnitInstance* cu = nullptr;
    for(int32_t i = 0; i < ipLayoutSection->m_count; i++) {
      const struct ip_data* ipData = &(ipLayoutSection->m_ip_data[i]);
      if(ipData->m_type != IP_KERNEL) {
        continue;
      }
      cu = new ComputeUnitInstance(i, reinterpret_cast<const char*>(ipData->m_name));
      devInfo->cus[i] = cu;
      if((ipData->properties >> IP_CONTROL_SHIFT) & AP_CTRL_CHAIN) {
        cu->setDataflowEnabled(true);
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
        }
        cu = new ComputeUnitInstance(connctn->m_ip_layout_index, reinterpret_cast<const char*>(ipData->m_name));
        devInfo->cus[connctn->m_ip_layout_index] = cu;
        if((ipData->properties >> IP_CONTROL_SHIFT) & AP_CTRL_CHAIN) {
          cu->setDataflowEnabled(true);
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
            cuObj->addMonitor(mon);
            if(debugIpData->m_properties & XAM_STALL_PROPERTY_MASK) {
              cuObj->setStallEnabled(true);
            }
            break;
          }
        }
        if(mon) { devInfo->amList.push_back(mon); }
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
        if(cuObj) {
          cuObj->addMonitor(mon);
          cuObj->setDataTransferEnabled(true);
        } else if(0 != monCuName.compare("shell")) {
          // If not connected to CU and not a shell monitor, then a floating monitor
          devInfo->hasFloatingAIM = true;
        }
        devInfo->aimList.push_back(mon);
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
        mon = new Monitor(debugIpData->m_type, index, debugIpData->m_name, cuId);
        if(debugIpData->m_properties & 0x2) {
          mon->isRead = true;
        }
        if(cuObj) {
          cuObj->addMonitor(mon);
          cuObj->setStreamEnabled(true);
        } else if(0 != monCuName.compare("shell")) {
          // If not connected to CU and not a shell monitor, then a floating monitor
          devInfo->hasFloatingASM = true;
        }
        devInfo->asmList.push_back(mon);
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
      runSummary = new VPRunSummaryWriter("xclbin.run_summary") ;
    }
    runSummary->write(false) ;
  }

}
