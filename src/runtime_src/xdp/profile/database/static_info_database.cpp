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

#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#define XDP_SOURCE

#include "xdp/profile/database/static_info_database.h"
#include "core/include/xclbin.h"

namespace xdp {

  ComputeUnitInstance::ComputeUnitInstance(const char* n, int i) :
    name(n), index(i)
  {
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

  void VPStaticDatabase::resetDeviceInfo(void* dev)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    // All the device specific information has to be reset
    kdmaCount[dev] = 0 ;
    deviceNames[dev] = "" ;
    loadedXclbins[dev] = "" ;
    cus[dev].clear() ;
    ddrBanks[dev].clear() ;
    hbmBanks[dev].clear() ;
    plramBanks[dev].clear() ;
  }

  bool VPStaticDatabase::initializeMemory(void* dev, const void* binary)
  {
    const axlf* xbin = static_cast<const struct axlf*>(binary) ;
    const axlf_section_header* memTopologyHeader =
      xclbin::get_axlf_section(xbin, MEM_TOPOLOGY) ;
    if (memTopologyHeader == nullptr) return false ;
    const mem_topology* memTopologySection =
      reinterpret_cast<const mem_topology*>(static_cast<const char*>(binary) + memTopologyHeader->m_sectionOffset) ;
    //if (memTopologySection == nullptr) return false ;

    std::lock_guard<std::mutex> lock(dbLock) ;

    for (int i = 0 ; i < memTopologySection->m_count ; ++i)
    {
      const struct mem_data* data = &(memTopologySection->m_mem_data[i]) ;
      std::pair<uint64_t, std::string> nextPair ;
      nextPair.first = data->m_base_address ;
      nextPair.second = reinterpret_cast<const char*>(data->m_tag) ;
      switch (data->m_type)
      {
      case MEM_DDR3:
      case MEM_DDR4:
      case MEM_DRAM:
	// Currently, everything is in this bucket.  Should this be a CR?
	ddrBanks[dev].push_back(nextPair) ;
	break ;
      case MEM_HBM:
	hbmBanks[dev].push_back(nextPair) ;
	break ;
      case MEM_BRAM:
      case MEM_URAM:
	plramBanks[dev].push_back(nextPair) ;
	break ;
      default:
	break ;
      }
    }
    return true ;
  }

  bool VPStaticDatabase::initializeComputeUnits(void* dev, const void* binary)
  {
    const axlf* xbin = static_cast<const struct axlf*>(binary) ;
    const axlf_section_header* ipLayoutHeader =
      xclbin::get_axlf_section(xbin, IP_LAYOUT) ;
    if (ipLayoutHeader == nullptr) return false ;

    const ip_layout* ipLayoutSection =
      reinterpret_cast<const ip_layout*>(static_cast<const char*>(binary) + ipLayoutHeader->m_sectionOffset) ;
    //if (ipLayoutSection == nullptr) return false ;

    for (int32_t i = 0 ; i < ipLayoutSection->m_count ; ++i)
    {
      const struct ip_data* nextIP = &(ipLayoutSection->m_ip_data[i]) ;
      if (nextIP->m_type == IP_KERNEL)
      {
	ComputeUnitInstance nextCU(reinterpret_cast<const char*>(nextIP->m_name), i);
	cus[dev].push_back(nextCU) ;
      }
    }
    return true ;
  }

  bool VPStaticDatabase::initializeConnections(void* /*dev*/,
                                               const void* /*binary*/)
  {
    // TODO
    /*
    const axlf* xbin = static_cast<const struct axlf*>(binary) ;
    const axlf_section_header* connectivityHeader =
      xclbin::get_axlf_section(xbin, CONNECTIVITY) ;
    if (connectivityHeader == nullptr) return false ;
    const connectivity* connectivitySection =
      reinterpret_cast<const connectivity*>(static_cast<const char*>(binary) + connectivityHeader->m_sectionOffset) ;
    //if (connectivitySection == nullptr) return false ;
    // TBD
    */
    return true ;
  }

  // This function is called whenever a device is loaded with an
  //  xclbin.  It has to clear out any previous device information and
  //  reload our information.
  void VPStaticDatabase::updateDevice(void* dev, const void* binary)
  {
    resetDeviceInfo(dev) ;

    if (binary == nullptr) return ;

    // Currently, we are going through the xclbin using the low level
    //  AXLF structure.  Would sysfs be a better solution?  Does
    //  that work with emulation?
    if (!initializeMemory(dev, binary)) return ;
    if (!initializeComputeUnits(dev, binary)) return ;
    if (!initializeConnections(dev, binary)) return ;
  }

  void VPStaticDatabase::addCommandQueueAddress(uint64_t a)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    commandQueueAddresses.emplace(a) ;
  }

  void VPStaticDatabase::addKDMACount(void* dev, uint16_t numKDMAs)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    kdmaCount[dev] = numKDMAs ;
  }

  void VPStaticDatabase::addOpenedFile(const std::string& name,
				       const std::string& type)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    openedFiles.push_back(std::make_pair(name, type)) ;
  }

}
