// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#define XDP_PLUGIN_SOURCE

#include "xdp/profile/plugin/aie_debug/edge/aie_debug.h"
#include "xdp/profile/plugin/aie_debug/aie_debug_metadata.h"
#include "xdp/profile/plugin/aie_debug/generations/aie1_attributes.h"
#include "xdp/profile/plugin/aie_debug/generations/aie1_registers.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2_attributes.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2_registers.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2ps_attributes.h"
#include "xdp/profile/plugin/aie_debug/generations/aie2ps_registers.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>

#include "core/common/message.h"
#include "core/common/time.h"
#include "core/edge/user/shim.h"
#include "core/include/xrt/xrt_kernel.h"
#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"

#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/vp_base/info.h"

namespace {
  static void* fetchAieDevInst(void* devHandle)
  {
    auto drv = ZYNQ::shim::handleCheck(devHandle);
    if (!drv)
      return nullptr ;
    auto aieArray = drv->getAieArray();
    if (!aieArray)
      return nullptr;
    return aieArray->get_dev();
  }

  static void* allocateAieDevice(void* devHandle)
  {
    auto aieDevInst = static_cast<XAie_DevInst*>(fetchAieDevInst(devHandle));
    if (!aieDevInst)
      return nullptr;
    return new xaiefal::XAieDev(aieDevInst, false);
  }

  static void deallocateAieDevice(void* aieDevice)
  {
    auto object = static_cast<xaiefal::XAieDev*>(aieDevice);
    if (object != nullptr)
      delete object;
  }
} // end anonymous namespace

namespace xdp {
  using severity_level = xrt_core::message::severity_level;
  using tile_type = xdp::tile_type;
  using module_type = xdp::module_type;

  /****************************************************************************
   * Edge constructor
   ***************************************************************************/
  AieDebug_EdgeImpl::AieDebug_EdgeImpl(VPDatabase* database, std::shared_ptr<AieDebugMetadata> metadata)
    : AieDebugImpl(database, metadata)
  {
    auto hwGen = metadata->getHardwareGen();
    //UsedRegisters* usedRegisters;
    if (hwGen==1){
        usedRegisters=new AIE1UsedRegisters();
    }
    else if (hwGen==3){
        usedRegisters=new AIE2UsedRegisters();
    }
    else if (hwGen==4 || hwGen==8 || hwGen==9){
        usedRegisters=new AIE2pUsedRegisters();
    }
    else if (hwGen==5){
        usedRegisters=new AIE2psUsedRegisters();
    }
    else if (hwGen>= 40){
        usedRegisters=new AIE4UsedRegisters();
    }
    usedRegisters->populateRegNameToValueMap();
  }

  /****************************************************************************
   * Edge destructor
   ***************************************************************************/
  AieDebug_EdgeImpl::~AieDebug_EdgeImpl(){
    xrt_core::message::send(severity_level::info, "XRT", "!! Calling AIE DebugAieDebug_EdgeImpl Destructor");
    delete usedRegisters;
  }

  /****************************************************************************
   * Poll all registers
   ***************************************************************************/
  void AieDebug_EdgeImpl::poll(const uint32_t deviceID, void* handle)
  {
    xrt_core::message::send(severity_level::debug, "XRT", "!! Inside AIE Debug AieDebug_EdgeImpl::poll");
    std::stringstream msg;
    msg << "!!!! AieDebug_EdgeImpl::poll deviceID is= "<<deviceID;
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
    // Wait until xclbin has been loaded and device has been updated in database

    if (!(db->getStaticInfo().isDeviceReady(deviceID))){
      xrt_core::message::send(severity_level::debug, "XRT", "!!!!!!  xclbin, device isn't ready and loaded");
      return;
    }
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));
    if (!aieDevInst) {
      xrt_core::message::send(severity_level::debug, "XRT", "!!!!!!  aieDevInst isn't correctly populated");
      return;
    }

    xrt_core::message::send(severity_level::debug, "XRT", "!!!! Calling AIE Poll.");

    /* Old code
    for (auto& tileAddr : debugAddresses) {
      auto tile    = tileAddr.first;
      auto addrVec = tileAddr.second;

      for (auto& addr : addrVec) {
        uint32_t value = 0;
        XAie_Read32(aieDevInst, addr, &value);

        std::stringstream msg;
        msg << "Debug tile (" << tile.col << ", " << tile.row << ") "
            << "hex address/values: 0x" << std::hex << addr << " : 0x"
            << value << std::dec;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
      }
    } */

   for (auto& tileAddr : debugTileMap){
      xrt_core::message::send(severity_level::debug, "XRT", "!!!!!! Reading values for all tiles ");
      tileAddr.second->readValues(aieDevInst);
      xrt_core::message::send(severity_level::debug, "XRT", "!!!!!! PRINTING values for all tiles ");
      tileAddr.second->printValues();
   }
  }

  /****************************************************************************
   * Update device
   ***************************************************************************/
  void AieDebug_EdgeImpl::updateDevice()
  {
    // Do nothing for now
  }

  /****************************************************************************
   * Convert each xrt.ini entry to actual list of registers
   ***************************************************************************/
  std::vector<uint64_t>
  AieDebug_EdgeImpl::stringToRegList(std::string stringEntry, module_type t)
  { //core=trace_config,0x3400
    xrt_core::message::send(severity_level::debug, "XRT", "!! Calling AIE Debug AieDebug_EdgeImpl::stringToRegList");
    std::vector<uint64_t> listofRegisters;
    if (stringEntry.rfind("0x", 0) == 0) {
      // if it starts with "0x" that means a particular register address is specified
      uint64_t val = stoul(stringEntry,nullptr,16);
      listofRegisters.push_back(val);
      return listofRegisters;
    }
    else if(stringEntry=="trace_config")
      {
        usedRegisters->populateTraceRegisters();
      }
    else if (stringEntry=="profile_config")
      {
        usedRegisters->populateProfileRegisters();
      }
    else if (stringEntry=="all")
      {
        usedRegisters->populateAllRegisters();
      }
    else {
      //first dealing with specific register names
      if(usedRegisters->regNametovalues.find(stringEntry) != usedRegisters->regNametovalues.end()) {
        uint64_t tmpRedAddr = usedRegisters->regNametovalues[stringEntry];
        listofRegisters.push_back(tmpRedAddr);
      }
      else {
        std::stringstream msg;
        msg << "Error Parsing Debug plugin Metric String. Please enter exact register address, register name, or either of trace_config/profile_config/all. ";
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      }
      return listofRegisters;
    }

    if (t==module_type::core){
      listofRegisters.insert( listofRegisters.end(), usedRegisters->core_addresses.begin(),
                             usedRegisters->core_addresses.end() );
    }
    else if (t==module_type::dma){
      listofRegisters.insert( listofRegisters.end(), usedRegisters->memory_addresses.begin(),
                             usedRegisters->memory_addresses.end() );
    }
    else if (t==module_type::shim){
      listofRegisters.insert( listofRegisters.end(), usedRegisters->interface_addresses.begin(),
                             usedRegisters->interface_addresses.end() );
    }
    else if (t==module_type::mem_tile){
      listofRegisters.insert( listofRegisters.end(), usedRegisters->memory_tile_addresses.begin(),
                             usedRegisters->memory_tile_addresses.end() );
    }
    return listofRegisters;
  }

  /****************************************************************************
   * Convert xrt.ini setting to vector
   ***************************************************************************/
  std::vector<std::string>
  AieDebug_EdgeImpl::getSettingsVector(std::string settingsString)
  {
    xrt_core::message::send(severity_level::debug, "XRT", "!! Calling AIE Debug AieDebug_EdgeImpl::getSettingsVector");
    if (settingsString.empty())
      return {};

    // Each of the metrics can have ; separated multiple values. Process and save all
    std::vector<std::string> settingsVector;
    boost::replace_all(settingsString, " ", "");
    boost::split(settingsVector, settingsString, boost::is_any_of(","));
    return settingsVector;
  }

  /****************************************************************************
   * Parse AIE metrics
   ***************************************************************************/
  std::map<module_type, std::vector<uint64_t>>
  AieDebug_EdgeImpl::parseMetrics()
  {
    xrt_core::message::send(severity_level::debug, "XRT", "!! Calling AIE Debug AieDebug_EdgeImpl::parseMetrics");
    //TODO: change regValues to set to prevent duplication
    std::map<module_type, std::vector<uint64_t>> regValues {
      {module_type::core, {}},
      {module_type::dma, {}},
      {module_type::shim, {}},
      {module_type::mem_tile, {}}
    };
    std::vector<std::string> metricsConfig;

    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_core_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_memory_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_interface_registers());
    metricsConfig.push_back(xrt_core::config::get_aie_debug_settings_memory_tile_registers());

    unsigned int module = 0;
    for (auto const& kv : moduleTypes) {
      auto type = kv.first;
      std::vector<std::string> metricsSettings = getSettingsVector(metricsConfig[module++]);

      for (auto& s : metricsSettings) {
        try {
          //uint64_t val = stoul(s,nullptr,16); //old code
          std::vector<uint64_t> regValList = stringToRegList(s,type);
          for(auto val:regValList)
            regValues[type].push_back(val);
        } catch (...) {
          xrt_core::message::send(severity_level::warning, "XRT", "Error Parsing Metric String.");
        }
      }
    }

    return regValues;
  }

  /****************************************************************************
   * Compile list of registers to read
   ***************************************************************************/
  void AieDebug_EdgeImpl::updateAIEDevice(void* handle)
  {
    xrt_core::message::send(severity_level::debug, "XRT", "!! Calling AIE Debug AieDebug_EdgeImpl::updateAIEDevice");
    if (!xrt_core::config::get_aie_debug())
      return;
    XAie_DevInst* aieDevInst =
      static_cast<XAie_DevInst*>(db->getStaticInfo().getAieDevInst(fetchAieDevInst, handle));

    auto regValues = parseMetrics();

    // Traverse all module types
    int counterId = 0;
    for (int module = 0; module < metadata->getNumModules(); ++module) {
      auto configMetrics = metadata->getConfigMetricsVec(module);
      if (configMetrics.empty())
        continue;

      module_type mod = metadata->getModuleType(module);
      //XAie_ModuleType mod = falModuleTypes[module];
      auto name = moduleTypes.at(mod);

      // List of registers to read for current module
      auto& Regs = regValues[mod];
      if (Regs.empty())
        continue;

      std::stringstream msg;
      msg << "AIE Debug monitoring tiles of type " << name << ":\n";
      for (auto& tileMetric : configMetrics)
        msg << tileMetric.first.col << "," << tileMetric.first.row << " ";
      xrt_core::message::send(severity_level::debug, "XRT", msg.str());

      /*
      Old code
      // Traverse all active tiles for this module type
      for (auto& tileMetric : configMetrics) {
        auto& metricSet  = tileMetric.second;
        auto tile        = tileMetric.first;

        // TODO: replace with gen-specific addresses
        uint32_t offset = 0;

        auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
        debugAddresses[tile].push_back(tileOffset + offset);
      }
      */
     //Rewriting it to populate a map debugTileMap <xdp::tile_type,EdgeReadableTile>
     for (auto& tileMetric : configMetrics) {
        auto& metricSet  = tileMetric.second;
        auto tile        = tileMetric.first;
        auto tileOffset = XAie_GetTileAddr(aieDevInst, tile.row, tile.col);
        //uint32_t offset = 0;
        for (auto& regAddr:Regs) {
        if(debugTileMap.find(tile) == debugTileMap.end()){
          debugTileMap[tile]=std::make_unique<EdgeReadableTile>(tile.row,tile.col);
        }
        //debugTileMap[tile]->insertOffsets(offset,tileOffset + offset)
        debugTileMap[tile]->insertOffsets(regAddr,tileOffset + regAddr);
        }
     }
    }
  }


}  // end namespace xdp
