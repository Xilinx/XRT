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

#define XDP_SOURCE

#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "core/include/xclbin.h"

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

  VPStaticDatabase::VPStaticDatabase()
  {
#ifdef _WIN32
    pid = _getpid() ;
#else
    pid = static_cast<int>(getpid()) ;
#endif
  }

  VPStaticDatabase::~VPStaticDatabase()
  {
  }

  // This function is called whenever a device is loaded with an 
  //  xclbin.  It has to clear out any previous device information and
  //  reload our information.
  void VPStaticDatabase::updateDevice(uint64_t deviceId, const void* binary)
  {  
    resetDeviceInfo(deviceId);

    if (binary == nullptr) return ;

    DeviceInfo *devInfo = new DeviceInfo();
    devInfo->clockRateMHz = 300;
    devInfo->platformInfo.kdmaCount = 0;

    deviceInfo[deviceId] = devInfo;

    // Currently, we are going through the xclbin using the low level
    //  AXLF structure.  Would sysfs be a better solution?  Does
    //  that work with emulation?
    if (!setXclbinUUID(devInfo, binary)) return;
    if (!initializeComputeUnits(devInfo, binary)) return ;
    if (!initializeProfileMonitors(devInfo, binary)) return ;
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

  bool VPStaticDatabase::setXclbinUUID(DeviceInfo* devInfo, const void* binary)
  {
    const axlf* xbin = static_cast<const struct axlf*>(binary) ;
    (void)xbin;
    (void)devInfo;
    #if 0
    long double id = *((long double*)xbin->m_header.uuid);
    std::cout << " the uid " << id << std::endl;
    devInfo->loadedXclbin = std::to_string(*(long double*)(xbin->m_header.uuid));
    
    std::stringstream ss;
    ss << xbin->m_header.uuid;
    devInfo->loadedXclbin = ss.str();
    std::cout << "sizeof xuid_t " << sizeof(xuid_t) << std::endl;
    std::cout << "sizeof unsigned long long " << sizeof(unsigned long long) << std::endl;
    std::cout << "sizeof long double " << sizeof(long double) << std::endl;
    #endif
    return true;
  }

  bool VPStaticDatabase::initializeComputeUnits(DeviceInfo* devInfo, const void* binary)
  {
    // Look into the connectivity section and load information about Compute Units and their Memory connections
    const axlf* xbin     = static_cast<const struct axlf*>(binary);
    const char* chBinary = static_cast<const char*>(binary);

    // Get CONNECTIVITY section
    const axlf_section_header* connectivityHeader = xclbin::get_axlf_section(xbin, CONNECTIVITY);
    if(connectivityHeader == nullptr) return false;
    const connectivity* connectivitySection = reinterpret_cast<const connectivity*>(chBinary + connectivityHeader->m_sectionOffset) ;
    if(connectivitySection == nullptr) return false;

    // Get IP_LAYOUT section 
    const axlf_section_header* ipLayoutHeader = xclbin::get_axlf_section(xbin, IP_LAYOUT);
    if(ipLayoutHeader == nullptr) return false;
    const ip_layout* ipLayoutSection = reinterpret_cast<const ip_layout*>(chBinary + ipLayoutHeader->m_sectionOffset) ;
    if(ipLayoutSection == nullptr) return false;
    

    // Get MEM_TOPOLOGY section 
    const axlf_section_header* memTopologyHeader = xclbin::get_axlf_section(xbin, MEM_TOPOLOGY);
    if(memTopologyHeader == nullptr) return false;
    const mem_topology* memTopologySection = reinterpret_cast<const mem_topology*>(chBinary + memTopologyHeader->m_sectionOffset) ;
    if(memTopologySection == nullptr) return false;

    // Now make the connections
    ComputeUnitInstance* cu = nullptr;
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
      }

      if(devInfo->memoryInfo.find(connctn->mem_data_index) == devInfo->memoryInfo.end()) {
        const struct mem_data* memData = &(memTopologySection->m_mem_data[connctn->mem_data_index]);
        devInfo->memoryInfo[connctn->mem_data_index]
                 = new Memory(memData->m_type, connctn->mem_data_index,
                              memData->m_base_address, reinterpret_cast<const char*>(memData->m_tag));
      }
      cu->addConnection(connctn->arg_index, connctn->mem_data_index);
    }
    return true;
  }

  bool VPStaticDatabase::initializeProfileMonitors(DeviceInfo* devInfo, const void* binary)
  {
    // Look into the debug_ip_layout section and load information about Profile Monitors
    const axlf* xbin     = static_cast<const struct axlf*>(binary);
    const char* chBinary = static_cast<const char*>(binary);

    // Get CONNECTIVITY section
    const axlf_section_header* debugIpLayoutHeader = xclbin::get_axlf_section(xbin, DEBUG_IP_LAYOUT);
    if(debugIpLayoutHeader == nullptr) return false;
    const debug_ip_layout* debugIpLayoutSection = reinterpret_cast<const debug_ip_layout*>(chBinary + debugIpLayoutHeader->m_sectionOffset) ;
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
        devInfo->amList.push_back(mon);
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
        cuObj->addMonitor(mon);
        cuObj->setDataTransferEnabled(true);
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
        cuObj->addMonitor(mon);
        cuObj->setDataTransferEnabled(true);
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
  }

}
