/**
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

#ifndef USED_REGISTERS_H
#define USED_REGISTERS_H

#include <vector>
#include <set>
#include <map>
#include <boost/property_tree/ptree.hpp>
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp {
/*************************************************************************************
The class UsedRegisters is what gives us AIE hw generation specific data. The base class
has virtual functions which populate the correct registers and their addresses according
to the AIE hw generation in the derived classes. Thus we can dynamically populate the
correct registers and their addresses at runtime.
**************************************************************************************/
class UsedRegisters {
  public:
    UsedRegisters() {
    //   populateRegNameToValueMap();
    //   populateRegValueToNameMap();
    //   populateRegAddrToSizeMap();
    }

    virtual ~UsedRegisters() {
      core_addresses.clear();
      memory_addresses.clear();
      interface_addresses.clear();
      memory_tile_addresses.clear();
      regNameToValue.clear();
      coreRegValueToName.clear();
      memoryRegValueToName.clear();
      memTileRegValueToName.clear();
      shimRegValueToName.clear();
      //regAddrToSize.clear();
      coreRegAddrToSize.clear();
      memoryRegAddrToSize.clear();
      shimRegAddrToSize.clear();
      memTileRegAddrToSize.clear();

    }

    std::set<uint64_t> getCoreAddresses() {
      return core_addresses;
    }
    std::set<uint64_t> getMemoryAddresses() {
      return memory_addresses;
    }
    std::set<uint64_t> getInterfaceAddresses() {
      return interface_addresses;
    }
    std::set<uint64_t> getMemoryTileAddresses() {
      return memory_tile_addresses;
    }

    std::string getRegisterName(uint64_t regVal,module_type mod) {
      std::map<uint64_t,std::string>::iterator itr;
      if (mod == module_type::core) {
        itr = coreRegValueToName.find(regVal);
        if (itr != coreRegValueToName.end())
          return itr->second;
      }
      else if (mod == module_type::dma) {
        itr = memoryRegValueToName.find(regVal);
        if (itr != memoryRegValueToName.end())
          return itr->second;
      }
      else if (mod == module_type::shim) {
        itr = shimRegValueToName.find(regVal);
        if (itr != shimRegValueToName.end())
          return itr->second;
      }
      else if (mod == module_type::mem_tile) {
        itr = memTileRegValueToName.find(regVal);
        if (itr != memTileRegValueToName.end())
          return itr->second;
      }
      std::stringstream ss;
      ss << "0x" << std::hex << std::uppercase << regVal;
      return ss.str();
      }



    uint64_t getRegisterAddr(const std::string& regName) {
      auto itr=regNameToValue.find(regName);
      return (itr != regNameToValue.end()) ? itr->second : 0;
    }

    uint32_t getRegAddrToSize(uint64_t regVal,module_type mod) {
      std::map<uint64_t, uint32_t>::iterator itr;
      switch (mod){
        case module_type::core : {
          itr = coreRegAddrToSize.find(regVal);
           if (itr != coreRegAddrToSize.end())
            return itr->second;
        }
        break;
        case module_type::dma : {
          itr = memoryRegAddrToSize.find(regVal);
           if (itr != memoryRegAddrToSize.end())
            return itr->second;
          }
        break;
        case module_type::shim : {
          itr = shimRegAddrToSize.find(regVal);
           if (itr != shimRegAddrToSize.end())
            return itr->second;
        }
        break;
        case module_type::mem_tile : {
          itr = memTileRegAddrToSize.find(regVal);
           if (itr != memTileRegAddrToSize.end())
            return itr->second;
        }
        break;
        default: break;
      }
      return 32;
    }

    virtual void populateProfileRegisters() = 0;
    virtual void populateTraceRegisters() = 0;
    virtual void populateRegNameToValueMap() = 0;
    virtual void populateRegValueToNameMap() = 0;
    virtual void populateRegAddrToSizeMap() = 0;

    void populateAllRegisters() {
      populateProfileRegisters();
      populateTraceRegisters();
    }

  protected:
  //public:
    std::set<uint64_t> core_addresses;
    std::set<uint64_t> memory_addresses;
    std::set<uint64_t> interface_addresses;
    std::set<uint64_t> memory_tile_addresses;
    std::map<std::string, uint64_t> regNameToValue;
    std::map<uint64_t, std::string> coreRegValueToName;
    std::map<uint64_t, std::string> memoryRegValueToName;
    std::map<uint64_t, std::string> shimRegValueToName;
    std::map<uint64_t, std::string> memTileRegValueToName;
    std::map<uint64_t, std::string> ucRegValueToName;
    std::map<uint64_t, std::string> npiRegValueToName;
    //std::map<uint64_t, uint32_t> regAddrToSize;
    std::map<uint64_t, uint32_t> coreRegAddrToSize;
    std::map<uint64_t, uint32_t> memoryRegAddrToSize;
    std::map<uint64_t, uint32_t> shimRegAddrToSize;
    std::map<uint64_t, uint32_t> memTileRegAddrToSize;
    std::map<uint64_t, uint32_t> ucRegAddrToSize ;
    std::map<uint64_t, uint32_t> npiRegAddrToSize;
};

/*************************************************************************************
 AIE1 Registers
 *************************************************************************************/
class AIE1UsedRegisters : public UsedRegisters {
public:
  AIE1UsedRegisters() {
    populateRegNameToValueMap();
    populateRegValueToNameMap();
    populateRegAddrToSizeMap();
  }
  virtual ~AIE1UsedRegisters() {}

  void populateProfileRegisters();

  void populateTraceRegisters() ;

  void populateRegNameToValueMap();

  void populateRegValueToNameMap() ;

  void populateRegAddrToSizeMap() ;

};

/*************************************************************************************
 AIE2 Registers
 *************************************************************************************/
class AIE2UsedRegisters : public UsedRegisters {
public:
  AIE2UsedRegisters() {
    populateRegNameToValueMap();
    populateRegValueToNameMap();
    populateRegAddrToSizeMap();
  }
 // ~AIE2UsedRegisters() = default;
  virtual ~AIE2UsedRegisters() {}

  void populateProfileRegisters() ;

  void populateTraceRegisters() ;

void populateRegNameToValueMap() ;

void populateRegValueToNameMap() ;

void populateRegAddrToSizeMap() ;


};

/*************************************************************************************
 AIE2ps Registers
 *************************************************************************************/
class AIE2psUsedRegisters : public UsedRegisters {
public:
  AIE2psUsedRegisters() {
    populateRegNameToValueMap();
    populateRegValueToNameMap();
    populateRegAddrToSizeMap();
  }
  //~AIE2psUsedRegisters() = default;
  virtual ~AIE2psUsedRegisters() {}

  void populateProfileRegisters();

  void populateTraceRegisters();

  void populateRegNameToValueMap() ;

  void populateRegValueToNameMap();

  void populateRegAddrToSizeMap() ;

};
} // end XDP namespace

#endif